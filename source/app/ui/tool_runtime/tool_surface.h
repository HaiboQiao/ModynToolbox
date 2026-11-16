#pragma once
#include <QWidget>
#include <QJsonObject>
#include <functional>
class QPlainTextEdit;
// A session owns a surface, not any particular business widget hierarchy.
class ToolSurface : public QWidget {
 Q_OBJECT
public:
 using QWidget::QWidget;
 virtual QJsonObject snapshot(bool includeInput=true) const=0;
 virtual QJsonObject input() const=0;
 virtual bool restore(const QJsonObject&)=0;
 virtual void setResult(const QJsonObject&)=0;
 virtual void setJobState(const QString&,const QString&,int,int,bool)=0;
 virtual void addPaths(const QStringList&) {}
 virtual void trigger(const QString&)=0;
 virtual bool canPark() const=0;
 virtual bool hasVolatileInput() const=0;
 virtual QPlainTextEdit *editor() const {return nullptr;}
 virtual QString resultText() const {return {};}
 virtual void setDraftEnabled(bool) {}
 virtual void notifyTheme(const QJsonObject&) {}
 virtual void requestPark() {}
 virtual void flush(std::function<void(bool)> done) {done(true);}
 virtual void setJobIdentity(const QString&) {}
 virtual bool opaqueState() const {return false;}
signals:
 void changed();
 void ready();
 void actionRequested(const QString&,const QJsonObject&);
 void cancelRequested();
 void exportRequested(const QString&,const QJsonObject&,bool);
 void detachRequested();
 void attachRequested();
};
