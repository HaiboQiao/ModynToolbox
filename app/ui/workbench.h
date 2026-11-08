#pragma once
#include <QMainWindow>
#include <QMap>
class QListWidget;
class QLineEdit;
class QStackedWidget;
class QPlainTextEdit;
class Workbench : public QMainWindow {
    Q_OBJECT
public:
    explicit Workbench(QWidget *parent = nullptr);
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
        QWidget *view = nullptr;
        QPlainTextEdit *editor = nullptr;
        QMainWindow *floating = nullptr;
    };
    QMap<int, Session> sessions;
    QListWidget *navigation;
    QStackedWidget *workspace;
    int nextId = 1;
};
