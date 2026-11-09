#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <iostream>
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    std::string line;
    if (!std::getline(std::cin, line)) return 2;
    QJsonParseError error;
    const auto request = QJsonDocument::fromJson(QByteArray::fromStdString(line), &error);
    if (error.error != QJsonParseError::NoError || !request.isObject()) return 3;
    auto result = request.object();
    result["kind"] = "result";
    result["engine"] = "cpp";
    std::cout << QJsonDocument(result).toJson(QJsonDocument::Compact).toStdString() << std::endl;
    return 0;
}
