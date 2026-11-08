#pragma once
#include "catalog/package_catalog.h"
#include <QMainWindow>
#include <QMap>
class QListWidget;
class QStackedWidget;
class QPlainTextEdit;
class QComboBox;
class Workbench : public QMainWindow {
    Q_OBJECT
public:
    explicit Workbench(QVector<ToolDefinition> definitions, QWidget *parent = nullptr);
    ~Workbench() override;
    int sessionCount() const;
    QWidget *sessionView(int id) const;
    QPlainTextEdit *editor(int id) const;
    bool isDetached(int id) const;
    int addSession();
    void detachSession(int id);
    void attachSession(int id);
private:
    struct Session {
        QString toolId;
        QWidget *view = nullptr;
        QPlainTextEdit *editor = nullptr;
        QMainWindow *floating = nullptr;
    };
    void openTool(const QString &toolId);
    void activateSession(int id);
    void updateSelector(int activeId);
    QVector<ToolDefinition> definitions;
    QMap<int, Session> sessions;
    QMap<QString, int> recentSessions;
    QListWidget *navigation;
    QStackedWidget *workspace;
    QComboBox *selector;
    QWidget *floatingNotice;
    QString currentTool;
    int nextId = 1;
};
