#include "hash_core.h"
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <iostream>
int main(int argc, char **argv) {
    QCoreApplication application(argc, argv);
    const auto arguments = application.arguments();
    if (arguments.size() != 3 || arguments[1] != "--file") {
        std::cerr << "Usage: ModynHashCli.exe --file <path>" << std::endl;
        return 2;
    }
    const auto value = hashFile(arguments[2], QCryptographicHash::Sha256);
    const QJsonObject result{
        {"path", value.path}, {"sha256", value.digest}, {"error", value.error}};
    std::cout << QJsonDocument(result).toJson(QJsonDocument::Compact).constData() << std::endl;
    return value.error.isEmpty() ? 0 : 1;
}
