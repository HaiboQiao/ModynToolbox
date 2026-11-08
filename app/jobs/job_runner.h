#pragma once
#include "catalog/package_catalog.h"
#include <QJsonObject>
#include <QThread>
#include <atomic>
enum class JobKind { External, FileExport };
struct JobRequest {
    QString jobId, sessionId, jobRoot, dataRoot, bridge;
    ToolDefinition tool;
    ActionSpec action;
    int revision = 0;
    JobKind kind = JobKind::External;
};
class JobRunner : public QThread {
    Q_OBJECT
  public:
    explicit JobRunner(JobRequest request, QObject *parent = nullptr);
    void grantResources() {
        granted.store(true);
    }
    void cancel() {
        cancellation.store(true);
    }
  signals:
    void reservation(const QString &jobId, const QStringList &keys);
    void progress(const QString &jobId, const QJsonObject &event);
    void completed(const QString &jobId, const QJsonObject &result, const QString &outcome);

  protected:
    void run() override;

  private:
    QJsonObject runExport();
    JobRequest request;
    std::atomic_bool cancellation = false;
    std::atomic_bool granted = false;
};
