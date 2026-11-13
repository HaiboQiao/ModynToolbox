#pragma once
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVector>
struct FieldSpec {
    QString id, label, type;
    QStringList options, optionValues;
    QString help, placeholder, optionsSource, optionsDependsOn;
    bool compact = false, advanced = false, searchable = false;
    QString value;
    int number = 0, minimum = 0, maximum = 100;
    bool boolean = false;
    QJsonObject condition;
};
struct ViewSpec {
    int version = 1;
    QString mediaProbe, mediaPrepare, mediaExport;
    QJsonObject mediaPrepareWhen;
    QString kind = "text";
    QString inputTitle = QStringLiteral("输入内容");
    QString hint = QStringLiteral("工具输入");
    QString placeholder = QStringLiteral("在这里输入内容");
    QString initialText;
    QStringList extensions;
    QVector<FieldSpec> fields;
    QJsonObject layout;
    QString error;
};
ViewSpec loadViewSpec(const QString &packageRoot, bool required = false);
