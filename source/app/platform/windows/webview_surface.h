#pragma once
#include <QWidget>
#include <QJsonObject>
#include <functional>
#include <memory>
class WebViewSurface : public QWidget {
 Q_OBJECT
public:
 WebViewSurface(QString assets,QString entry,QString profile,QWidget *parent=nullptr);
 ~WebViewSurface() override;
 void initialize();
 void post(const QJsonObject&);
 void evaluate(const QString&,std::function<void(QString)> callback={});
 void capture(const QString&,std::function<void(bool)>);
 void reload();
 void testInput(const QString &method,const QJsonObject &params,std::function<void(bool)> done);
 QString origin() const;
 quint32 browserProcessId() const;
 void allowFile(const QString &token,const QString &path);
signals:
 void message(const QJsonObject&);
 void loaded();
 void failed(const QString&);
protected:
 bool event(QEvent*) override;
 bool eventFilter(QObject*,QEvent*) override;
 void moveEvent(QMoveEvent*) override;
 void resizeEvent(QResizeEvent*) override;
 void showEvent(QShowEvent*) override;
 void focusInEvent(QFocusEvent*) override;
 void hideEvent(QHideEvent*) override;
private:
 struct Impl; std::unique_ptr<Impl> impl;
 void start();
 void synchronize();
 void scheduleSynchronize();
 void drain();
};
