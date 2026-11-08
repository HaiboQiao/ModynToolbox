#pragma once
#include "job_runner.h"
#include <QHash>
#include <QSet>
#include <QObject>
#include <QThreadPool>
#include <QDateTime>
struct JobRecord {
    JobRequest request;
    QJsonObject input, result;
    QString state = "preparing", message;
    QStringList resources;
    bool cancellable = true;
    bool leased = true;
    qint64 created = QDateTime::currentMSecsSinceEpoch();
    int completed = 0, total = 0;
};
class Scheduler : public QObject {
    Q_OBJECT
  public:
    Scheduler(QString dataRoot, QString bridge, QObject *parent = nullptr);
    ~Scheduler() override;
    QString submit(const ToolDefinition &tool, const QString &session, int revision,
                   const QString &action, const QJsonObject &input,
                   JobKind kind = JobKind::External);
    QString submitExport(const ToolDefinition &tool, const QString &session, int revision,
                         const QString &path, const QJsonObject &document, bool plainText);
    void cancel(const QString &id);
    bool busy() const;
    void setBatchLimit(int limit);
    bool critical() const;
    int activeCount() const;
    const QMap<QString, JobRecord> &records() const {
        return jobs;
    }
    JobRecord record(const QString &id) const {
        if (jobs.contains(id))
            return jobs.value(id);
        JobRecord expired;
        expired.state = "expired";
        expired.message = QStringLiteral("运行记录已过保留范围，会话中的结果仍然保留");
        return expired;
    }
  signals:
    void updated(const QString &id);
    void finished(const QString &session, const QString &id, int revision,
                  const QJsonObject &result, const QString &outcome);

  private:
    void dispatch();
    void grantReservations();
    void finish(const QString &id, const QJsonObject &result, const QString &outcome);
    bool compatible(const JobRecord &candidate) const;
    QString dataRoot, bridge;
    QMap<QString, JobRecord> jobs;
    QHash<QString, JobRunner *> running;
    QList<QString> queue;
    QSet<QString> reservations;
    QThreadPool preparation;
    int preparingCount = 0;
    int batchLimit = 1;
};
