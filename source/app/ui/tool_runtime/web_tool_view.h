#pragma once
#include "tool_surface.h"
#include "catalog/package_catalog.h"
#include <QElapsedTimer>
class WebViewSurface;
class QLabel;
class WebToolView : public ToolSurface {
 Q_OBJECT
public:
 WebToolView(ToolDefinition,QString dataRoot,QString session,QWidget *parent=nullptr);
 ~WebToolView() override;
 QJsonObject snapshot(bool includeInput=true) const override;
 QJsonObject input() const override;
 bool restore(const QJsonObject&) override;
 void setResult(const QJsonObject&) override;
 void setJobState(const QString&,const QString&,int,int,bool) override;
 void trigger(const QString&) override;
 void addPaths(const QStringList&) override;
 bool canPark() const override;
 bool hasVolatileInput() const override;
 QString resultText() const override;
 void requestPark() override;
 void flush(std::function<void(bool)>) override;
 bool opaqueState() const override {return true;}
 void setDraftEnabled(bool) override;
 void notifyTheme(const QJsonObject&) override;
 void setJobIdentity(const QString &id) override {currentJob=id;}
 WebViewSurface *browser() const {return surface;}
 QJsonObject jobSummary() const {return job;}
private:
 void receive(const QJsonObject&);
 void send(const QString&,const QJsonObject&);
 void bootstrap();
 void grantArtifacts(const QJsonObject &);
 QString grant(const QString&);
 bool allowed(const QString&) const;
 ToolDefinition tool;
 QString dataRoot,sessionId,currentJob;
 WebViewSurface *surface=nullptr;QLabel *errorLabel=nullptr;
 QLabel *loadingLabel=nullptr;QElapsedTimer startupClock;
 QJsonObject state,backendInput,result,job,receipts;
 QMap<QString,QString> grants;
 QMap<QString,QPair<QString,qint64>> uploads;
 QStringList directoryGrants;
 QMap<QString,std::function<void(bool)>> flushes;
 bool parkReady=false,parkPending=false;
 bool draft=false,checkpoint=false,loaded=false,busy=false;
 QElapsedTimer rateClock;int messages=0;quint64 sequence=0;
};
