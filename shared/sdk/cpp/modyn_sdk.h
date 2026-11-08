#pragma once
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <atomic>
#include <chrono>
#include <iostream>
#include <mutex>
#include <thread>
#include <QStringList>
#include <stdexcept>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <unistd.h>
#endif
namespace Modyn {
inline std::atomic_bool stopped = false, granted = false, committing = false;
inline QByteArray readLine() {
    QByteArray line;
    char byte = 0;
    while (line.size() <= 65536) {
#ifdef _WIN32
        DWORD count = 0;
        if (!ReadFile(GetStdHandle(STD_INPUT_HANDLE), &byte, 1, &count, nullptr) || count == 0)
            return line;
#else
        if (::read(STDIN_FILENO, &byte, 1) != 1)
            return line;
#endif
        if (byte == '\n')
            return line;
        line.append(byte);
    }
    throw std::runtime_error("Control line too large");
}
inline std::mutex output;
inline QJsonObject request;
inline int sequence = 0;
inline void send(const QString &kind, QJsonObject values = {}) {
    std::lock_guard<std::mutex> lock(output);
    for (const auto &key : {"protocol", "tool_id", "session_id", "job_id", "revision"})
        values[key] = request.value(key);
    values["kind"] = kind;
    values["seq"] = ++sequence;
    const auto bytes = QJsonDocument(values).toJson(QJsonDocument::Compact);
    if (bytes.size() > 65536)
        throw std::runtime_error("Protocol message too large");
    std::cout.write(bytes.constData(), bytes.size());
    std::cout.put('\n');
    std::cout.flush();
}
inline QJsonObject readInput() {
    const auto line = readLine();
    if (line.size() > 65536)
        throw std::runtime_error("Request too large");
    request = QJsonDocument::fromJson(line).object();
    if (request.value("protocol").toInt() != 1 || request.value("kind") != "run")
        throw std::runtime_error("Unsupported protocol");
    send("ready");
    std::thread([] {
        try {
            while (true) {
                const auto line = readLine();
                if (line.isEmpty())
                    return;
                const auto message = QJsonDocument::fromJson(line).object();
                if (message.value("job_id") != request.value("job_id"))
                    continue;
                if (message.value("kind") == "cancel")
                    stopped = true;
                if (message.value("kind") == "resources_granted")
                    granted = true;
            }
        } catch (...) {
            stopped = true;
        }
    }).detach();
    QFile input(request.value("job_root").toString() + "/input.json");
    if (!input.open(QIODevice::ReadOnly) || input.size() > 16 * 1024 * 1024)
        throw std::runtime_error("Invalid input");
    return QJsonDocument::fromJson(input.readAll()).object();
}
inline bool save(const QString &path, const QJsonObject &value) {
    QSaveFile file(path);
    const auto bytes = QJsonDocument(value).toJson(QJsonDocument::Compact);
    return bytes.size() <= 32 * 1024 * 1024 && file.open(QIODevice::WriteOnly) &&
           file.write(bytes) == bytes.size() && file.commit();
}
inline void progress(int completed, int total, const QString &message = {}) {
    send("progress", {{"completed", completed},
                      {"total", total},
                      {"message", message},
                      {"cancellable", !committing.load()}});
}
inline void reserve(const QStringList &paths) {
    QJsonArray values;
    for (const auto &path : paths)
        values.append(path);
    if (!save(request.value("job_root").toString() + "/resources.json", {{"paths", values}}))
        throw std::runtime_error("Resource file failed");
    send("reserve", {{"resources_ref", "resources.json"}});
    while (!granted && !stopped)
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    if (stopped)
        throw std::runtime_error("Cancelled");
}
inline void critical() {
    if (stopped)
        throw std::runtime_error("Cancelled");
    if (request.value("dynamic_resources").toBool() && !granted)
        throw std::runtime_error("Resources not granted");
    committing = true;
    send("progress", {{"phase", "commit"}, {"cancellable", false}, {"message", "Committing"}});
}
inline int result(QJsonObject value) {
    if (!value.contains("status"))
        value["status"] = stopped && !committing ? "cancelled" : "succeeded";
    if (!save(request.value("job_root").toString() + "/result.json", value))
        return 2;
    send("result", {{"result_ref", "result.json"}});
    return 0;
}
} // namespace Modyn
