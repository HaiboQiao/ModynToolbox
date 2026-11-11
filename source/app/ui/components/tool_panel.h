#pragma once
#include "catalog/package_catalog.h"
#include "ui/view_spec.h"
#include <QJsonObject>
#include <QMap>
#include <QWidget>
#include <QElapsedTimer>
class VideoEditor;
class QLabel;
class FullTextEdit;
class QScrollArea;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;
class QTableWidget;
class QTimer;
class ToolPanel : public QWidget {
    Q_OBJECT
  public:
    ToolPanel(ToolDefinition tool, ViewSpec spec, QWidget *parent = nullptr);
    QJsonObject snapshot(bool includeInput = true) const;
    QJsonObject input() const;
    bool restore(const QJsonObject &state);
    void setResult(const QJsonObject &result);
    void setJobState(const QString &state, const QString &message, int done, int total,
                     bool cancellable);
    void addPaths(const QStringList &paths);
    void trigger(const QString &action);
    bool canPark() const {
        return !scanning;
    }
    bool hasVolatileInput() const;
    QPlainTextEdit *editor() const;
    QString resultText() const;
  signals:
    void changed();
    void ready();
    void actionRequested(const QString &action, const QJsonObject &input);
    void cancelRequested();
    void exportRequested(const QString &path, const QJsonObject &document, bool plainText);

  private:
    QWidget *node(const QJsonObject &description);
    QWidget *makeFields();
    QWidget *makeFiles();
    QWidget *makeText();
    QWidget *makeActions();
    QJsonObject parameters() const;
    void inputChanged();
    void conditions();
    void updateActions();
    void verifyRestoredFiles();
    void appendRecords(const QJsonArray &records, bool replace = false);
    void loadText();
    void setStatusText(const QString &text);
    void exportText();
    void writeExport(const QString &path, const QJsonObject &document, bool plainText);
    ToolDefinition tool;
    ViewSpec spec;
    QMap<QString, QWidget *> fields, fieldContainers;
    QMap<QString, QPushButton *> actions;
    QVector<QWidget *> frozenInputs;
    VideoEditor *videoEditor = nullptr;
    FullTextEdit *textInput = nullptr;
    QPlainTextEdit *textResult = nullptr;
    QScrollArea *bodyScroll = nullptr;
    QTableWidget *fileTable = nullptr, *resultTable = nullptr;
    QLabel *status = nullptr, *preview = nullptr, *fileNotice = nullptr;
    QProgressBar *progress = nullptr;
    QPushButton *cancelButton = nullptr;
    QLabel *elapsedLabel = nullptr;
    QElapsedTimer jobClock;
    QJsonObject context, lastResult, pendingMediaSelection;
    bool restoring = false, scanning = false, busy = false;
};
