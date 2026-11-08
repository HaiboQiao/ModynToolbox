#pragma once
#include <QString>
#include <QStringList>
#include <QVector>
struct ToolDefinition {
    QString id;
    QString name;
    QString version;
    QString description;
    QString packageRoot;
};
struct CatalogResult {
    QVector<ToolDefinition> tools;
    QStringList errors;
};
CatalogResult readCatalog(const QString &root);
