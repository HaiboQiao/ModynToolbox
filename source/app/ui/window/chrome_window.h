#pragma once
#include <QMainWindow>
class QToolButton;
class ChromeWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit ChromeWindow(QWidget *parent = nullptr);
    void setContentWidget(QWidget *content);
    QWidget *titleBar() const { return bar; }
    QToolButton *maximizeButton() const { return maximize; }
    void initializeNativeFrame();
protected:
    bool nativeEvent(const QByteArray &type, void *message, qintptr *result) override;
    void showEvent(QShowEvent *event) override;
    void changeEvent(QEvent *event) override;
private:
    QWidget *bar = nullptr;
    QToolButton *maximize = nullptr;
    QWidget *frame = nullptr;
    bool nativeFrameReady = false;
};
