"""Exercise actual guard failures with private source trees, not the user's app/data."""
from pathlib import Path
import importlib.util
import json
import os
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch
ROOT=Path(__file__).resolve().parents[3]
spec=importlib.util.spec_from_file_location('change_guard',ROOT/'scripts/change-guard.py')
guard=importlib.util.module_from_spec(spec);spec.loader.exec_module(guard)

class ChangeGuardTests(unittest.TestCase):
    def setUp(self):
        base=ROOT/'.local/guard-tests';base.mkdir(parents=True,exist_ok=True)
        self.tmp=tempfile.TemporaryDirectory(prefix='scope-',dir=base)
        self.addCleanup(self.tmp.cleanup)
        self.root=Path(self.tmp.name)/'source';self.root.mkdir()
        self.write('AGENTS.md','<!-- project-status: current -->\nRead [baseline](docs/project-baseline.md).')
        self.write('docs/project-baseline.md','<!-- project-status: current -->\nPB-01: preserve behavior.')
        self.write('docs/behavior-contracts.md','<!-- project-status: current -->\nBC-01 PB-01 [baseline](project-baseline.md).')
        self.write('app/model.py','def execute():\n    return 1\n')
        self.write('app/tests/protected.py','def must_preserve():\n    return True\n')
        self.write('app/tests/integration/test_change_guard.py','import os,sys\nsys.exit(int(os.environ.get("GUARD_FIXTURE_EXIT","0")))\n')
        self.write('CMakeLists.txt','add_test(NAME change_guard COMMAND python fixture.py)\n')
        self.write('.gitignore','.local/\ndata/\n')
        self.policy={'schema_version':1,'baseline_ids':['PB-01'],
          'contracts':[{'id':'BC-01','baseline':'PB-01','tests':['change_guard'],'reference':'app/model.py','symbols':['execute']}],
          'checks':{'change_guard':'python'},'current_documents':['AGENTS.md','docs/project-baseline.md','docs/behavior-contracts.md'],'historical_documents':[],
          'protected_paths':['docs/project-baseline.md','app/tests/protected.py'],'bookkeeping':['docs/STATUS.md','docs/verification/**'],
          'source_inputs':['app/**','scripts/**','CMakeLists.txt','docs/guardrails.json'],
          'areas':[{'id':'docs','paths':['docs/**'],'risk':'low','tests':['change_guard']},{'id':'model','paths':['app/model.py'],'risk':'high','tests':['change_guard']}]}
        self.save_policy()
    def write(self,name,text):
        path=self.root/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_text(text,encoding='utf-8');return path
    def save_policy(self):
        self.write('docs/guardrails.json',json.dumps(self.policy));self.project=guard.Project(self.root)
    def start(self,allowed=None,priority='P2'):
        return self.project.start('task','private fixture request',allowed or ['app/model.py'],priority)
    def change_model(self):
        self.write('app/model.py','def execute():\n    return 2\n')
    def ctest_fixture(self):
        self.policy['checks']['native_case']='ctest';self.policy['areas'][1]['tests'].append('native_case')
        self.write('CMakeLists.txt','add_test(NAME native_case COMMAND fixture)\nadd_test(NAME change_guard COMMAND python fixture.py)\n');self.save_policy()
        self.write('.local/build/windows-release/CMakeCache.txt','CMAKE_HOME_DIRECTORY:INTERNAL='+str(self.root)+'\n')
        exe=self.write('.local/build/windows-release/bin/Release/App.exe','old fixture binary')
        tool=self.write('.local/tool/ctest.exe','fixture tool');self.write('.local/tool/cmake.exe','fixture tool');return tool,exe

    def test_real_current_project_guidance_is_consistent(self):
        self.assertEqual(guard.Project(ROOT).validate(),[])
    def test_preexisting_dirty_changes_are_not_this_task(self):
        self.change_model();self.start();self.assertEqual(self.project.review('task')['changes'],[])
        self.write('app/model.py','def execute():\n    return 3\n')
        self.assertEqual(self.project.review('task')['changes'],[{'path':'app/model.py','action':'modified'}])
    def test_git_snapshot_includes_untracked_without_other_projects(self):
        if not shutil.which('git'):self.skipTest('Git unavailable; exported-tree path covered separately')
        subprocess.run(['git','init','-q',str(self.root)],check=True,capture_output=True)
        self.project=guard.Project(self.root);self.assertTrue(self.project.git)
        self.change_model();self.start();self.write('app/new.py','new code')
        self.assertIn('app/new.py',self.project.review('task')['out_of_scope'])
    def test_added_and_deleted_outside_scope_fail(self):
        self.start();self.write('unrelated/feature.py','other feature')
        (self.root/'app/tests/protected.py').unlink();report=self.project.review('task')
        self.assertIn('unrelated/feature.py',report['out_of_scope']);self.assertIn('app/tests/protected.py',report['out_of_scope'])
        self.assertTrue(any(x['action']=='deleted' for x in report['review_required']))
        with self.assertRaises(guard.GuardError):self.project.verify('task')
    def test_no_git_export_prunes_generated_but_not_tool_named_data(self):
        self.assertFalse(self.project.git)
        self.write('.local/cache.py','generated');self.write('data/private.txt','private');self.write('tools/data/core.py','real tool source')
        snap=self.project.snapshot();self.assertNotIn('data/private.txt',snap);self.assertNotIn('.local/cache.py',snap);self.assertIn('tools/data/core.py',snap)
    def test_task_baseline_cannot_be_overwritten_or_escape(self):
        self.start()
        with self.assertRaises(guard.GuardError):self.start()
        with self.assertRaises(guard.GuardError):self.project.start('../escape','request',['app/**'])
        with self.assertRaises(guard.GuardError):self.project.start('new','request',['../outside/**'])
    def test_low_priority_does_not_reduce_high_risk(self):
        self.start(priority='P3');self.change_model();report=self.project.review('task');self.assertEqual(report['priority'],'P3');self.assertEqual(report['risk'],'high')
    def test_unclassified_source_requires_all_registered_ctests(self):
        self.policy['checks']['other_case']='ctest';self.save_policy();self.start(['other/**']);self.write('other/new.cpp','new source')
        report=self.project.review('task');self.assertEqual(report['risk'],'high');self.assertIn('other_case',report['required_checks'])
    def test_finish_needs_actual_verification(self):
        self.start();self.change_model()
        with self.assertRaisesRegex(guard.GuardError,'validation required'):self.project.finish('task')
        self.project.verify('task');self.project.finish('task');self.assertEqual(self.project.load('task')['status'],'complete')
    def test_changed_source_invalidates_previous_green(self):
        self.start();self.change_model();self.project.verify('task');self.write('app/model.py','def execute():\n    return 3\n');self.assertEqual(self.project.review('task')['evidence'],'stale-source')
        with self.assertRaisesRegex(guard.GuardError,'validation required'):self.project.finish('task')
    def test_later_failure_cannot_reuse_earlier_green_same_source(self):
        self.start();self.change_model();self.project.verify('task')
        with patch.dict(os.environ,{'GUARD_FIXTURE_EXIT':'1'}):
            with self.assertRaises(guard.GuardError):self.project.verify('task')
        with self.assertRaisesRegex(guard.GuardError,'validation required'):self.project.finish('task')
    def test_protected_policy_edit_requires_review_note(self):
        self.start(['docs/project-baseline.md']);self.write('docs/project-baseline.md','<!-- project-status: current -->\nPB-01: preserve behavior with clarification.')
        self.project.verify('task')
        with self.assertRaisesRegex(guard.GuardError,'review note'):self.project.finish('task')
        self.project.finish('task','Fixture request authorized clarification; original behavior preserved')
    def test_removing_protected_behavior_symbol_fails(self):
        self.start();self.write('app/model.py','def different():\n    return 2\n')
        self.assertTrue(any('protected behavior symbol' in x for x in self.project.validate()))
        with self.assertRaises(guard.GuardError):self.project.verify('task')
    def test_no_collected_ctests_is_not_success(self):
        tool,_=self.ctest_fixture();self.start();self.change_model()
        def mocked(command,**kwargs):return subprocess.CompletedProcess(command,0,stdout=json.dumps({'tests':[]}),stderr='')
        with patch.object(guard.subprocess,'run',side_effect=mocked) as run:
            with self.assertRaisesRegex(guard.GuardError,'Expected tests not collected'):self.project.verify('task',ctest=str(tool))
        self.assertTrue(any('--build' in call.args[0] for call in run.call_args_list))
        with self.assertRaises(guard.GuardError):self.project.finish('task')
    def test_wrong_build_source_rejected_before_native_tests(self):
        tool,_=self.ctest_fixture();self.write('.local/build/windows-release/CMakeCache.txt','CMAKE_HOME_DIRECTORY:INTERNAL='+str(self.root.parent)+'\n');self.start();self.change_model()
        with self.assertRaisesRegex(guard.GuardError,'another source tree'):self.project.verify('task',ctest=str(tool))
    def test_native_build_runs_before_tests_and_artifact_changes_invalidate(self):
        tool,exe=self.ctest_fixture();self.start();self.change_model()
        def mocked(command,**kwargs):return subprocess.CompletedProcess(command,0,stdout=json.dumps({'tests':[{'name':'native_case'},{'name':'change_guard'}]}),stderr='')
        with patch.object(guard.subprocess,'run',side_effect=mocked) as run:self.project.verify('task',ctest=str(tool))
        commands=[call.args[0] for call in run.call_args_list];self.assertLess(next(i for i,c in enumerate(commands) if '--build' in c),next(i for i,c in enumerate(commands) if '--output-on-failure' in c))
        exe.write_bytes(b'changed binary, identical source')
        with self.assertRaisesRegex(guard.GuardError,'Build outputs changed'):self.project.finish('task')
    def test_external_directory_link_is_inventoried_not_followed(self):
        outside=Path(self.tmp.name)/'external';outside.mkdir();(outside/'secret.txt').write_text('not project content')
        try:os.symlink(outside,self.root/'external-link',target_is_directory=True)
        except OSError as error:self.skipTest('OS did not allow private symlink fixture: '+str(error))
        snap=self.project.snapshot();self.assertTrue(snap['external-link'].startswith('link:'));self.assertNotIn('external-link/secret.txt',snap)
        (self.root/'external-link').unlink();self.start(['external-link']);os.symlink(outside,self.root/'external-link',target_is_directory=True)
        self.assertIn('external-link',self.project.review('task')['unsafe_links'])
    def test_changing_policy_cannot_silently_shrink_original_scope_or_routing(self):
        self.start(['docs/guardrails.json']);self.policy['bookkeeping']=['**'];self.policy['protected_paths']=[];self.policy['areas']=[];self.save_policy();self.change_model()
        report=self.project.review('task');self.assertIn('app/model.py',report['out_of_scope']);self.assertEqual(report['risk'],'high');self.assertTrue(any(x['path']=='docs/guardrails.json' for x in report['review_required']))
    def test_runtime_inputs_cannot_be_hidden_by_bad_bookkeeping_config(self):
        self.policy['source_inputs']=[];self.policy['bookkeeping']=['**'];self.save_policy();before=self.project.code_digest();self.change_model();self.assertNotEqual(before,self.project.code_digest())
    def test_original_test_route_survives_area_removed_from_current_policy(self):
        self.policy['checks']['native_case']='ctest';self.policy['areas'][1]['tests'].append('native_case');self.save_policy();self.start(['app/model.py','docs/guardrails.json']);self.policy['areas']=[];self.save_policy();self.change_model()
        self.assertIn('native_case',self.project.review('task')['required_checks'])
    def test_status_bookkeeping_does_not_stale_business_evidence(self):
        self.start();self.change_model();self.project.verify('task');self.write('docs/STATUS.md','Updated actual status')
        self.project.finish('task')

if __name__=='__main__':unittest.main(verbosity=2)
