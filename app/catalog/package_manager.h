#pragma once
#include "package_catalog.h"
class PackageManager {
  public:
    explicit PackageManager(QString root);
    QString install(const QString &source);
    QString remove(const ToolDefinition &tool, bool referenced);
    QString verify(const ToolDefinition &tool) const;
    QVector<ToolDefinition> versions(QStringList *errors = nullptr) const;
    QString root() const {
        return packagesRoot;
    }

  private:
    QString packagesRoot;
};
