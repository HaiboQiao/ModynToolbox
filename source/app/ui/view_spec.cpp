#include "view_spec.h"
#include "core/files.h"
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QRegularExpression>
#include <QSet>
#include <functional>
#include <cmath>
ViewSpec loadViewSpec(const QString &packageRoot, bool required) {
    ViewSpec spec;
    auto invalid = [&spec](const QString &reason) {
        spec = ViewSpec{};
        spec.error = QStringLiteral("工具界面无法加载 · ") + reason;
        return spec;
    };
    if (packageRoot.isEmpty())
        return required ? invalid(QStringLiteral("工具包路径缺失")) : spec;
    const auto path = Files::child(packageRoot, "ui.json", true);
    if (path.isEmpty())
        return required ? invalid(QStringLiteral("界面文件缺失或为链接")) : spec;
    QString error;
    const auto object = Files::readObject(path, 65536, &error);
    if (!error.isEmpty())
        return invalid(error);
    spec.version = object.value("view_version").toInt();
    if (spec.version != 1 && spec.version != 2 && spec.version != 3)
        return invalid(QStringLiteral("不支持的界面版本"));
    if (spec.version >= 2 && object.contains("media")) {
        const auto media = object.value("media").toObject();
        const QRegularExpression action("^[a-z][a-z0-9-]{0,63}$");
        spec.mediaPrepareWhen = media.value("prepare_when").toObject();
        spec.mediaProbe = media.value("probe_action").toString();
        spec.mediaPrepare = media.value("prepare_action").toString();
        spec.mediaExport = media.value("export_action").toString();
        if (!action.match(spec.mediaProbe).hasMatch() || !action.match(spec.mediaPrepare).hasMatch() ||
            !action.match(spec.mediaExport).hasMatch() || spec.mediaProbe == spec.mediaPrepare ||
            spec.mediaProbe == spec.mediaExport || spec.mediaPrepare == spec.mediaExport)
            return invalid(QStringLiteral("媒体动作绑定无效"));
    }
    spec.kind = object.value("kind").toString();
    if (spec.kind != "text" && spec.kind != "files")
        return invalid(QStringLiteral("不支持的布局"));
    for (const auto &key : {"input_title", "hint", "placeholder", "initial_text"})
        if (object.contains(key) &&
            (!object.value(key).isString() || object.value(key).toString().size() > 2048))
            return invalid(QStringLiteral("文本字段无效或过长"));
    spec.inputTitle = object.value("input_title").toString(spec.inputTitle);
    spec.hint = object.value("hint").toString(spec.hint);
    spec.placeholder = object.value("placeholder").toString(spec.placeholder);
    spec.initialText = object.value("initial_text").toString();
    if (object.contains("extensions") && !object.value("extensions").isArray())
        return invalid(QStringLiteral("扩展名列表无效"));
    const auto extensions = object.value("extensions").toArray();
    if (extensions.size() > 20)
        return invalid(QStringLiteral("扩展名过多"));
    const QRegularExpression extensionPattern("^[a-z0-9]{1,8}$");
    for (const auto &entry : extensions) {
        if (!entry.isString() || !extensionPattern.match(entry.toString()).hasMatch())
            return invalid(QStringLiteral("扩展名无效"));
        spec.extensions.append(entry.toString());
    }
    if (object.contains("fields") && !object.value("fields").isArray())
        return invalid(QStringLiteral("参数列表无效"));
    const auto fields = object.value("fields").toArray();
    if (fields.size() > 12)
        return invalid(QStringLiteral("参数超过 12 项"));
    QSet<QString> identifiers;
    const QRegularExpression pattern("^[a-z][a-z0-9_]{0,39}$");
    for (const auto &entry : fields) {
        if (!entry.isObject())
            return invalid(QStringLiteral("参数结构无效"));
        const auto field = entry.toObject();
        FieldSpec item;
        item.id = field.value("id").toString();
        item.label = field.value("label").toString();
        item.type = field.value("type").toString();
        if (!pattern.match(item.id).hasMatch() || identifiers.contains(item.id) ||
            item.label.trimmed().isEmpty() || item.label.size() > 80)
            return invalid(QStringLiteral("参数名称或标识无效"));
        identifiers.insert(item.id);
        if (spec.version >= 3) {
            for (const auto &key : {"help", "placeholder", "options_source", "options_depends_on"})
                if (field.contains(key) && (!field.value(key).isString() || field.value(key).toString().size() > 512))
                    return invalid(QStringLiteral("字段说明或来源无效"));
            for (const auto &key : {"compact", "advanced", "searchable"})
                if (field.contains(key) && !field.value(key).isBool())
                    return invalid(QStringLiteral("字段布局标记无效"));
            item.help = field.value("help").toString();
            item.placeholder = field.value("placeholder").toString();
            item.optionsSource = field.value("options_source").toString();
            item.optionsDependsOn = field.value("options_depends_on").toString();
            item.compact = field.value("compact").toBool();
            item.advanced = field.value("advanced").toBool();
            item.searchable = field.value("searchable").toBool();
            if ((!item.optionsSource.isEmpty() && (!QRegularExpression("^[a-zA-Z0-9_]+(\\.[a-zA-Z0-9_]+){0,7}$").match(item.optionsSource).hasMatch() || item.type != "choice")) ||
                (!item.optionsDependsOn.isEmpty() && item.optionsSource.isEmpty()))
                return invalid(QStringLiteral("动态选项路径无效"));
        }
        if (item.type == "choice") {
            const auto options = field.value("options").toArray();
            if ((item.optionsSource.isEmpty() && (!field.value("options").isArray() || options.isEmpty())) ||
                options.size() > (spec.version >= 3 ? 200 : 20) || !field.value("default").isString())
                return invalid(QStringLiteral("选项结构无效"));
            for (const auto &option : options) {
                const auto name = option.isString() ? option.toString() : option.toObject().value("label").toString();
                const auto value = option.isString() ? option.toString() : option.toObject().value("value").toString();
                if ((!option.isString() && (spec.version < 3 || !option.isObject() || !option.toObject().value("value").isString())) || name.isEmpty() || name.size() > (spec.version >= 3 ? 160 : 80) ||
                    value.size() > 512 || (spec.version >= 3 && item.optionValues.contains(value)))
                    return invalid(QStringLiteral("选项文本或值无效"));
                item.options.append(name);
                item.optionValues.append(value);
            }
            item.value = field.value("default").toString();
            if (item.optionsSource.isEmpty() && !item.optionValues.contains(item.value))
                return invalid(QStringLiteral("默认选项无效"));
        } else if (item.type == "range" || item.type == "number") {
            const int limit = item.type == "range" ? 100 : 1000000;
            for (const auto &key : {"minimum", "maximum", "default"}) {
                const auto number = field.value(key).toDouble(-1);
                if (!field.value(key).isDouble() || number < 0 || number > limit ||
                    std::floor(number) != number)
                    return invalid(QStringLiteral("数值范围无效"));
            }
            item.minimum = field.value("minimum").toInt();
            item.maximum = field.value("maximum").toInt();
            item.number = field.value("default").toInt();
            if (item.minimum >= item.maximum || item.number < item.minimum ||
                item.number > item.maximum)
                return invalid(QStringLiteral("数值默认值无效"));
        } else if (item.type == "boolean") {
            if (!field.value("default").isBool())
                return invalid(QStringLiteral("布尔默认值无效"));
            item.boolean = field.value("default").toBool();
        } else if (item.type == "text" || item.type == "directory") {
            if (!field.value("default").isString() ||
                field.value("default").toString().size() > 512)
                return invalid(QStringLiteral("默认文本无效"));
            item.value = field.value("default").toString();
        } else
            return invalid(QStringLiteral("不支持的参数控件"));
        if (field.contains("enabled_when")) {
            if (!field.value("enabled_when").isObject())
                return invalid(QStringLiteral("条件结构无效"));
            item.condition = field.value("enabled_when").toObject();
            if (item.condition.size() != 2 ||
                (!item.condition.contains("equals") && !item.condition.contains("not")))
                return invalid(QStringLiteral("条件操作无效"));
        }
        spec.fields.append(item);
    }
    for (const auto &field : spec.fields) {
        if (!field.optionsDependsOn.isEmpty()) {
            bool earlier = false;
            for (const auto &candidate : spec.fields) {
                if (candidate.id == field.id) break;
                if (candidate.id == field.optionsDependsOn && candidate.type == "choice") earlier = true;
            }
            if (!earlier) return invalid(QStringLiteral("动态选项依赖须引用前面的下拉字段"));
        }
    }
    for (const auto &field : spec.fields)
        if (!field.condition.isEmpty() &&
            !identifiers.contains(field.condition.value("field").toString()))
            return invalid(QStringLiteral("条件引用不存在"));
    if (!spec.mediaProbe.isEmpty() &&
        (spec.kind != "files" || !object.value("layout").isObject() ||
         spec.mediaPrepareWhen.size() != 2 || !spec.mediaPrepareWhen.contains("equals") ||
         !identifiers.contains(spec.mediaPrepareWhen.value("field").toString()) ||
         identifiers.contains("start_ms") || identifiers.contains("end_ms") || identifiers.contains("audio_index")))
        return invalid(QStringLiteral("媒体布局、条件或保留参数无效"));
    if (object.contains("layout")) {
        if (!object.value("layout").isObject())
            return invalid(QStringLiteral("组件树无效"));
        spec.layout = object.value("layout").toObject();
        QSet<QString> components;
        int count = 0;
        std::function<bool(const QJsonObject &, int)> validate = [&](const QJsonObject &node,
                                                                     int depth) {
            if (depth > 8 || ++count > 80)
                return false;
            const auto type = node.value("type").toString();
            if (type == "row" || type == "column") {
                const auto children = node.value("children").toArray();
                if (children.isEmpty() || children.size() > 16)
                    return false;
                for (const auto &child : children)
                    if (!child.isObject() || !validate(child.toObject(), depth + 1))
                        return false;
                return true;
            }
            if (!(QStringList{"files", "text", "fields", "actions", "output_text", "output_table",
                              "image_preview", "status"}.contains(type) ||
                  (spec.version >= 2 && !spec.mediaProbe.isEmpty() && type == "video_editor")) ||
                components.contains(type))
                return false;
            components.insert(type);
            return true;
        };
        if (!validate(spec.layout, 1) ||
            (!spec.mediaProbe.isEmpty() && (!components.contains("video_editor") || components.contains("files") || components.contains("text"))))
            return invalid(QStringLiteral("未知、重复或超限的界面组件"));
    }
    return spec;
}
