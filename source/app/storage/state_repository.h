#pragma once
#include <QJsonObject>
#include <QJsonArray>
#include <QSqlDatabase>
struct JobRecord;
class StateRepository {
  public:
    explicit StateRepository(const QString &root);
    ~StateRepository();
    bool open();
    void close();
    QString error() const {
        return failure;
    }
    QJsonObject state() const;
    bool save(const QJsonObject &state);
    void job(const JobRecord &record, const QString &sessionTitle = {});
    QJsonArray history() const;
    QString root() const {
        return dataRoot;
    }

  private:
    QString dataRoot, connection, failure;
    QSqlDatabase database;
    bool ready = false;
};
