#pragma once
#include "catalog/package_catalog.h"
#include "ui/window/chrome_window.h"
#include <QMap>
#include <QSet>
#include <QJsonObject>
#include <memory>
class ToolNavigation;
class QMenu;
class QStackedWidget;
class QPlainTextEdit;
class QComboBox;
class QSplitter;
class ToolPanel;
class Scheduler;
class QPushButton;
class StateRepository;
struct WorkbenchConfig {
    QString dataRoot, packagesRoot, bridge;
};
class Workbench : public ChromeWindow {
    Q_OBJECT
  public:
    explicit Workbench(QVector<ToolDefinition> definitions, QWidget *parent = nullptr);
    Workbench(QVector<ToolDefinition> definitions, WorkbenchConfig config,
              QWidget *parent = nullptr);
    ~Workbench() override;
    int sessionCount() const;
    QWidget *sessionView(int id) const;
    QPlainTextEdit *editor(int id) const;
    bool isDetached(int id) const;
    int addSession();
    int open(const QString &toolId, bool fresh = false);
    void detachSession(int id);
    void attachSession(int id);
    void pairSession(int id);
    void closeSession(int id, bool confirmed = false);
    bool refreshTools();
    QString installPackage(const QString &folder);
    QString removePackage(const QString &id, const QString &version);
    ToolPanel *panel(int id) const;
    Scheduler *jobScheduler() const {
        return scheduler;
    }
    QJsonObject savedState() const;
    bool saveState();
    QString storageError() const;
    void runAction(int id, const QString &action, const QJsonObject &input, bool confirmed = false);
    void setDraft(int id, bool enabled);

  protected:
    void closeEvent(QCloseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

  private:
    struct Session {
        QString toolId, uuid, job, title;
        ToolDefinition tool;
        QWidget *view = nullptr;
        ToolPanel *panel = nullptr;
        ChromeWindow *floating = nullptr;
        QJsonObject parked;
        QByteArray geometry;
        int revision = 0;
        bool draft = false, missing = false, customTitle = false, preserveSnapshot = false;
    };
    void openTool(const QString &toolId);
    void activateSession(int id);
    void syncNavigation(int selected);
    QMenu *sessionMenu(int id);
    void showNavigationMenu(const QString &tool, int id, const QPoint &position);
    void ensureView(int id);
    void parkUnused();
    void initializeServices();
    void showManager();
    void showHistory();
    void showPreferences();
    void applyPreferences();
    void renameSession(int id);
    void rebuildNavigation();
    void filterNavigation();
    void updatePairVisibility();
    void updateActivity();
    void transferData(bool importing);
    void showSessions();
    QVector<ToolDefinition> definitions;
    QMap<int, Session> sessions;
    QMap<QString, int> recentSessions;
    QMap<QString, qint64> persistedAt;
    ToolNavigation *navigation = nullptr;
    QStackedWidget *workspace = nullptr, *secondary = nullptr;
    QComboBox *categories = nullptr;
    QSplitter *splitter = nullptr, *navigationSplit = nullptr;
    QPushButton *activity = nullptr;
    QJsonObject preferences, navigationExpansion;
    QSet<int> pendingSessionClose;
    int unreadErrors = 0, navigationWidth = 190;
    qint64 historyReviewedAt = 0;
    QWidget *floatingNotice = nullptr;
    QString currentTool;
    int nextId = 1, activeId = 0, pairedId = 0;
    WorkbenchConfig config;
    Scheduler *scheduler = nullptr;
    std::unique_ptr<StateRepository> repository;
    bool restoring = false, closing = false, waitingExit = false;
};
