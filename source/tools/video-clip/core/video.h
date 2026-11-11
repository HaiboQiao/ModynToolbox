#pragma once
#include <QJsonObject>
#include <QStringList>
#include <functional>
namespace Video {
struct Hooks {
    std::function<bool()> cancelled = [] { return false; };
    std::function<void(int, const QString &)> progress = [](int, const QString &) {};
    std::function<void(const QStringList &)> reserve = [](const QStringList &) {};
    std::function<void()> commit = [] {};
};
QJsonObject execute(const QString &action, const QJsonObject &input, const QString &binaryRoot,
                    const QString &dataRoot, const Hooks &hooks = {});
} // namespace Video
