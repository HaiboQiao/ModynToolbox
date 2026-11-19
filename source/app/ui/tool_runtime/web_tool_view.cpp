#include "web_tool_view.h"
#include "platform/windows/webview_surface.h"
#include "core/files.h"
#include "ui/theme/theme.h"
#include "catalog/package_manager.h"
#include <QFutureWatcher>
#include <QtConcurrent>
#include <QApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QClipboard>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QUrl>
#include <QUuid>
#include <QVBoxLayout>
WebToolView::WebToolView(ToolDefinition t,QString root,QString session,QWidget *parent):ToolSurface(parent),tool(std::move(t)),dataRoot(std::move(root)),sessionId(std::move(session)) {
 startupClock.start();
 setObjectName("webToolView");auto *layout=new QVBoxLayout(this);layout->setContentsMargins(0,0,0,0);layout->setSpacing(0);
 auto assets=tool.packageRoot+"/frontend";auto entry=tool.frontend.mid(9);
 surface=new WebViewSurface(assets,entry,dataRoot+"/tools/"+tool.id+"/ui-profiles/"+tool.version,this);layout->addWidget(surface,1);setFocusProxy(surface);
 loadingLabel=new QLabel(QStringLiteral("正在校验工具包…"));loadingLabel->setObjectName("frontendLoading");loadingLabel->setAlignment(Qt::AlignCenter);layout->addWidget(loadingLabel,1);surface->hide();
 auto *recovery=new QWidget;auto *box=new QVBoxLayout(recovery);errorLabel=new QLabel;errorLabel->setWordWrap(true);box->addWidget(errorLabel);auto *reload=new QPushButton(QStringLiteral("重载工具界面"));box->addWidget(reload);layout->addWidget(recovery);recovery->hide();
 connect(reload,&QPushButton::clicked,this,[this,recovery]{loaded=false;recovery->hide();surface->reload();});
 connect(surface,&WebViewSurface::failed,this,[this,recovery](const QString &s){setProperty("frontendFailure",s);errorLabel->setText(s);loadingLabel->hide();surface->hide();recovery->show();});
 connect(surface,&WebViewSurface::loaded,this,[this]{loaded=true;});
 connect(surface,&WebViewSurface::message,this,&WebToolView::receive);rateClock.start();
 auto *validation=new QFutureWatcher<QString>(this);
 connect(validation,&QFutureWatcher<QString>::finished,this,[this,validation,recovery]{
  const auto error=validation->result();validation->deleteLater();
  setProperty("frontendValidationMs",startupClock.elapsed());
  if(error.isEmpty()){loadingLabel->setText(QStringLiteral("正在加载工具界面…"));surface->initialize();}else{loadingLabel->hide();errorLabel->setText(error);recovery->show();}
 });
 validation->setFuture(QtConcurrent::run([definition=tool]{return PackageManager(QFileInfo(definition.packageRoot).absolutePath()).verify(definition);}));

}
WebToolView::~WebToolView(){for(const auto &entry:uploads)QFile::remove(entry.first);}
QJsonObject WebToolView::snapshot(bool include) const {
 QJsonObject out{{"frontend_state_version",tool.frontendStateVersion},{"receipts",receipts},{"last_job",job}};
 if(include){out["ui_state"]=state;out["input"]=backendInput;out["result"]=result;}
 return out;
}
QJsonObject WebToolView::input() const{return backendInput;}
bool WebToolView::restore(const QJsonObject &s){
 if(s.contains("frontend_state_version")&&s.value("frontend_state_version").toInt()!=tool.frontendStateVersion)return false;
 state=s.value("ui_state").toObject();backendInput=s.value("input").toObject();
 for(const auto &v:backendInput.value("files").toArray())grant(v.toString());result=s.value("result").toObject();grantArtifacts(result);receipts=s.value("receipts").toObject();job=s.value("last_job").toObject();currentJob=job.value("id").toString();
 if(QStringList{"preparing","queued","running","cancelling"}.contains(job.value("state").toString())){job["state"]="interrupted";job["message"]=QStringLiteral("上次作业已中断，请确认后重新执行");}
 // Developer automation can supply backend snapshots without imposing a UI model.
 if(!s.contains("ui_state")&&!s.contains("input")&&!s.contains("frontend_state_version"))backendInput=s;
 if(loaded)bootstrap();return true;
}
void WebToolView::send(const QString &event,const QJsonObject &payload){surface->post({{"event",event},{"sequence",QString::number(++sequence)},{"payload",payload}});}
void WebToolView::bootstrap(){
 send("bootstrap",{{"bridgeVersion",1},{"toolId",tool.id},{"packageVersion",tool.version},{"sessionId",sessionId},{"frontendInstanceId",surface->origin()},{"capabilities",QJsonArray::fromStringList(tool.frontendCapabilities)},{"stateVersion",tool.frontendStateVersion},{"state",state},{"input",backendInput},{"result",result},{"job",job},{"draft",draft},{"viewport",QJsonObject{{"width",width()},{"height",height()}}},{"theme",QJsonObject{{"accent",Theme::accent.name()},{"fontSize",qApp->property("modynFontSize").toInt()},{"compact",qApp->property("modynCompact").toBool()}}}});
}
void WebToolView::notifyTheme(const QJsonObject &theme){send("theme",theme);}
void WebToolView::setDraftEnabled(bool on){draft=on;send("draft",{{"enabled",on}});}
void WebToolView::grantArtifacts(const QJsonObject &r){
 // Only explicit backend output files; never infer access to their parent directory.
 for(const auto &value:r.value("artifacts").toArray())if(value.isString()&&QDir::isAbsolutePath(value.toString())&&!allowed(value.toString()))grant(value.toString());
}
void WebToolView::setResult(const QJsonObject &r){result=r;grantArtifacts(r);send("result",r);}
void WebToolView::setJobState(const QString &s,const QString &m,int done,int total,bool cancellable){
 busy=QStringList{"preparing","queued","running","cancelling"}.contains(s);
 job={{"id",currentJob},{"state",s},{"message",m},{"done",done},{"total",total},{"cancellable",cancellable}};send("job",job);
}
void WebToolView::trigger(const QString &action){emit actionRequested(action,backendInput);}
void WebToolView::addPaths(const QStringList &paths){QJsonArray list;for(const auto &p:paths){grant(p);list.append(p);}send("files",{{"paths",list}});}
bool WebToolView::canPark() const{return parkReady&&!busy;}
bool WebToolView::hasVolatileInput() const{return !state.isEmpty()||!backendInput.isEmpty();}
QString WebToolView::resultText() const{return result.value("text").toString();}
QString WebToolView::grant(const QString &p){
 if(!QFileInfo(p).isFile()||Files::reparse(p))return {};
 auto token=QUuid::createUuid().toString(QUuid::Id128);grants[token]=QFileInfo(p).canonicalFilePath();surface->allowFile(token,p);return surface->origin().replace("https://","https://media.")+"/__file/"+token;
}
bool WebToolView::allowed(const QString &p) const {
 auto canonical=QFileInfo(p).canonicalFilePath();if(canonical.isEmpty())return false;
 if(grants.values().contains(canonical))return true;
 for(const auto &root:directoryGrants)if(canonical==root||canonical.startsWith(root+"/",Qt::CaseInsensitive))return !Files::reparse(p);
 const auto root=QDir(dataRoot+"/tools/"+tool.id).canonicalPath();return !root.isEmpty()&&canonical.startsWith(root+"/",Qt::CaseInsensitive)&&!Files::reparse(p);
}
void WebToolView::receive(const QJsonObject &m){
 if(rateClock.elapsed()>1000){rateClock.restart();messages=0;}++messages;
 auto request=m.value("requestId").toString();auto method=m.value("method").toString();auto p=m.value("payload").toObject();
 if(m.value("bridgeVersion").toInt()!=1||request.isEmpty()||request.size()>100)return;
 auto reply=[this,request](QJsonObject value,QString error={}){surface->post({{"requestId",request},{"result",value},{"error",error}});};

 QString capability;
 if(method.startsWith("jobs."))capability="jobs";
 else if(method.startsWith("files.")||method=="artifacts.read"||method=="artifacts.open")capability="files";
 else if(method=="artifacts.save")capability="export";
 else if(method.startsWith("clipboard."))capability="clipboard";
 else if(method.startsWith("window."))capability="window";
 else if(method.startsWith("session."))capability="state";
 if(!capability.isEmpty()&&!tool.frontendCapabilities.contains(capability)){reply({},"capability not declared: "+capability);return;}
 if(messages>1000 || (messages>100 && !method.startsWith("payload."))){reply({},"message rate limit");return;}
 if(method=="payload.begin"){
  const auto bytes=p.value("bytes").toInteger();if(bytes<1||bytes>16*1024*1024||uploads.size()>=4){reply({},"payload budget exceeded");return;}
  auto token=QUuid::createUuid().toString(QUuid::Id128);auto root=dataRoot+"/tools/"+tool.id+"/ui-transfer";QDir().mkpath(root);auto path=root+"/"+token;
  QFile file(path);if(!file.open(QIODevice::WriteOnly)){reply({},"cannot create transfer");return;}uploads[token]={path,bytes};
  QTimer::singleShot(120000,this,[this,token]{if(uploads.contains(token)){QFile::remove(uploads.take(token).first);}});reply({{"token",token}});return;
 }
 if(method=="payload.append"){
  auto token=p.value("token").toString();if(!uploads.contains(token)){reply({},"transfer missing");return;}auto entry=uploads.value(token);auto bytes=QByteArray::fromBase64(p.value("base64").toString().toLatin1());
  QFile file(entry.first);if(bytes.size()>65536||QFileInfo(file).size()+bytes.size()>entry.second||!file.open(QIODevice::Append)||file.write(bytes)!=bytes.size()){reply({},"invalid transfer chunk");return;}reply({});return;
 }
 if(method=="payload.dispatch"){
  auto token=p.value("token").toString(),target=p.value("method").toString();if(!uploads.contains(token)||target.startsWith("payload.")){reply({},"transfer missing or invalid method");return;}
  auto entry=uploads.take(token);if(QFileInfo(entry.first).size()!=entry.second){QFile::remove(entry.first);reply({},"incomplete transfer");return;}
  auto *reader=new QFutureWatcher<QJsonObject>(this);
  connect(reader,&QFutureWatcher<QJsonObject>::finished,this,[this,reader,target,request]{auto payload=reader->result();reader->deleteLater();receive({{"bridgeVersion",1},{"requestId",request},{"method",target},{"payload",payload}});});
  reader->setFuture(QtConcurrent::run([path=entry.first]{auto value=Files::readObject(path,16*1024*1024);QFile::remove(path);return value;}));return;
 }
 if(method=="ready"){loaded=true;loadingLabel->hide();surface->show();setProperty("frontendReadyMs",startupClock.elapsed());bootstrap();reply({{"ready",true}});emit ready();return;}
 if(method=="session.checkpoint"){
  if(QJsonDocument(p).toJson(QJsonDocument::Compact).size()>16*1024*1024){reply({},"checkpoint too large");return;}
  parkReady=false;state=p.value("state").toObject();backendInput=p.value("input").toObject();checkpoint=true;emit changed();reply({{"saved",true}});
  const auto nonce=p.value("nonce").toString();if(flushes.contains(nonce)){auto callback=flushes.take(nonce);callback(true);}return;
 }
 if(method=="session.dirty"){parkReady=false;checkpoint=false;reply({});return;}
 if(method=="jobs.query"){reply({{"job",job},{"result",result}});return;}
 if(method=="jobs.cancel"){emit cancelRequested();reply({{"requested",true}});return;}
 if(method=="jobs.submit"){
  auto key=p.value("key").toString();if(key.isEmpty()||key.size()>100){reply({},"idempotency key required");return;}
  auto fingerprint=QString::fromLatin1(QCryptographicHash::hash(QJsonDocument(QJsonObject{{"action",p.value("action")},{"input",p.value("input")}}).toJson(QJsonDocument::Compact),QCryptographicHash::Sha256).toHex());
  if(receipts.contains(key)){auto previous=receipts.value(key).toObject();if(previous.value("fingerprint")!=QJsonValue(fingerprint)){reply({},"idempotency key reused with different input");return;}reply({{"jobId",previous.value("jobId")}});return;}
  if(busy){reply({},"session already has a running job");return;}
  bool known=false;for(const auto &a:tool.actions)known|=a.id==p.value("action").toString();if(!known){reply({},"unknown action");return;}
  backendInput=p.value("input").toObject();checkpoint=false;emit changed();
  auto prior=currentJob;currentJob.clear();emit actionRequested(p.value("action").toString(),backendInput);
  if(currentJob.isEmpty()){currentJob=prior;reply({},"job was not submitted");return;}
  QJsonObject accepted{{"jobId",currentJob}};receipts[key]=QJsonObject{{"jobId",currentJob},{"fingerprint",fingerprint}};while(receipts.size()>32)receipts.remove(receipts.keys().first());reply(accepted);return;
 }
 if(method=="files.drop"){
  QJsonArray files;for(const auto &v:m.value("_nativeFiles").toArray()){auto path=v.toString();files.append(QJsonObject{{"path",path},{"name",QFileInfo(path).fileName()},{"bytes",QFileInfo(path).size()},{"url",grant(path)}});}reply({{"files",files}});return;
 }
 if(method=="files.pick"){
  QStringList paths;
  if(p.value("directory").toBool()){auto path=QFileDialog::getExistingDirectory(this,QStringLiteral("选择目录"));if(!path.isEmpty())paths<<path;}
  else paths=QFileDialog::getOpenFileNames(this,QStringLiteral("选择文件"),{},p.value("filter").toString());
  QJsonArray files;for(const auto &path:paths){if(QFileInfo(path).isDir())directoryGrants.append(QFileInfo(path).canonicalFilePath());files.append(QJsonObject{{"path",path},{"name",QFileInfo(path).fileName()},{"bytes",QFileInfo(path).size()},{"url",grant(path)}});}reply({{"files",files}});return;
 }
 if(method=="files.url"){
  auto path=p.value("path").toString();if(!allowed(path)){reply({},"file has not been granted");return;}reply({{"url",grant(path)}});return;
 }
 if(method=="artifacts.read"){
  auto path=p.value("path").toString();if(!allowed(path)){reply({},"file has not been granted");return;}QFile file(path);
  if(!file.open(QIODevice::ReadOnly)){reply({},"file unavailable");return;}auto offset=p.value("offset").toInteger();auto bytes=qBound(1,p.value("bytes").toInt(32768),65536);if(offset<0||!file.seek(offset)){reply({},"invalid offset");return;}
  auto data=file.read(bytes);reply({{"base64",QString::fromLatin1(data.toBase64())},{"offset",offset},{"bytes",data.size()},{"size",file.size()}});return;
 }
 if(method=="clipboard.write"){auto text=p.value("text").toString();if(text.toUtf8().size()>5*1024*1024){reply({},"clipboard too large");return;}QApplication::clipboard()->setText(text);reply({});return;}
 if(method=="artifacts.open"){
  auto path=p.value("path").toString();if(!allowed(path)){reply({},"file has not been granted");return;}QDesktopServices::openUrl(QUrl::fromLocalFile(path));reply({});return;
 }
 if(method=="artifacts.save"){
  auto path=QFileDialog::getSaveFileName(this,QStringLiteral("导出结果"));if(path.isEmpty()){reply({},"cancelled");return;}
  auto text=p.value("text").toString();emit exportRequested(path,{{"text",text}},true);reply({{"requested",true}});return;
 }
 if(method=="window.detach"){emit detachRequested();reply({});return;}
 if(method=="window.attach"){emit attachRequested();reply({});return;}
 reply({},"unsupported bridge method");
}

void WebToolView::flush(std::function<void(bool)> done){
 if(!loaded){done(false);return;}auto nonce=QUuid::createUuid().toString(QUuid::Id128);flushes[nonce]=std::move(done);
 send("checkpoint-request",{{"nonce",nonce}});
 QTimer::singleShot(5000,this,[this,nonce]{if(flushes.contains(nonce)){auto callback=flushes.take(nonce);callback(false);}});
}
void WebToolView::requestPark(){
 if(parkPending||busy||!loaded)return;parkPending=true;
 flush([this](bool ok){parkPending=false;parkReady=ok;if(ok)emit ready();});
}
