#pragma once
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVector>
struct ActionSpec {
    QString id, label, confirmation, requiredContext;
    bool batch = false;
    bool dynamicResources = false;
    bool writeSources = false;
    bool cancellable = true;
    int timeoutSeconds = 600;
};
struct ToolDefinition {
    QString id, name, version, description, packageRoot;
    QString icon = "files";
    QString viewFile;
    bool executable = false;
    QString entryProgram;
    QStringList entryArguments;
    QVector<ActionSpec> actions;
    QJsonObject limits;
    QString category = QStringLiteral("其他");
    QStringList keywords;
};
struct CatalogResult {
    QVector<ToolDefinition> tools;
    QStringList errors;
};
ToolDefinition definitionFromManifest(const QString &root, const QJsonObject &manifest,
                                      QString *error);
QVector<ToolDefinition> readPackages(const QString &root, QStringList *errors = nullptr);
CatalogResult readCatalog(const QString &root);
bool versionGreater(const QString &left, const QString &right);
