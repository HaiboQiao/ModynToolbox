#pragma once
#include <QObject>
#include <QString>
#include <functional>
#include <memory>
// One normal workbench per current Windows user/session. Test scopes never address the production broker.
class SingleInstance : public QObject {
public:
    enum class Result { Primary, Secondary, Unresponsive, Error };
    explicit SingleInstance(QString scope = {}, QObject *parent = nullptr);
    ~SingleInstance() override;
    Result acquire(bool activateExisting = true);
    void setActivationHandler(std::function<void()> handler);
    QString errorString() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl;
    void activationRequested();
};
class QWidget;
void restoreWorkbenchWindow(QWidget *window);
