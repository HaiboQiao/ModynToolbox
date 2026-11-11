#include "../core/video.h"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonDocument>
#include <iostream>
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    const auto a = app.arguments();
    if (a.size() < 3) {
        std::cerr << "ModynVideoCli <probe|prepare|export> <input.json> [cancel-after-ms]\n";
        return 2;
    }
    QFile file(a[2]);
    if (!file.open(QIODevice::ReadOnly) || file.size() > 1024 * 1024)
        return 2;
    auto input = QJsonDocument::fromJson(file.readAll()).object();
    QElapsedTimer elapsed;
    elapsed.start();
    Video::Hooks hooks;
    if (a.size() > 3)
        hooks.cancelled = [&] { return elapsed.elapsed() > a[3].toInt(); };
    const auto result =
        Video::execute(a[1], input, QCoreApplication::applicationDirPath(), {}, hooks);
    std::cout << QJsonDocument(result).toJson(QJsonDocument::Compact).constData() << std::endl;
    return result.value("status") == "succeeded" ? 0 : 1;
}
