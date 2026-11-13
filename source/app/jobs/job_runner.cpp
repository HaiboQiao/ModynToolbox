#include "job_runner.h"
#include "catalog/package_manager.h"
#include "core/files.h"
#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QProcess>
#include <QProcessEnvironment>
#include <QSaveFile>

#include <cmath>
#define NOMINMAX
#include <windows.h>
#include <shlobj.h>
JobRunner::JobRunner(JobRequest value, QObject *parent)
    : QThread(parent), request(std::move(value)) {
}
void JobRunner::run() {
    if (request.kind == JobKind::FileExport) {
        const auto result = runExport();
        emit completed(request.jobId, result, result.value("status").toString());
        return;
    }

    QJsonObject result;
    bool externalMayRun = false;
    QString error =
        PackageManager(QFileInfo(request.tool.packageRoot).absolutePath()).verify(request.tool);
    if (!error.isEmpty()) {
        emit completed(request.jobId, {{"message", error}, {"status", "failed"}}, "failed");
        return;
    }
    const auto group = CreateJobObjectW(nullptr, nullptr);
    if (!group) {
        emit completed(request.jobId, {{"message", "无法创建受控进程组"}}, "failed");
        return;
    }
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE |
                                              JOB_OBJECT_LIMIT_ACTIVE_PROCESS |
                                              JOB_OBJECT_LIMIT_JOB_MEMORY;
    limits.BasicLimitInformation.ActiveProcessLimit = request.tool.limits.value("max_processes").toInt(16);
    limits.JobMemoryLimit =
        static_cast<SIZE_T>(request.tool.limits.value("memory_mb").toInt(512)) * 1024 * 1024;
    if (!SetInformationJobObject(group, JobObjectExtendedLimitInformation, &limits,
                                 sizeof(limits))) {
        CloseHandle(group);
        emit completed(request.jobId, {{"message", "无法应用进程资源限制"}}, "failed");
        return;
    }
    JOBOBJECT_CPU_RATE_CONTROL_INFORMATION cpu{};
    cpu.ControlFlags = JOB_OBJECT_CPU_RATE_CONTROL_ENABLE | JOB_OBJECT_CPU_RATE_CONTROL_HARD_CAP;
    cpu.CpuRate = 8000;
    if (!SetInformationJobObject(group, JobObjectCpuRateControlInformation, &cpu, sizeof(cpu))) {
        CloseHandle(group);
        emit completed(request.jobId, {{"message", "无法应用 CPU 限制"}}, "failed");
        return;
    }
    QProcess process;
    process.setProgram(request.bridge);
    process.setWorkingDirectory(request.jobRoot);
    QProcessEnvironment environment;
    for (const auto &key : {"SystemRoot", "WINDIR", "SystemDrive", "COMSPEC", "USERPROFILE", "USERNAME", "USERDOMAIN", "HOMEDRIVE", "HOMEPATH", "APPDATA", "LOCALAPPDATA", "ProgramFiles", "ProgramFiles(x86)", "ProgramData"})
        if (qEnvironmentVariableIsSet(key))
            environment.insert(key, qEnvironmentVariable(key));
    auto userFolder = [&](const QString &key, REFKNOWNFOLDERID folder) {
        if (!environment.value(key).isEmpty()) return;
        PWSTR path=nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(folder, KF_FLAG_DONT_VERIFY, nullptr, &path)))
            environment.insert(key, QString::fromWCharArray(path));
        CoTaskMemFree(path);
    };
    userFolder("USERPROFILE", FOLDERID_Profile);
    userFolder("LOCALAPPDATA", FOLDERID_LocalAppData);
    userFolder("APPDATA", FOLDERID_RoamingAppData);
    if (environment.value("USERNAME").isEmpty()) {
        wchar_t name[257]{}; DWORD size=257;
        if (GetUserNameW(name, &size)) environment.insert("USERNAME", QString::fromWCharArray(name));
    }
    environment.insert("PATH", QFileInfo(request.bridge).absolutePath());
    environment.insert("TEMP", request.jobRoot + "/temp");
    environment.insert("TMP", request.jobRoot + "/temp");
    environment.insert("PYTHONNOUSERSITE", "1");
    environment.insert("PYTHONDONTWRITEBYTECODE", "1");
    environment.insert("PYTHONUTF8", "1");
    QDir().mkpath(request.jobRoot + "/temp");
    process.setProcessEnvironment(environment);
    process.setCreateProcessArgumentsModifier(
        [](QProcess::CreateProcessArguments *arguments) { arguments->flags |= CREATE_NO_WINDOW; });
    process.start();
    if (!process.waitForStarted(5000)) {
        error = QStringLiteral("启动桥无法运行 · ") + process.errorString();
    } else {
        const auto handle = OpenProcess(PROCESS_SET_QUOTA | PROCESS_TERMINATE, FALSE,
                                        static_cast<DWORD>(process.processId()));
        if (!handle || !AssignProcessToJobObject(group, handle))
            error = QStringLiteral("进程树归属失败，工具未启动");
        if (handle)
            CloseHandle(handle);
    }
    if (error.isEmpty()) {
        QStringList arguments = request.tool.entryArguments;
        for (auto &argument : arguments)
            argument.replace("{package}", request.tool.packageRoot)
                .replace("{job}", request.jobRoot)
                .replace("{data}", request.dataRoot);
        QJsonArray args;
        for (const auto &argument : arguments)
            args.append(argument);
        const QJsonObject launch{
            {"program", Files::child(request.tool.packageRoot, request.tool.entryProgram, true)},
            {"args", args},
            {"cwd", request.jobRoot}};
        externalMayRun = true;
        process.write(QJsonDocument(launch).toJson(QJsonDocument::Compact) + "\n");
        const QJsonObject run{{"protocol", 1},
                              {"kind", "run"},
                              {"tool_id", request.tool.id},
                              {"session_id", request.sessionId},
                              {"job_id", request.jobId},
                              {"revision", request.revision},
                              {"action", request.action.id},
                              {"job_root", request.jobRoot},
                              {"data_root", request.dataRoot},
                              {"dynamic_resources", request.action.dynamicResources}};
        process.write(QJsonDocument(run).toJson(QJsonDocument::Compact) + "\n");
    }
    QByteArray buffer;
    qint64 stdoutBytes = 0, stderrBytes = 0;
    int sequence = 0;
    bool ready = false, terminal = false, cancelSent = false, critical = false, reserving = false,
         grantSent = false;
    QElapsedTimer clock, updates;
    clock.start();
    updates.start();
    auto read = [&] {
        const auto bytes = process.readAllStandardOutput();
        stdoutBytes += bytes.size();
        stderrBytes += process.readAllStandardError().size();
        if (stdoutBytes > 4 * 1024 * 1024 || stderrBytes > 2 * 1024 * 1024) {
            error = QStringLiteral("工作进程输出超过限额");
            return;
        }
        buffer.append(bytes);
        int newline = 0;
        while ((newline = buffer.indexOf('\n')) >= 0) {
            if (newline > 65536) {
                error = QStringLiteral("消息行超过 64 KiB");
                return;
            }
            const auto line = buffer.left(newline);
            buffer.remove(0, newline + 1);
            QJsonParseError parse;
            const auto document = QJsonDocument::fromJson(line, &parse);
            const auto event = document.object();
            if (parse.error != QJsonParseError::NoError || !document.isObject() ||
                event.value("protocol") != QJsonValue(1) ||
                event.value("job_id").toString() != request.jobId ||
                event.value("session_id").toString() != request.sessionId ||
                event.value("tool_id").toString() != request.tool.id ||
                event.value("revision").toInt(-1) != request.revision ||
                event.value("seq").toInt() <= sequence || terminal) {
                error = QStringLiteral("工作进程消息标识、顺序或格式无效");
                return;
            }
            sequence = event.value("seq").toInt();
            const auto kind = event.value("kind").toString();
            if (kind == "ready" && !ready)
                ready = true;
            else if (kind == "progress" && ready) {
                critical = event.value("phase").toString() == "commit" ||
                           !event.value("cancellable").toBool(true);
                if (updates.elapsed() > 100 || critical) {
                    emit progress(request.jobId, event);
                    updates.restart();
                }
            } else if (kind == "reserve" && ready && request.action.dynamicResources &&
                       !reserving && !critical) {
                if (event.value("resources_ref").toString() != "resources.json") {
                    error = QStringLiteral("资源引用无效");
                    return;
                }
                const auto path = Files::child(request.jobRoot, "resources.json", true);
                const auto resources =
                    Files::readObject(path, 8 * 1024 * 1024, &error).value("paths").toArray();
                if (!error.isEmpty() || resources.size() > 8000) {
                    error = QStringLiteral("资源预约超限");
                    return;
                }
                QStringList paths;
                for (const auto &value : resources) {
                    if (!value.isString() || value.toString().size() > 32760 ||
                        !QDir::isAbsolutePath(value.toString())) {
                        error = QStringLiteral("资源路径无效");
                        return;
                    }
                    paths.append(value.toString());
                }
                reserving = true;
                emit reservation(request.jobId, Files::resourceKeys(paths));
            } else if (kind == "result" && ready) {
                if (event.value("result_ref").toString() != "result.json") {
                    error = QStringLiteral("结果引用无效");
                    return;
                }
                terminal = true;
            } else {
                error = QStringLiteral("未知消息或启动握手缺失");
                return;
            }
        }
        if (buffer.size() > 65536)
            error = QStringLiteral("未结束消息超过限额");
    };
    while (error.isEmpty() && process.state() != QProcess::NotRunning) {
        process.waitForReadyRead(20);
        read();
        if (reserving && granted.load() && !grantSent) {
            process.write(
                QJsonDocument(QJsonObject{{"kind", "resources_granted"}, {"job_id", request.jobId}})
                    .toJson(QJsonDocument::Compact) +
                "\n");
            grantSent = true;
        }
        if (cancellation.load() && request.action.cancellable && !critical && !cancelSent) {
            const QJsonObject cancel{
                {"protocol", 1}, {"kind", "cancel"}, {"job_id", request.jobId}};
            process.write(QJsonDocument(cancel).toJson(QJsonDocument::Compact) + "\n");
            cancelSent = true;
        }
        if (clock.elapsed() > request.action.timeoutSeconds * 1000LL && !critical)
            error = QStringLiteral("作业超过执行时限，已中断；请检查已完成输出");
    }
    if (!error.isEmpty()) {
        TerminateJobObject(group, 1);
        process.kill();
        process.waitForFinished(5000);
    } else {
        read();
        if (!buffer.trimmed().isEmpty() || !ready || !terminal ||
            process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0)
            error =
                QStringLiteral(
                    "工作进程未正常确认结果，作业中断 · exit=%1 ready=%2 result=%3 stderr_bytes=%4")
                    .arg(process.exitCode())
                    .arg(ready)
                    .arg(terminal)
                    .arg(stderrBytes);
    }
    CloseHandle(group);
    if (error.isEmpty()) {
        const auto path = Files::child(request.jobRoot, "result.json", true);
        result = Files::readObject(path, 32 * 1024 * 1024, &error);
        const auto text = result.value("text").toString();
        const auto rows = result.value("rows").toArray();
        const auto columns = result.value("columns").toArray();
        if (text.toUtf8().size() > 20 * 1024 * 1024 || rows.size() > 2000 || columns.size() > 20 ||
            result.value("message").toString().size() > 2048)
            error = QStringLiteral("结果数据超限");
        for (const auto &row : rows) {
            if (!row.isObject() || row.toObject().size() > 24) {
                error = QStringLiteral("结果行结构无效");
                break;
            }
            for (const auto &cell : row.toObject())
                if (cell.isArray() || cell.isObject() || cell.toString().size() > 4096)
                    error = QStringLiteral("结果单元格无效或超限");
        }
        if (result.contains("preview_ref")) {
            const auto preview =
                Files::child(request.jobRoot, result.value("preview_ref").toString(), true);
            if (preview.isEmpty() || QFileInfo(preview).size() > 1024 * 1024)
                error = QStringLiteral("缩略图引用无效");
            else
                result["preview_path"] = preview;
        }
    }
    for (const auto &key : {"completed", "total", "failed"})
        if (result.contains(key)) {
            const auto number = result.value(key).toDouble(-1);
            if (!result.value(key).isDouble() || number < 0 || number > 2147483647 ||
                std::floor(number) != number)
                error = QStringLiteral("结果计数无效或超限");
        }
    if (result.contains("rows") && !result.value("rows").isArray())
        error = QStringLiteral("结果行类型无效");
    if (result.contains("columns") && !result.value("columns").isArray())
        error = QStringLiteral("结果列类型无效");
    if (result.contains("context") &&
        (!result.value("context").isObject() ||
         QJsonDocument(result.value("context").toObject()).toJson(QJsonDocument::Compact).size() >
             65536))
        error = QStringLiteral("结果上下文无效或超过 64 KiB");
    if (result.contains("artifacts")) {
        const auto artifacts = result.value("artifacts").toArray();
        if (!result.value("artifacts").isArray() || artifacts.size() > 2000)
            error = QStringLiteral("输出引用无效或超限");
        for (const auto &path : artifacts)
            if (!path.isString() || path.toString().size() > 32760 ||
                !QDir::isAbsolutePath(path.toString()))
                error = QStringLiteral("输出引用路径无效");
    }
    if (result.contains("columns"))
        for (const auto &column : result.value("columns").toArray()) {
            const auto item = column.toObject();
            if (!column.isObject() || item.value("key").toString().isEmpty() ||
                item.value("key").toString().size() > 80 ||
                item.value("label").toString().size() > 80)
                error = QStringLiteral("结果列定义无效");
        }
    if (result.contains("text") && !result.value("text").isString())
        error = QStringLiteral("文本结果类型无效");
    if (!result.value("status").isString())
        error = QStringLiteral("结果缺少终态标识");
    QString outcome = result.value("status").toString("succeeded");
    if (!QStringList{"succeeded", "failed", "cancelled"}.contains(outcome))
        error = QStringLiteral("结果状态无效");
    if (!error.isEmpty()) {
        result = {{"status", "failed"}, {"message", error}, {"partial_effects", externalMayRun}};
        outcome = "failed";
    }
    emit completed(request.jobId, result, outcome);
}

QJsonObject JobRunner::runExport() {
    QString error;
    const auto input = Files::readObject(request.jobRoot + "/input.json", 40 * 1024 * 1024, &error);
    const auto path = input.value("output_path").toString();
    auto failed = [&](const QString &message) {
        return QJsonObject{{"status", "failed"},
                           {"message", message},
                           {"completed", 0},
                           {"total", 1},
                           {"failed", 1}};
    };
    auto cancelled = []() {
        return QJsonObject{{"status", "cancelled"},
                           {"message", QStringLiteral("已取消导出，原文件保留")},
                           {"completed", 0},
                           {"total", 1},
                           {"failed", 0}};
    };
    if (!error.isEmpty() || !QDir::isAbsolutePath(path))
        return failed(QStringLiteral("导出输入无效 · ") + error);
    if (cancellation.load())
        return cancelled();
    const auto document = input.value("document").toObject();
    const auto bytes = input.value("plain_text").toBool()
                           ? document.value("text").toString().toUtf8()
                           : QJsonDocument(document).toJson();
    if (bytes.size() > 40 * 1024 * 1024)
        return failed(QStringLiteral("导出数据超过 40 MiB"));
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return failed(QStringLiteral("无法创建导出文件 · ") + file.errorString());
    QElapsedTimer clock;
    clock.start();
    qint64 written = 0;
    while (written < bytes.size()) {
        if (cancellation.load()) {
            file.cancelWriting();
            return cancelled();
        }
        const auto count = file.write(bytes.constData() + written,
                                      qMin(qint64(1024 * 1024), bytes.size() - written));
        if (count <= 0)
            return failed(QStringLiteral("导出写入失败，原文件保留 · ") + file.errorString());
        written += count;
        if (clock.elapsed() >= 100) {
            emit progress(request.jobId, {{"completed", double(written)},
                                          {"total", bytes.size()},
                                          {"message", QStringLiteral("正在导出结果")},
                                          {"cancellable", true}});
            clock.restart();
        }
    }
    if (cancellation.load()) {
        file.cancelWriting();
        return cancelled();
    }
    emit progress(request.jobId, {{"phase", "commit"},
                                  {"completed", 1},
                                  {"total", 1},
                                  {"message", QStringLiteral("正在提交导出文件")},
                                  {"cancellable", false}});
    if (!file.commit())
        return failed(QStringLiteral("导出提交失败，原文件保留 · ") + file.errorString());
    return {{"status", "succeeded"},
            {"message", QStringLiteral("已导出 · ") + path},
            {"artifacts", QJsonArray{path}},
            {"completed", 1},
            {"total", 1},
            {"failed", 0}};
}
