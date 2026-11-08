#pragma once
#include <QString>
#include <QStringList>
#include <QVector>
struct FieldSpec {
    QString id;
    QString label;
    QString type;
    QStringList options;
    QString value;
    int number = 0;
    int minimum = 0;
    int maximum = 100;
};
struct ViewSpec {
    QString kind = "text";
    QString inputTitle = QStringLiteral("输入内容");
    QString hint = QStringLiteral("编辑区接入预览");
    QString placeholder = QStringLiteral("在这里输入内容，处理后端将在对应阶段接入。");
    QString initialText;
    QStringList extensions;
    QVector<FieldSpec> fields;
    QString error;
};
ViewSpec loadViewSpec(const QString &packageRoot, bool required = false);
