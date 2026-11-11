#include "video.h"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLockFile>
#include <QProcess>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStorageInfo>
#include <QUuid>
#include <cmath>
#include <memory>
#include <stdexcept>
#ifdef Q_OS_WIN
#define NOMINMAX
#include <windows.h>
#endif
namespace Video {
namespace {
struct Failure {
    QString text;
};
void fail(const QString &s) { throw Failure{s}; }
void check(const Hooks &h) {
    if (h.cancelled())
        fail(QStringLiteral("已取消"));
}
QString seconds(qint64 ms) { return QString::number(ms / 1000.0, 'f', 3); }
constexpr qint64 MaximumDuration = 7LL * 24 * 3600 * 1000;
struct Source {
    QString path, identity;
#ifdef Q_OS_WIN
    HANDLE handle = INVALID_HANDLE_VALUE;
#endif
    explicit Source(const QString &name) {
        const QFileInfo info(name);
        if (!info.isFile() || info.isSymLink())
            fail(QStringLiteral("视频必须是存在的普通文件"));
        path = info.canonicalFilePath();
#ifdef Q_OS_WIN
        // Deny writes/deletion while probing or exporting; FFmpeg can open a read handle.
        handle = CreateFileW(reinterpret_cast<LPCWSTR>(QDir::toNativeSeparators(path).utf16()),
                             GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
        if (handle == INVALID_HANDLE_VALUE)
            fail(QStringLiteral("无法只读锁定视频，请关闭正在修改该文件的程序"));
        BY_HANDLE_FILE_INFORMATION f{};
        if (!GetFileInformationByHandle(handle, &f)) {
            CloseHandle(handle);
            handle = INVALID_HANDLE_VALUE;
            fail(QStringLiteral("无法读取视频文件身份"));
        }
        identity = QString("%1:%2:%3:%4:%5:%6:%7")
                       .arg(f.dwVolumeSerialNumber)
                       .arg(f.nFileIndexHigh)
                       .arg(f.nFileIndexLow)
                       .arg(f.nFileSizeHigh)
                       .arg(f.nFileSizeLow)
                       .arg(f.ftLastWriteTime.dwHighDateTime)
                       .arg(f.ftLastWriteTime.dwLowDateTime);
#else
        identity = QString::number(info.size()) + ":" +
                   QString::number(info.lastModified().toMSecsSinceEpoch());
#endif
    }
    ~Source() {
#ifdef Q_OS_WIN
        if (handle != INVALID_HANDLE_VALUE)
            CloseHandle(handle);
#endif
    }
};
QByteArray run(const QString &program, const QStringList &args, const Hooks &h,
               int timeout = 120000, qint64 duration = 0) {
    check(h);
    QProcess p;
#ifdef Q_OS_WIN
    p.setCreateProcessArgumentsModifier(
        [](QProcess::CreateProcessArguments *a) { a->flags |= CREATE_NO_WINDOW; });
#endif
    p.setProgram(program);
    p.setArguments(args);
    p.start();
    if (!p.waitForStarted(5000))
        fail(QStringLiteral("无法启动包内媒体引擎 · ") + p.errorString());
    p.closeWriteChannel();
    QByteArray out, err, pending;
    QElapsedTimer elapsed, notified;
    elapsed.start();
    notified.start();
    auto drain = [&] {
        auto bytes = p.readAllStandardOutput();
        err = (err + p.readAllStandardError()).right(16384);
        if (!duration) {
            out += bytes;
            if (out.size() > 2 * 1024 * 1024) {
                p.kill();
                p.waitForFinished();
                fail(QStringLiteral("媒体分析输出超过限额"));
            }
        } else {
            pending += bytes;
            if (pending.size() > 65536)
                pending = pending.right(32768);
            int pos;
            while ((pos = pending.indexOf('\n')) >= 0) {
                const auto line = pending.left(pos).trimmed();
                pending.remove(0, pos + 1);
                if (line.startsWith("out_time_us=") && notified.elapsed() >= 150) {
                    bool ok = false;
                    auto us = line.mid(12).toLongLong(&ok);
                    if (ok)
                        h.progress(int(qBound(0LL, us / (duration * 10), 97LL)),
                                   QStringLiteral("正在导出视频"));
                    notified.restart();
                }
            }
        }
    };
    while (p.state() != QProcess::NotRunning) {
        p.waitForReadyRead(30);
        drain();
        if (h.cancelled() || elapsed.elapsed() > timeout) {
            p.kill();
            p.waitForFinished(5000);
            if (h.cancelled())
                fail(QStringLiteral("已取消"));
            fail(QStringLiteral("媒体引擎超过执行时限"));
        }
    }
    drain();
    check(h);
    if (p.exitStatus() != QProcess::NormalExit || p.exitCode() != 0)
        fail(QStringLiteral("媒体处理失败 · ") + QString::fromUtf8(err).right(2000));
    return out;
}
QJsonObject object(const QByteArray &bytes) {
    QJsonParseError e;
    auto d = QJsonDocument::fromJson(bytes, &e);
    if (e.error != QJsonParseError::NoError || !d.isObject())
        fail(QStringLiteral("媒体引擎返回了无效结果"));
    return d.object();
}
double number(const QJsonValue &v, double fallback = 0) {
    bool ok = false;
    auto n = v.toVariant().toString().toDouble(&ok);
    return ok && std::isfinite(n) ? n : fallback;
}
QJsonObject probe(const Source &s, const QString &bin, const Hooks &h) {
    h.progress(0, QStringLiteral("正在分析视频"));
    auto raw = object(
        run(bin + "/ffprobe.exe",
            {"-v", "error", "-show_entries",
             "format=duration,start_time,format_name:stream=index,codec_type,codec_name,width,"
             "height,pix_fmt,avg_frame_rate,r_frame_rate,duration,color_transfer,field_order:"
             "stream_tags=rotate:stream_disposition=attached_pic:stream_side_data=rotation",
             "-of", "json", s.path},
            h));
    const auto streams = raw.value("streams").toArray();
    if (streams.size() > 64)
        fail(QStringLiteral("媒体轨道超过64条，初版不支持"));
    QJsonObject video;
    QJsonArray audio;
    int ignored = 0;
    for (const auto &value : streams) {
        auto st = value.toObject();
        if (st.value("codec_type") == "video" && video.isEmpty() &&
            !st.value("disposition").toObject().value("attached_pic").toInt())
            video = st;
        else if (st.value("codec_type") == "audio")
            audio.append(
                QJsonObject{{"index", st.value("index")}, {"codec", st.value("codec_name")}});
        else
            ++ignored;
    }
    const auto format = raw.value("format").toObject();
    const auto duration =
        qRound64(number(format.value("duration"), number(video.value("duration"))) * 1000);
    if (video.isEmpty() || duration <= 0 || duration > MaximumDuration)
        fail(QStringLiteral("没有有效的视频画面/时长，或素材超过初版7天时长范围"));
    auto pix = video.value("pix_fmt").toString();
    auto transfer = video.value("color_transfer").toString();
    const bool precise =
        QStringList{"yuv420p", "yuvj420p", "yuv422p", "yuvj422p", "yuv444p", "yuvj444p",
                    "nv12",    "nv21",     "rgb24",   "bgr24",    "rgba",    "bgra",
                    "gray",    "gbrp",     "yuyv422", "uyvy422"}
            .contains(pix) &&
        transfer != "smpte2084" && transfer != "arib-std-b67" &&
        QStringList{"progressive", "unknown", ""}.contains(video.value("field_order").toString());
    int rotation = video.value("tags").toObject().value("rotate").toString().toInt();
    for (const auto &v : video.value("side_data_list").toArray())
        if (v.toObject().contains("rotation"))
            rotation = v.toObject().value("rotation").toInt();
    const auto codec = video.value("codec_name").toString();
    const bool fast =
        codec == "h264" && (audio.isEmpty() || audio.first().toObject().value("codec") == "aac");
    return {{"source", s.path},
            {"identity", s.identity},
            {"duration_ms", double(duration)},
            {"modified_ms", double(QFileInfo(s.path).lastModified().toMSecsSinceEpoch())},
            {"size_bytes", double(QFileInfo(s.path).size())},
            {"width", video.value("width")},
            {"height", video.value("height")},
            {"rotation", rotation},
            {"codec", codec},
            {"frame_rate", video.value("avg_frame_rate")},
            {"nominal_frame_rate", video.value("r_frame_rate")},
            {"video_index", video.value("index")},
            {"audio_tracks", audio},
            {"ignored_streams", ignored},
            {"source_start_seconds", number(format.value("start_time"))},
            {"precise_supported", precise},
            {"fast_supported", fast},
            {"warning", QStringLiteral("输出仅包含所选视频与一个音轨；字幕、附件和章节不保留。")}};
}
qint64 time(const QJsonObject &p, const QString &key, qint64 fallback) {
    if (!p.contains(key))
        return fallback;
    const double v = p.value(key).toDouble(-1);
    if (!std::isfinite(v) || v < 0 || v > MaximumDuration || std::floor(v) != v)
        fail(QStringLiteral("时间参数无效 · ") + key);
    return qint64(v);
}
QString fingerprint(const QJsonObject &p, const QString &identity) {
    return QCryptographicHash::hash(identity.toUtf8() +
                                        QJsonDocument(p).toJson(QJsonDocument::Compact),
                                    QCryptographicHash::Sha256)
        .toHex();
}
QJsonObject prepare(const Source &s, const QJsonObject &m, const QJsonObject &p, const QString &bin,
                    const Hooks &h) {
    const auto duration = qint64(m.value("duration_ms").toDouble());
    const auto start = time(p, "start_ms", 0), end = time(p, "end_ms", duration);
    if (start >= end || end > duration)
        fail(QStringLiteral("时间区间应满足 起点 < 终点，且不能超过视频时长"));
    const auto mode = p.value("mode").toString(QStringLiteral("精确裁剪"));
    if (!QStringList{QStringLiteral("精确裁剪"), QStringLiteral("快速截取")}.contains(mode))
        fail(QStringLiteral("导出方式无效"));
    const bool fast = mode == QStringLiteral("快速截取");
    if (!m.value(fast ? "fast_supported" : "precise_supported").toBool())
        fail(fast ? QStringLiteral("快速模式仅支持H.264与AAC音轨，请改用精确裁剪")
                  : QStringLiteral(
                        "初版精确模式暂不处理HDR、高位深或交错视频，避免画面颜色发生错误"));
    qint64 actual = start;
    if (fast && start > 0) {
        const auto origin = m.value("source_start_seconds").toDouble();
        auto frames =
            object(run(bin + "/ffprobe.exe",
                       {"-v", "error", "-select_streams", "v:0", "-skip_frame", "nokey",
                        "-read_intervals",
                        QString::number(qMax(0.0, origin + start / 1000.0 - 30), 'f', 3) + "%+32",
                        "-show_entries", "frame=key_frame,best_effort_timestamp_time", "-of",
                        "json", s.path},
                       h))
                .value("frames")
                .toArray();
        actual = -1;
        for (const auto &f : frames) {
            const auto st = f.toObject();
            auto ms =
                qRound64((number(st.value("best_effort_timestamp_time"), -1) - origin) * 1000);
            if (st.value("key_frame").toInt() && ms >= 0 && ms <= start && ms > actual)
                actual = ms;
        }
        if (actual < 0)
            fail(QStringLiteral("附近未找到可用关键帧，请使用精确裁剪"));
    }
    auto name = p.value("filename").toString().trimmed();
    if (name.isEmpty())
        name = QFileInfo(s.path).completeBaseName() + "_clip";
    if (name.endsWith(".mp4", Qt::CaseInsensitive))
        name.chop(4);
    if (name.isEmpty() || name.size() > 120 || name.endsWith('.') || name.endsWith(' ') ||
        name.contains(QRegularExpression("[<>:\"/\\\\|?*\\x00-\\x1f]")) ||
        QRegularExpression("^(con|prn|aux|nul|com[1-9]|lpt[1-9])(?:\\.|$)",
                           QRegularExpression::CaseInsensitiveOption)
            .match(name)
            .hasMatch())
        fail(QStringLiteral("输出文件名含不支持的字符或保留名称"));
    auto directory = p.value("output_dir").toString().trimmed();
    if (directory.isEmpty())
        directory = QFileInfo(s.path).absolutePath() + "/ModynToolbox Exports";
    if (!QDir::isAbsolutePath(directory))
        fail(QStringLiteral("输出目录必须是绝对路径"));
    const auto tracks = m.value("audio_tracks").toArray();
    int audio = tracks.isEmpty() || p.value("remove_audio").toBool()
                    ? -1
                    : tracks.first().toObject().value("index").toInt();
    if (p.contains("audio_index") && !p.value("remove_audio").toBool()) {
        int index = p.value("audio_index").toInt(-1);
        bool found = index == -1;
        for (const auto &t : tracks)
            if (t.toObject().value("index").toInt() == index)
                found = true;
        if (!found)
            fail(QStringLiteral("选中的音轨不存在，请重新分析视频"));
        audio = index;
    }
    if (fast && audio >= 0)
        for (const auto &track : tracks)
            if (track.toObject().value("index").toInt() == audio &&
                track.toObject().value("codec") != "aac")
                fail(QStringLiteral("所选音轨不支持快速复制，请改用精确裁剪或移除音频"));
    return {{"fingerprint", fingerprint(p, s.identity)},
            {"start_ms", double(actual)},
            {"requested_start_ms", double(start)},
            {"end_ms", double(end)},
            {"fast", fast},
            {"output_dir", QDir::cleanPath(directory)},
            {"filename", name},
            {"audio_index", audio},
            {"message", fast ? QStringLiteral("快速模式起点已对齐关键帧，末尾按编码包边界结束")
                             : QStringLiteral("精确模式将重新编码视频")}};
}
// A per-export lock distinguishes live writers from abandoned files after a process-tree kill.
// Recovery deletes only UUID-named files whose Windows file identity matches our registration.
void recoverTemporaries(const QString &dataRoot) {
    if (dataRoot.isEmpty())
        return;
    QDir folder(dataRoot + "/temporaries");
    const QRegularExpression uuid("^[0-9a-f-]{36}\\.json$");
    for (const auto &name : folder.entryList({"*.json"}, QDir::Files | QDir::NoSymLinks)) {
        if (!uuid.match(name).hasMatch())
            continue;
        const auto marker = folder.filePath(name);
        QLockFile lock(marker + ".lock");
        lock.setStaleLockTime(0);
        if (!lock.tryLock(0))
            continue;
        QFile file(marker);
        if (!file.open(QIODevice::ReadOnly) || file.size() > 4096)
            continue;
        const auto record = QJsonDocument::fromJson(file.readAll()).object();
        file.close();
        const auto path = record.value("path").toString();
        if (!QDir::isAbsolutePath(path) || QFileInfo(path).isSymLink() ||
            QFileInfo(path).fileName() != ".modyn-video-" + name.chopped(5) + ".mp4")
            continue;
        if (!QFileInfo::exists(path)) {
            QFile::remove(marker);
            continue;
        }
        bool owned = false;
        try {
            Source temp(path);
            owned = temp.identity.section(':', 0, 2) == record.value("file_id").toString();
        } catch (...) {
        }
        if (owned && QFile::remove(path))
            QFile::remove(marker);
    }
}
struct Temporary {
    QString path, journal;
    std::unique_ptr<QLockFile> lock;
    ~Temporary() {
        if (!path.isEmpty())
            QFile::remove(path);
        if (!journal.isEmpty())
            QFile::remove(journal);
    }
};
QString publish(const QString &temp, const QString &directory, const QString &base) {
    for (int i = 0; i < 1; ++i) {
        auto target =
            QDir(directory).filePath(base + (i ? QString(" (%1)").arg(i) : QString()) + ".mp4");
#ifdef Q_OS_WIN
        if (MoveFileExW(reinterpret_cast<LPCWSTR>(QDir::toNativeSeparators(temp).utf16()),
                        reinterpret_cast<LPCWSTR>(QDir::toNativeSeparators(target).utf16()),
                        MOVEFILE_WRITE_THROUGH))
            return target;
        const auto error = GetLastError();
        if (error != ERROR_ALREADY_EXISTS && error != ERROR_FILE_EXISTS)
            fail(QStringLiteral("无法提交输出文件，请检查目录权限和磁盘空间"));
#else
        if (!QFileInfo::exists(target) && QFile::rename(temp, target))
            return target;
        if (!QFileInfo::exists(target))
            fail(QStringLiteral("无法提交输出文件"));
#endif
    }
    fail(QStringLiteral("提交时目标文件已出现，请重试导出以重新编号"));
    return {};
}
} // namespace
QJsonObject execute(const QString &action, const QJsonObject &input, const QString &bin,
                    const QString &dataRoot, const Hooks &h) {
    QJsonObject context;
    try {
        recoverTemporaries(dataRoot);
        const auto files = input.value("files").toArray();
        if (files.size() != 1 || !files.first().isString())
            fail(QStringLiteral("请选择一个视频文件"));
        Source source(files.first().toString());
        auto params = input.value("params").toObject();
        auto previous = input.value("context").toObject();
        auto media = probe(source, bin, h);
        context["media"] = media;
        if (action == "probe")
            return {{"status", "succeeded"},
                    {"message", QStringLiteral("视频分析完成")},
                    {"context", context}};
        if (action != "prepare" && action != "export")
            fail(QStringLiteral("未知媒体操作"));
        auto previousMedia = previous.value("media").toObject();
        if (!previousMedia.isEmpty() && previousMedia.value("identity") != source.identity)
            fail(QStringLiteral("源视频已变化，请重新分析并确认选段"));
        auto plan = prepare(source, media, params, bin, h);
        context["plan"] = plan;
        if (action == "prepare")
            return {
                {"status", "succeeded"}, {"message", plan.value("message")}, {"context", context}};
        const bool fast = plan.value("fast").toBool();
        if (fast &&
            previous.value("plan").toObject().value("fingerprint") != plan.value("fingerprint"))
            fail(QStringLiteral("请先预览快速截取区间，再确认导出"));
        const auto directory = plan.value("output_dir").toString();
        if (!QDir().mkpath(directory))
            fail(QStringLiteral("无法创建输出目录，请选择可写目录"));
        QStorageInfo storage(directory);
        if (storage.isValid() && storage.bytesAvailable() < 16 * 1024 * 1024)
            fail(QStringLiteral("输出目录可用空间不足16MiB"));
        Temporary temp;
        const auto id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        temp.path = QDir(directory).filePath(".modyn-video-" + id + ".mp4");
        QFile claim(temp.path);
        if (!claim.open(QIODevice::WriteOnly | QIODevice::NewOnly))
            fail(QStringLiteral("输出目录不可写"));
        claim.close();
        if (!dataRoot.isEmpty()) {
            QDir().mkpath(dataRoot + "/temporaries");
            temp.journal = dataRoot + "/temporaries/" + id + ".json";
            temp.lock = std::make_unique<QLockFile>(temp.journal + ".lock");
            temp.lock->setStaleLockTime(0);
            if (!temp.lock->tryLock(0))
                fail(QStringLiteral("无法锁定导出暂存记录"));
            Source temporaryIdentity(temp.path);
            QSaveFile file(temp.journal);
            if (!file.open(QIODevice::WriteOnly) ||
                file.write(
                    QJsonDocument(
                        QJsonObject{{"path", temp.path},
                                    {"file_id", temporaryIdentity.identity.section(':', 0, 2)},
                                    {"pid", double(QCoreApplication::applicationPid())}})
                        .toJson()) < 0 ||
                !file.commit())
                fail(QStringLiteral("无法记录导出暂存文件"));
        }
        const auto start = qint64(plan.value("start_ms").toDouble());
        const auto end = qint64(plan.value("end_ms").toDouble());
        QStringList args{"-hide_banner",
                         "-loglevel",
                         "error",
                         "-nostdin",
                         "-y",
                         "-ss",
                         seconds(start),
                         "-i",
                         source.path,
                         "-t",
                         seconds(end - start),
                         "-map",
                         "0:" + QString::number(media.value("video_index").toInt())};
        int audio = plan.value("audio_index").toInt(-1);
        if (audio >= 0)
            args << "-map" << "0:" + QString::number(audio);
        else
            args << "-an";
        args << "-map_metadata" << "-1" << "-map_chapters" << "-1";
        if (fast)
            args << "-c" << "copy" << "-avoid_negative_ts" << "make_zero";
        else {
            auto quality = params.value("quality").toString(QStringLiteral("较高画质"));
            auto size = params.value("size").toString(QStringLiteral("原尺寸"));
            const QStringList qualities{QStringLiteral("较高画质"), QStringLiteral("均衡"),
                                        QStringLiteral("较小文件")};
            const QStringList sizes{QStringLiteral("原尺寸"), QStringLiteral("最长边1920"),
                                    QStringLiteral("最长边1280")};
            if (!qualities.contains(quality) || !sizes.contains(size))
                fail(QStringLiteral("画质或尺寸参数无效"));
            auto filter = QStringLiteral("scale=trunc(iw*sar/2)*2:trunc(ih/2)*2,setsar=1");
            if (size != QStringLiteral("原尺寸")) {
                auto edge = size == QStringLiteral("最长边1920") ? "1920" : "1280";
                filter += QString(",scale=w='min(iw,%1)':h='min(ih,%1)':force_original_aspect_"
                                  "ratio=decrease:force_divisible_by=2")
                              .arg(edge);
            }
            args << "-vf" << filter << "-c:v" << "libx264" << "-preset" << "fast" << "-threads"
                 << "4"
                 << "-filter_threads" << "2" << "-crf"
                 << (quality == qualities[0]   ? "18"
                     : quality == qualities[1] ? "23"
                                               : "28")
                 << "-pix_fmt" << "yuv420p";
            if (audio >= 0)
                args << "-c:a" << "aac" << "-b:a" << "128k";
        }
        args << "-movflags" << "+faststart" << "-progress" << "pipe:1" << "-f" << "mp4"
             << temp.path;
        run(bin + "/ffmpeg.exe", args, h, 86400000, end - start);
        h.progress(98, QStringLiteral("正在校验输出视频"));
        Source output(temp.path);
        auto validationHooks = h;
        validationHooks.progress = [](int, const QString &) {};
        auto checked = probe(output, bin, validationHooks);
        if (QFileInfo(temp.path).size() <= 0 || checked.value("duration_ms").toDouble() <= 0)
            fail(QStringLiteral("输出视频校验失败"));
        // Source output's read handle must close before renaming.
        auto actualDuration = checked.value("duration_ms");
        check(h);
        QString chosenBase = plan.value("filename").toString();
        const auto originalBase = chosenBase;
        int suffix = 0;
        while (QFileInfo::exists(QDir(directory).filePath(chosenBase + ".mp4"))) {
            if (++suffix > 9999)
                fail(QStringLiteral("同名输出过多"));
            chosenBase = originalBase + QString(" (%1)").arg(suffix);
        }
        h.reserve({QDir(directory).filePath(chosenBase + ".mp4")});
        h.commit();
        // Close the temporary's validation handle before the atomic no-replace move.
#ifdef Q_OS_WIN
        CloseHandle(output.handle);
        output.handle = INVALID_HANDLE_VALUE;
#endif
        const auto path = publish(temp.path, directory, chosenBase);
        temp.path.clear();
        auto bytes = QFileInfo(path).size();
        QJsonArray columns{
            QJsonObject{{"key", "file"}, {"label", QStringLiteral("输出文件")}},
            QJsonObject{{"key", "duration"}, {"label", QStringLiteral("时长")}},
            QJsonObject{{"key", "size"}, {"label", QStringLiteral("实际大小")}},
            QJsonObject{{"key", "change"}, {"label", QStringLiteral("较完整原视频")}}};
        return {
            {"status", "succeeded"},
            {"message", QStringLiteral("视频已导出 · %1 MiB").arg(bytes / 1048576.0, 0, 'f', 2)},
            {"artifacts", QJsonArray{path}},
            {"context", context},
            {"completed", 1},
            {"total", 1},
            {"failed", 0},
            {"duration_ms", actualDuration},
            {"size_bytes", double(bytes)},
            {"size_change_percent", 100.0 * (bytes / media.value("size_bytes").toDouble() - 1)},
            {"columns", columns},
            {"rows",
             QJsonArray{QJsonObject{
                 {"file", path},
                 {"duration", QString::number(actualDuration.toDouble() / 1000, 'f', 3) + " s"},
                 {"size", QString::number(bytes / 1048576.0, 'f', 2) + " MiB"},
                 {"change",
                  QString::number(100.0 * (bytes / media.value("size_bytes").toDouble() - 1), 'f',
                                  1) +
                      "%"}}}}};
    } catch (const Failure &f) {
        return {{"status", h.cancelled() ? "cancelled" : "failed"},
                {"message", f.text},
                {"context", context}};
    } catch (const std::exception &e) {
        return {{"status", h.cancelled() ? "cancelled" : "failed"},
                {"message", QString::fromUtf8(e.what())},
                {"context", context}};
    }
}
} // namespace Video
