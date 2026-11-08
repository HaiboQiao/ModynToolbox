#include "view_spec.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>
#include <cmath>
ViewSpec loadViewSpec(const QString &packageRoot, bool required) {
    ViewSpec spec;
    auto invalid = [&spec](const QString &reason) {
        spec = ViewSpec{};
        spec.error = QStringLiteral("工具界面无法加载 · %1").arg(reason);
        return spec;
    };
    if (packageRoot.isEmpty()) return required ? invalid(QStringLiteral("工具包路径缺失")) : spec;
    const auto path = QDir(packageRoot).absoluteFilePath("ui.json");
    if (!QFileInfo::exists(path)) return required ? invalid(QStringLiteral("界面文件缺失")) : spec;
    QFile file(path);
    if (QFileInfo(path).isSymLink() || !file.open(QIODevice::ReadOnly)) return invalid(QStringLiteral("界面文件不可读"));
    const auto bytes = file.read(65537);
    if (bytes.size() > 65536) return invalid(QStringLiteral("界面描述超过 64 KiB"));
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(bytes,&parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) return invalid(QStringLiteral("JSON 格式无效"));
    const auto object = document.object();
    if (object.value("view_version") != QJsonValue(1)) return invalid(QStringLiteral("不支持的界面版本"));
    spec.kind = object.value("kind").toString();
    if (spec.kind != "text" && spec.kind != "files") return invalid(QStringLiteral("不支持的布局"));
    for (const auto &key : {"input_title","hint","placeholder","initial_text"}) {
        if (object.contains(key) && (!object.value(key).isString() || object.value(key).toString().size() > 2048))
            return invalid(QStringLiteral("文本字段无效或过长"));
    }
    spec.inputTitle = object.value("input_title").toString(spec.inputTitle);
    spec.hint = object.value("hint").toString(spec.hint);
    spec.placeholder = object.value("placeholder").toString(spec.placeholder);
    spec.initialText = object.value("initial_text").toString();
    if (object.contains("extensions") && !object.value("extensions").isArray()) return invalid(QStringLiteral("扩展名列表无效"));
    const auto extensions = object.value("extensions").toArray();
    if (extensions.size() > 20) return invalid(QStringLiteral("扩展名过多"));
    const QRegularExpression extensionPattern("^[a-z0-9]{1,8}$");
    for (const auto &entry : extensions) {
        const auto extension = entry.toString();
        if (!entry.isString() || !extensionPattern.match(extension).hasMatch()) return invalid(QStringLiteral("扩展名无效"));
        spec.extensions.append(extension);
    }
    if (object.contains("fields") && !object.value("fields").isArray()) return invalid(QStringLiteral("参数列表无效"));
    const auto fields = object.value("fields").toArray();
    if (fields.size() > 12) return invalid(QStringLiteral("参数超过 12 项"));
    QSet<QString> identifiers;
    const QRegularExpression fieldPattern("^[a-z][a-z0-9_]{0,39}$");
    for (const auto &entry : fields) {
        if (!entry.isObject()) return invalid(QStringLiteral("参数结构无效"));
        const auto field = entry.toObject();
        FieldSpec item;
        item.id = field.value("id").toString();
        item.label = field.value("label").toString();
        item.type = field.value("type").toString();
        if (!fieldPattern.match(item.id).hasMatch() || identifiers.contains(item.id) || item.label.trimmed().isEmpty() || item.label.size() > 80)
            return invalid(QStringLiteral("参数名称或标识无效"));
        identifiers.insert(item.id);
        if (item.type == "choice") {
            if (!field.value("options").isArray() || !field.value("default").isString()) return invalid(QStringLiteral("选项结构无效"));
            const auto options = field.value("options").toArray();
            if (options.isEmpty() || options.size() > 20) return invalid(QStringLiteral("选项数量无效"));
            for (const auto &option : options) {
                if (!option.isString() || option.toString().isEmpty() || option.toString().size() > 80) return invalid(QStringLiteral("选项文本无效"));
                item.options.append(option.toString());
            }
            item.value = field.value("default").toString();
            if (!item.options.contains(item.value)) return invalid(QStringLiteral("默认选项无效"));
        } else if (item.type == "range") {
            for (const auto &key : {"minimum","maximum","default"}) {
                const auto value = field.value(key);
                const auto number = value.toDouble(-1);
                if (!value.isDouble() || number < 0 || number > 100 || std::floor(number) != number) return invalid(QStringLiteral("滑块范围无效"));
            }
            item.minimum = field.value("minimum").toInt();
            item.maximum = field.value("maximum").toInt();
            item.number = field.value("default").toInt();
            if (item.minimum >= item.maximum || item.number < item.minimum || item.number > item.maximum) return invalid(QStringLiteral("滑块默认值无效"));
        } else if (item.type == "text") {
            if (!field.value("default").isString() || field.value("default").toString().size() > 512) return invalid(QStringLiteral("默认文本无效"));
            item.value = field.value("default").toString();
        } else return invalid(QStringLiteral("不支持的参数控件"));
        spec.fields.append(item);
    }
    return spec;
}
