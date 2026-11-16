"""Task-local scope and verification checks. No rollback, publishing or arbitrary shell execution."""
from __future__ import annotations
import argparse
import datetime as dt
import fnmatch
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import stat
import subprocess
import sys
import time
from urllib.parse import unquote

SOURCE = Path(__file__).resolve().parents[1]
EXCLUDED = {'.git', '.local', 'node_modules', '__pycache__', '.venv', 'venv'}
PRESETS = ('windows-release', 'windows-check', 'windows-complete', 'windows-dev')
RISK = {'low': 0, 'medium': 1, 'high': 2}
CORE_PROTECTED = ('AGENTS.md','README.md','@repo/AGENTS.md','@repo/README.md','docs/project-baseline.md','docs/behavior-contracts.md','docs/change-policy.md','docs/guardrails.json','scripts/change-guard.py','CMakeLists.txt','scripts/dev.ps1')
CORE_INPUTS = ('app/**','tools/**','shared/**','scripts/**','examples/**','CMakeLists.txt','CMakePresets.json')

class GuardError(Exception):
    pass

def now():
    return dt.datetime.now(dt.timezone.utc).isoformat()

def digest(data):
    return hashlib.sha256(data).hexdigest()

def read_json(path):
    return json.loads(Path(path).read_text(encoding='utf-8-sig'))

def save_json(path, value):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    stage = path.with_suffix(path.suffix + '.tmp')
    stage.write_text(json.dumps(value, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    os.replace(stage, path)

def match(name, patterns):
    return any(fnmatch.fnmatchcase(name, pattern) for pattern in patterns)

def is_test(name):
    return bool(re.search(r'(^|/)(tests|test_[^/]+)', name))

class Project:
    def __init__(self, root=SOURCE):
        self.root = Path(root).resolve()
        self.repo = self.root
        self.git = False
        try:
            candidate = Path(subprocess.check_output(['git', 'rev-parse', '--show-toplevel'], cwd=self.root, stderr=subprocess.DEVNULL, text=True).strip()).resolve()
            # Never scan a containing multi-project workspace.
            if candidate in (self.root, self.root.parent) and (candidate == self.root or self.root.name == 'source'):
                self.repo = candidate
                self.git = True
        except (OSError, subprocess.SubprocessError):
            pass
        self.policy = read_json(self.root / 'docs/guardrails.json')

    def path(self, logical):
        base = self.repo if logical.startswith('@repo/') else self.root
        name = logical[6:] if logical.startswith('@repo/') else logical
        parts = PurePosixPath(name).parts
        if not name or '\\' in name or ':' in name or name.startswith('/') or '..' in parts:
            raise GuardError('Unsafe project-relative path: ' + logical)
        value = base / name
        if not value.resolve().is_relative_to(base):
            raise GuardError('Path resolves outside project: ' + logical)
        return value

    def snapshot(self):
        names = None
        try:
            if not self.git:
                raise OSError('No project-local Git root')
            raw = subprocess.check_output(['git', 'ls-files', '--cached', '--others', '--exclude-standard', '-z'], cwd=self.repo, stderr=subprocess.DEVNULL)
            names = [(self.repo / x) for x in raw.decode('utf-8').split('\0') if x]
        except (OSError, subprocess.SubprocessError):
            pass
        if names is None:
            names = []
            for folder, dirs, files in os.walk(self.root, followlinks=False):
                kept = []
                for name in dirs:
                    child = Path(folder)/name
                    if name in EXCLUDED or (Path(folder)==self.root and name=='data'):
                        continue
                    if child.is_symlink() or getattr(child.lstat(), 'st_file_attributes', 0) & 0x400:
                        names.append(child)  # Inventory the link itself; never walk its target.
                    else:
                        kept.append(name)
                dirs[:] = kept
                names.extend(Path(folder)/n for n in files if not n.endswith(('.pyc', '.pyo')))
        result = {}
        for path in sorted(set(names)):
            relative = path.relative_to(self.repo)
            # Explicitly tracked README sentinels in ignored directories remain stable, but generated files do not enter snapshots.
            logical = path.relative_to(self.root).as_posix() if path.is_relative_to(self.root) else '@repo/' + relative.as_posix()
            try:
                info = path.lstat()
                if stat.S_ISLNK(info.st_mode) or getattr(info, 'st_file_attributes', 0) & 0x400:
                    result[logical] = 'link:' + digest(os.readlink(path).encode('utf-8'))
                elif stat.S_ISREG(info.st_mode):
                    result[logical] = digest(path.read_bytes())
            except FileNotFoundError:
                continue
        return result

    def code_digest(self):
        current = self.snapshot()
        protected = list(CORE_PROTECTED)+self.policy['protected_paths']
        relevant = {}
        for name, value in current.items():
            critical = match(name, protected)
            # Only source-bearing documents/assets and core guidance bind business evidence.
            # Release receipts remain scope-checked but do not create an endless verification loop.
            if name.startswith('@repo/releases/') and not critical:
                continue
            if self.bookkeeping(name) and not critical:
                continue
            if critical or match(name, list(CORE_INPUTS)+self.policy['source_inputs']) or not name.endswith('.md'):
                relevant[name] = value
        return digest(json.dumps(relevant, sort_keys=True).encode('utf-8'))

    def bookkeeping(self, name):
        # A bad config must never turn runtime code into automatically permitted bookkeeping.
        return name.startswith('docs/') and match(name, self.policy['bookkeeping'])

    def validate(self):
        errors = []
        def require(condition, message):
            if not condition:
                errors.append(message)
        require(self.policy.get('schema_version') == 1, 'Unsupported guardrail schema')
        baseline = (self.root/'docs/project-baseline.md').read_text(encoding='utf-8')
        contracts = (self.root/'docs/behavior-contracts.md').read_text(encoding='utf-8')
        require(set(re.findall(r'\bPB-\d\d\b', baseline)) == set(self.policy['baseline_ids']), 'PB baseline IDs differ from index')
        require(set(re.findall(r'\bBC-\d\d\b', contracts)) == {x['id'] for x in self.policy['contracts']}, 'BC behavior IDs differ from index')
        cmake = (self.root/'CMakeLists.txt').read_text(encoding='utf-8')
        collected = set(re.findall(r'add_test\s*\(\s*NAME\s+([\w-]+)', cmake))
        for name, kind in self.policy['checks'].items():
            require(bool(re.fullmatch(r'[a-z][a-z0-9_]*', name)), 'Unsafe check ID: ' + name)
            require(kind in ('python', 'ctest', 'python_frozen'), 'Unsupported check kind: ' + name)
            if kind == 'ctest':
                require(name in collected, 'CTest not registered: ' + name)
        for item in self.policy['contracts']:
            require(item['baseline'] in self.policy['baseline_ids'], 'Unknown PB for ' + item['id'])
            reference = self.path(item['reference'])
            require(reference.is_file(), 'Missing behavior reference: ' + item['reference'])
            for check in item['tests']:
                require(check in self.policy['checks'], 'Unknown check for ' + item['id'] + ': ' + check)
            if reference.is_file():
                content = reference.read_text(encoding='utf-8')
                for symbol in item.get('symbols', []):
                    require(bool(re.search(r'\b(?:def|void)\s+' + re.escape(symbol) + r'\s*\(', content)), 'Missing protected behavior symbol: ' + item['id'] + '/' + symbol)
        for area in self.policy['areas']:
            require(area['risk'] in RISK, 'Unknown risk: ' + area['id'])
            require(set(area['tests']) <= self.policy['checks'].keys(), 'Unknown area check: ' + area['id'])
        for name in self.policy['current_documents']:
            path = self.path(name)
            if not path.is_file():
                errors.append('Missing current document: ' + name)
                continue
            content = path.read_text(encoding='utf-8')
            require('<!-- project-status: current -->' in content, 'No current marker: ' + name)
            if name != 'docs/project-baseline.md':
                require('project-baseline.md' in content, 'No current baseline route: ' + name)
            for target in re.findall(r'(?<!!)\[[^\]\n]+\]\(([^)\n]+)\)', content):
                target = target.strip().strip('<>').split('#')[0]
                if not target or re.match(r'^[a-zA-Z][\w+.-]*:', target) or any(c in target for c in '{}<>'):
                    continue
                target = unquote(target)
                resolved = (path.parent/target).resolve()
                require(resolved.is_relative_to(self.repo) and resolved.exists(), 'Broken/outside current link: ' + name + ' -> ' + target)
        for name in self.policy.get('historical_documents', []):
            path = self.path(name)
            require(path.is_file() and '<!-- project-status: historical -->' in path.read_text(encoding='utf-8'), 'Historical scope missing: ' + name)
        if self.root != self.repo:
            for name in ('AGENTS.md', 'README.md'):
                require('project-baseline.md' in (self.repo/name).read_text(encoding='utf-8'), 'Root handoff misses current baseline: ' + name)
        return errors

    def task_path(self, task):
        if not re.fullmatch(r'[a-z][a-z0-9-]{0,63}', task):
            raise GuardError('Task ID must be a short lowercase slug')
        path = self.root/'.local/change-guard'/task/'record.json'
        if not path.resolve().is_relative_to(self.root):
            raise GuardError('Task directory resolves outside source project')
        return path

    def load(self, task):
        path = self.task_path(task)
        if not path.is_file():
            raise GuardError('No task baseline. Start a new task before editing: ' + task)
        return read_json(path)

    def start(self, task, request, allowed, priority='P2', initial=None):
        path = self.task_path(task)
        if path.exists():
            raise GuardError('Refusing to overwrite an existing task baseline')
        if not request.strip() or not allowed:
            raise GuardError('Request and explicit allowed paths are required')
        for pattern in allowed:
            self.path(pattern.replace('*', 'glob').replace('?', 'q').replace('[', '').replace(']', ''))
        record = {'schema_version': 1, 'task': task, 'request': request, 'priority': priority,
                  'created_at': now(), 'status': 'active', 'allowed': allowed,
                  'before': initial if initial is not None else self.snapshot(), 'policy_before': json.loads(json.dumps(self.policy)), 'validations': []}
        save_json(path, record)
        return record

    def review(self, task):
        record = self.load(task)
        current = self.snapshot()
        changes = [{'path': n, 'action': 'added' if n not in record['before'] else 'deleted' if n not in current else 'modified'}
                   for n in sorted(set(current)|set(record['before'])) if current.get(n) != record['before'].get(n)]
        original = record.get('policy_before', self.policy)  # GP-1 bootstrap predates the guard itself.
        protected = list(CORE_PROTECTED)+self.policy['protected_paths']+original['protected_paths']
        def bookkeeping(name):
            return self.bookkeeping(name) and match(name, original['bookkeeping'])
        outside = [x['path'] for x in changes if not bookkeeping(x['path']) and not match(x['path'], record['allowed'])]
        unsafe = []
        for item in changes:
            if current.get(item['path'], '').startswith('link:'):
                try:
                    self.path(item['path'])
                except GuardError:
                    unsafe.append(item['path'])
        outside.extend(n for n in unsafe if n not in outside)
        sensitive = [x for x in changes if match(x['path'], protected) or is_test(x['path'])]
        required, areas, unknown, retired = {'change_guard'}, [], [], set()
        risk = 'high' if sensitive else 'low'
        for change in changes:
            name = change['path']
            if bookkeeping(name):
                continue
            found = False
            for area in self.policy['areas']+original['areas']:
                if match(name, area['paths']):
                    found = True
                    required.update(n for n in area['tests'] if n in self.policy['checks'])
                    retired.update(n for n in area['tests'] if n not in self.policy['checks'])
                    areas.append(area['id'])
                    risk = max(risk, area['risk'], key=RISK.get)
            if not found:
                unknown.append(name)
        if unknown:
            risk = 'high'
            required.update(n for n, kind in self.policy['checks'].items() if kind == 'ctest')
        latest = record['validations'][-1] if record['validations'] else None
        evidence = 'pending'
        if latest:
            evidence = 'stale-source' if latest['digest'] != self.code_digest() else 'failed' if latest.get('error') else 'current'
            if evidence=='current' and latest.get('artifacts') is not None and latest['artifacts']!=self.build_digest(latest['preset']):
                evidence='stale-build'
        return {'task': task, 'priority': record['priority'], 'risk': risk, 'evidence': evidence, 'changes': changes,
                'out_of_scope': outside, 'unsafe_links': unsafe, 'review_required': sensitive, 'unknown_paths': unknown,
                'areas': sorted(set(areas)), 'retired_checks_for_review': sorted(retired), 'required_checks': sorted(required)}

    def locate_ctest(self, explicit=None):
        if explicit:
            return explicit
        found = shutil.which('ctest')
        if found:
            return found
        finder = Path(os.environ.get('ProgramFiles(x86)', 'C:/Program Files (x86)'))/'Microsoft Visual Studio/Installer/vswhere.exe'
        if finder.is_file():
            vs = subprocess.check_output([str(finder), '-latest', '-products', '*', '-requires', 'Microsoft.VisualStudio.Component.VC.Tools.x86.x64', '-property', 'installationPath'], text=True).strip()
            found = Path(vs)/'Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/ctest.exe'
            if found.is_file():
                return str(found)
        raise GuardError('CTest missing. Restore the project toolchain; validation remains pending')

    def build_digest(self, preset):
        configuration = 'Release' if preset == 'windows-release' else 'Debug'
        folder = self.root/'.local/build'/preset/'bin'/configuration
        if not folder.resolve().is_relative_to(self.root):
            raise GuardError('Build outputs resolve outside source project')
        entries = {p.name: digest(p.read_bytes()) for p in sorted(folder.glob('*')) if p.is_file() and p.suffix.lower() in ('.exe','.dll')}
        return digest(json.dumps(entries, sort_keys=True).encode('utf-8'))

    def verify(self, task, preset='windows-release', ctest=None):
        errors = self.validate()
        review = self.review(task)
        if errors or review['out_of_scope']:
            raise GuardError('Validation/scope failed: ' + json.dumps(errors + review['out_of_scope']))
        if preset not in PRESETS:
            raise GuardError('Unsupported build preset')
        record = self.load(task)
        fingerprint = self.code_digest()
        folder = self.task_path(task).parent
        ctests = [n for n in review['required_checks'] if self.policy['checks'][n] == 'ctest']
        if ctests:
            ctests = sorted(set(ctests)|{'change_guard'})  # Also exercise the CTest integration, not only direct Python.
        results = []
        def execute(names, command):
            log = folder/(str(time.time_ns())+'-'+names[0]+'.log')
            started = time.monotonic()
            try:
                with log.open('wb') as stream:
                    result = subprocess.run(command, cwd=self.root, stdout=stream, stderr=stream, timeout=900)
                status = result.returncode
            except (OSError, subprocess.TimeoutExpired) as error:
                status = -1
                with log.open('ab') as stream:
                    stream.write(str(error).encode('utf-8'))
            results.append({'checks': names, 'returncode': status, 'command': command, 'log': str(log.relative_to(self.root)), 'seconds': round(time.monotonic()-started, 3)})
            if status:
                raise GuardError('Required check failed; see ' + str(log.relative_to(self.root)))
        failure = None
        artifacts = None
        try:
            execute(['change_guard'], [sys.executable, '-B', str(self.root/'app/tests/integration/test_change_guard.py')])
            if ctests:
                tool = Path(self.locate_ctest(ctest))
                cmake = tool.with_name('cmake.exe' if tool.suffix.lower()=='.exe' else 'cmake')
                if not cmake.is_file():
                    raise GuardError('Use the CTest installed beside the project CMake')
                build = self.root/'.local/build'/preset
                cache = (build/'CMakeCache.txt').read_text(encoding='utf-8')
                home = re.search(r'^CMAKE_HOME_DIRECTORY:INTERNAL=(.+)$', cache, re.M)
                if not home or Path(home[1].strip()).resolve() != self.root:
                    raise GuardError('Build cache belongs to another source tree. Configure this project first')
                configuration = 'Release' if preset == 'windows-release' else 'Debug'
                # Source hashes alone cannot prove that CTest used newly compiled binaries.
                execute(['build'], [str(cmake), '--build', str(build), '--config', configuration])
                common = [str(tool), '--test-dir', str(build), '-C', configuration]
                built = self.build_digest(preset)
                collection = subprocess.run(common + ['--show-only=json-v1'], cwd=self.root, capture_output=True, text=True, timeout=30)
                if collection.returncode:
                    raise GuardError('CTest discovery failed; restore/configure the selected preset')
                found = {x['name'] for x in json.loads(collection.stdout).get('tests', [])}
                missing = set(ctests)-found
                if missing:
                    raise GuardError('Expected tests not collected (zero tests is not success): ' + ', '.join(sorted(missing)))
                execute(ctests, common + ['--output-on-failure', '-R', '^('+'|'.join(re.escape(n) for n in ctests)+')$'])
                artifacts = self.build_digest(preset)
                if artifacts != built:
                    raise GuardError('Build outputs changed while tests ran; validation is not stable')
            if 'frozen_frontends' in review['required_checks']:
                execute(['frozen_frontends'], [sys.executable, '-B', str(self.root/'scripts/verify-frozen-frontends.py'), '--freeze'])
        except (GuardError, OSError, ValueError, subprocess.SubprocessError) as error:
            failure = str(error)
        record['validations'].append({'at': now(), 'digest': fingerprint, 'preset': preset, 'artifacts': artifacts, 'results': results, 'error': failure})
        save_json(self.task_path(task), record)
        if failure:
            raise GuardError(failure)
        if self.code_digest() != fingerprint:
            raise GuardError('Code changed during validation. Results are stale; re-run after edits settle')
        return results

    def finish(self, task, review_note=None):
        errors = self.validate()
        report = self.review(task)
        record = self.load(task)
        if errors or report['out_of_scope']:
            raise GuardError('Cannot finish: ' + json.dumps(errors+report['out_of_scope']))
        note = review_note or record.get('review_note', '')
        if report['review_required'] and not note.strip():
            raise GuardError('Protected rules/tests changed. Supply a review note explaining preservation/replacement and the current request basis')
        current = self.code_digest()
        # A later failed attempt supersedes an earlier green run of the same source.
        run = next((v for v in reversed(record['validations']) if v['digest']==current),None)
        successful = set()
        if run and not run.get('error'):
            if run.get('artifacts') is not None and run['artifacts'] != self.build_digest(run['preset']):
                raise GuardError('Build outputs changed after validation; re-run current binaries')
            successful = {n for item in run['results'] if item['returncode']==0 for n in item['checks']}
        missing = set(report['required_checks']) - successful
        if missing:
            raise GuardError('Missing/current-code validation required: ' + ', '.join(sorted(missing)))
        record.update(status='complete', completed_at=now(), review_note=note, final_review=report, final_digest=current)
        save_json(self.task_path(task), record)
        return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=SOURCE, help='Source root (private fixtures/source exports supported)')
    commands = parser.add_subparsers(dest='command', required=True)
    commands.add_parser('validate')
    start = commands.add_parser('start')
    start.add_argument('task'); start.add_argument('--request', required=True); start.add_argument('--allow', action='append', required=True)
    start.add_argument('--priority', choices=['P0','P1','P2','P3'], default='P2')
    for name in ('review','verify','finish'):
        child = commands.add_parser(name);child.add_argument('task')
        if name == 'verify':
            child.add_argument('--preset', choices=PRESETS, default='windows-release');child.add_argument('--ctest')
        if name == 'finish':
            child.add_argument('--review-note')
    args = parser.parse_args()
    try:
        project = Project(args.root)
        if args.command == 'validate':
            errors = project.validate()
            if errors:
                raise GuardError('\n'.join(errors))
            print('Current project guidance and behavior index: PASS')
        elif args.command == 'start':
            project.start(args.task, args.request, args.allow, args.priority)
            print('Task baseline captured: ' + args.task)
        elif args.command == 'review':
            report = project.review(args.task);print(json.dumps(report, indent=2))
            if report['out_of_scope']:
                return 2
        elif args.command == 'verify':
            print(json.dumps(project.verify(args.task, args.preset, args.ctest), indent=2))
        else:
            print(json.dumps(project.finish(args.task, args.review_note), indent=2))
        return 0
    except (GuardError, OSError, ValueError, KeyError) as error:
        print('GUARD CHECK FAILED: ' + str(error), file=sys.stderr)
        return 2

if __name__ == '__main__':
    raise SystemExit(main())
