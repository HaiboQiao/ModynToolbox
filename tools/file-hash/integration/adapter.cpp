#include "hash_core.h"
#include "modyn_sdk.h"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QJsonArray>
#include <iostream>
int main(int argc, char **argv) {
    QCoreApplication application(argc, argv);
    try {
        const auto input = Modyn::readInput();
        if (Modyn::request.value("action") != "hash")
            throw std::runtime_error("Unsupported action");
        const auto files = input.value("files").toArray();
        if (files.isEmpty() || files.size() > 2000)
            throw std::runtime_error("Choose 1-2000 files");
        const auto name = input.value("params").toObject().value("algorithm").toString("SHA-256");
        if (name != "SHA-256" && name != "SHA-512")
            throw std::runtime_error("Unsupported algorithm");
        const auto algorithm =
            name == "SHA-512" ? QCryptographicHash::Sha512 : QCryptographicHash::Sha256;
        const QJsonArray columns{
            QJsonObject{{"key", "name"}, {"label", QStringLiteral("文件")}},
            QJsonObject{{"key", "algorithm"}, {"label", QStringLiteral("算法")}},
            QJsonObject{{"key", "digest"}, {"label", QStringLiteral("校验值")}},
            QJsonObject{{"key", "bytes"}, {"label", QStringLiteral("读取字节")}},
            QJsonObject{{"key", "status"}, {"label", QStringLiteral("状态")}}};
        QJsonArray rows;
        int succeeded = 0;
        QElapsedTimer updates;
        updates.start();
        for (int index = 0; index < files.size(); ++index) {
            if (Modyn::stopped)
                break;
            const auto path = files[index].toString();
            const auto value = hashFile(
                path, algorithm, [] { return Modyn::stopped.load(); },
                [&](qint64 done, qint64 total) {
                    if (updates.elapsed() >= 300) {
                        Modyn::progress(index, files.size(),
                                        QStringLiteral("%1 · %2 / %3 MiB")
                                            .arg(QFileInfo(path).fileName())
                                            .arg(done / 1048576)
                                            .arg(total / 1048576));
                        updates.restart();
                    }
                });
            if (value.cancelled)
                break;
            if (value.error.isEmpty())
                ++succeeded;
            rows.append(QJsonObject{
                {"name", QFileInfo(path).fileName()},
                {"path", value.path},
                {"algorithm", name},
                {"digest", value.digest},
                {"bytes", QString::number(value.bytes)},
                {"status", value.error.isEmpty() ? QStringLiteral("成功") : value.error}});
            Modyn::progress(index + 1, files.size(), QFileInfo(path).fileName());
        }
        return Modyn::result(
            {{"status", Modyn::stopped ? "cancelled"
                        : succeeded    ? "succeeded"
                                       : "failed"},
             {"message", QStringLiteral("校验成功 %1 / %2%3")
                             .arg(succeeded)
                             .arg(files.size())
                             .arg(Modyn::stopped ? QStringLiteral(" · 已取消") : QString())},
             {"completed",int(rows.size())},{"total",int(files.size())},{"failed",int(rows.size())-succeeded},{"columns", columns},
             {"rows", rows}});
    } catch (const std::exception &error) {
        return Modyn::result({{"status", "failed"}, {"message", QString::fromUtf8(error.what())}});
    }
}
