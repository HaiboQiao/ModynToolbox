#include "webview_surface.h"
#include "core/files.h"
#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QEvent>
#include <QFocusEvent>
#include <QKeyEvent>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonArray>
#include <QPointer>
#include <QQueue>
#include <QRegularExpression>
#include <QTimer>
#include <QUuid>
#include <QPlatformSurfaceEvent>
#include <QMoveEvent>
#define NOMINMAX
#include <windows.h>
#include <wrl.h>
#include <WebView2.h>
#include <shlwapi.h>
using namespace Microsoft::WRL;
namespace {
// Bounded file stream: large media stays outside JSON and outside the GUI heap.
class RangeStream final : public RuntimeClass<RuntimeClassFlags<ClassicCom>,IStream,FtmBase> {
 HANDLE file=INVALID_HANDLE_VALUE; ULONGLONG begin=0,length=0,position=0;
public:
 HRESULT open(const QString &path,ULONGLONG start,ULONGLONG size) {
  file=CreateFileW((LPCWSTR)path.utf16(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
  begin=start;length=size;return file==INVALID_HANDLE_VALUE?HRESULT_FROM_WIN32(GetLastError()):S_OK;
 }
 ~RangeStream(){if(file!=INVALID_HANDLE_VALUE)CloseHandle(file);}
 HRESULT STDMETHODCALLTYPE Read(void *target,ULONG count,ULONG *read) override {
  LARGE_INTEGER offset;offset.QuadPart=begin+position;SetFilePointerEx(file,offset,nullptr,FILE_BEGIN);
  DWORD got=0;auto want=(DWORD)qMin<ULONGLONG>(count,length-position);
  if(!ReadFile(file,target,want,&got,nullptr))return HRESULT_FROM_WIN32(GetLastError());
  position+=got;if(read)*read=got;return got==count?S_OK:S_FALSE;
 }
 HRESULT STDMETHODCALLTYPE Seek(LARGE_INTEGER move,DWORD origin,ULARGE_INTEGER *where) override {
  LONGLONG next=(origin==STREAM_SEEK_SET?0:origin==STREAM_SEEK_CUR?position:length)+move.QuadPart;
  if(next<0||(ULONGLONG)next>length)return STG_E_INVALIDFUNCTION;position=next;if(where)where->QuadPart=position;return S_OK;
 }
 HRESULT STDMETHODCALLTYPE Stat(STATSTG *s,DWORD) override {ZeroMemory(s,sizeof(*s));s->type=STGTY_STREAM;s->cbSize.QuadPart=length;return S_OK;}
 HRESULT STDMETHODCALLTYPE Write(const void*,ULONG,ULONG*) override{return STG_E_ACCESSDENIED;}
 HRESULT STDMETHODCALLTYPE SetSize(ULARGE_INTEGER) override{return E_NOTIMPL;}
 HRESULT STDMETHODCALLTYPE CopyTo(IStream*,ULARGE_INTEGER,ULARGE_INTEGER*,ULARGE_INTEGER*) override{return E_NOTIMPL;}
 HRESULT STDMETHODCALLTYPE Commit(DWORD) override{return S_OK;}
 HRESULT STDMETHODCALLTYPE Revert() override{return E_NOTIMPL;}
 HRESULT STDMETHODCALLTYPE LockRegion(ULARGE_INTEGER,ULARGE_INTEGER,DWORD) override{return E_NOTIMPL;}
 HRESULT STDMETHODCALLTYPE UnlockRegion(ULARGE_INTEGER,ULARGE_INTEGER,DWORD) override{return E_NOTIMPL;}
 HRESULT STDMETHODCALLTYPE Clone(IStream**) override{return E_NOTIMPL;}
};
QString mime(const QString &path) {
 auto ext=QFileInfo(path).suffix().toLower();
 if(ext=="mp4"||ext=="m4v")return "video/mp4";if(ext=="webm")return "video/webm";
 if(ext=="png")return "image/png";if(ext=="jpg"||ext=="jpeg")return "image/jpeg";if(ext=="webp")return "image/webp";
 return "application/octet-stream";
}
}
struct WebViewSurface::Impl {
 QQueue<QString> outbound;qint64 queuedChars=0;bool draining=false;
 QString assets,entry,profile,host; QMap<QString,QString> files;
 ComPtr<ICoreWebView2Environment> environment;ComPtr<ICoreWebView2Controller> controller;ComPtr<ICoreWebView2Controller3> scaling;ComPtr<ICoreWebView2> web;
 double appliedScale=0;
 HWND parking=nullptr,controllerParent=nullptr;
 RECT appliedBounds{-1,-1,-1,-1};POINT appliedOrigin{LONG_MIN,LONG_MIN};
 bool syncQueued=false,visibilityKnown=false,appliedVisible=false;
 QPointer<QWidget> observedWindow;
 bool started=false,processDead=false,comInitialized=false;
};
WebViewSurface::WebViewSurface(QString assets,QString entry,QString profile,QWidget *parent):QWidget(parent),impl(std::make_unique<Impl>()) {
 impl->assets=std::move(assets);impl->entry=std::move(entry);impl->profile=std::move(profile);
 impl->host="s"+QUuid::createUuid().toString(QUuid::Id128)+".modyn.invalid";
 setAttribute(Qt::WA_NativeWindow);setAttribute(Qt::WA_NoSystemBackground);setFocusPolicy(Qt::StrongFocus);

}
WebViewSurface::~WebViewSurface(){if(impl->controller)impl->controller->Close();impl->web.Reset();impl->scaling.Reset();impl->controller.Reset();if(impl->parking)DestroyWindow(impl->parking);impl->environment.Reset();if(impl->comInitialized)CoUninitialize();}
QString WebViewSurface::origin() const{return "https://"+impl->host;}
void WebViewSurface::initialize(){start();}
void WebViewSurface::start() {
 if(impl->started)return;impl->started=true;
 if(QGuiApplication::platformName()=="offscreen"){emit failed(QStringLiteral("完整前端需要原生WebView2窗口；离屏Qt仅用于旧界面测试"));return;}
 const auto co=impl->comInitialized?S_FALSE:CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
 if(FAILED(co)){emit failed(QStringLiteral("浏览器STA线程初始化失败"));return;}
 impl->comInitialized=true;
 QDir().mkpath(impl->profile);
 if(!impl->parking)impl->parking=CreateWindowExW(0,L"STATIC",L"ModynWebParking",WS_POPUP,0,0,1,1,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
 QString runtime=qEnvironmentVariable("MODYN_WEBVIEW2_RUNTIME");
 if(runtime.isEmpty()){auto bundled=QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("../runtime/webview2");if(QFileInfo::exists(bundled+"/msedgewebview2.exe"))runtime=bundled;}
 if(runtime.isEmpty()&&QFileInfo::exists(QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("../release-metadata.json"))){emit failed(QStringLiteral("完整便携包的浏览器运行时缺失，请重新完整解压"));return;}
 if(!runtime.isEmpty())runtime=QDir::toNativeSeparators(QFileInfo(runtime).canonicalFilePath());
 impl->profile=QDir::toNativeSeparators(QFileInfo(impl->profile).absoluteFilePath());
 QPointer<WebViewSurface> self(this);
 auto hr=CreateCoreWebView2EnvironmentWithOptions(runtime.isEmpty()?nullptr:(LPCWSTR)runtime.utf16(),(LPCWSTR)impl->profile.utf16(),nullptr,
 Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>([self](HRESULT hr,ICoreWebView2Environment *env)->HRESULT {
  if(!self)return S_OK;if(FAILED(hr)){emit self->failed(QStringLiteral("WebView2运行时初始化失败：%1").arg((quint32)hr,8,16,QChar('0')));return S_OK;}
  self->impl->environment=env;
  return env->CreateCoreWebView2Controller((HWND)self->winId(),Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>([self](HRESULT hr,ICoreWebView2Controller *controller)->HRESULT {
   if(!self){if(controller)controller->Close();return S_OK;}
   if(FAILED(hr)){emit self->failed(QStringLiteral("浏览器容器创建失败"));return S_OK;}
   auto &i=*self->impl;i.controller=controller;i.controllerParent=(HWND)self->winId();i.appliedBounds={-1,-1,-1,-1};i.visibilityKnown=false;i.appliedOrigin={LONG_MIN,LONG_MIN};i.appliedScale=0;controller->QueryInterface(IID_PPV_ARGS(&i.scaling));if(i.scaling){i.scaling->put_ShouldDetectMonitorScaleChanges(FALSE);i.scaling->put_BoundsMode(COREWEBVIEW2_BOUNDS_MODE_USE_RAW_PIXELS);}controller->get_CoreWebView2(&i.web);
   ComPtr<ICoreWebView2_3> local;i.web.As(&local);
   if(!local){emit self->failed(QStringLiteral("浏览器版本不支持本地资源映射"));return S_OK;}
   local->SetVirtualHostNameToFolderMapping((LPCWSTR)i.host.utf16(),(LPCWSTR)i.assets.utf16(),COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_DENY_CORS);
   ComPtr<ICoreWebView2Settings> settings;i.web->get_Settings(&settings);settings->put_AreHostObjectsAllowed(FALSE);settings->put_IsStatusBarEnabled(FALSE);settings->put_AreDefaultScriptDialogsEnabled(FALSE);
   ComPtr<ICoreWebView2Settings4> privacy;settings.As(&privacy);if(privacy){privacy->put_IsGeneralAutofillEnabled(FALSE);privacy->put_IsPasswordAutosaveEnabled(FALSE);}
   EventRegistrationToken token;
   i.web->add_NavigationStarting(Callback<ICoreWebView2NavigationStartingEventHandler>([self](ICoreWebView2*,ICoreWebView2NavigationStartingEventArgs *a)->HRESULT {
    if(!self)return S_OK;LPWSTR raw=nullptr;a->get_Uri(&raw);QString url=QString::fromWCharArray(raw?raw:L"");CoTaskMemFree(raw);
    if(!url.startsWith(self->origin()+"/"))a->put_Cancel(TRUE);return S_OK;
   }).Get(),&token);
   i.web->add_NewWindowRequested(Callback<ICoreWebView2NewWindowRequestedEventHandler>([](ICoreWebView2*,ICoreWebView2NewWindowRequestedEventArgs *a)->HRESULT{a->put_Handled(TRUE);return S_OK;}).Get(),&token);
   i.web->add_PermissionRequested(Callback<ICoreWebView2PermissionRequestedEventHandler>([](ICoreWebView2*,ICoreWebView2PermissionRequestedEventArgs *a)->HRESULT{a->put_State(COREWEBVIEW2_PERMISSION_STATE_DENY);return S_OK;}).Get(),&token);
   i.web->add_WebMessageReceived(Callback<ICoreWebView2WebMessageReceivedEventHandler>([self](ICoreWebView2*,ICoreWebView2WebMessageReceivedEventArgs *a)->HRESULT {
    if(!self)return S_OK;LPWSTR source=nullptr;a->get_Source(&source);QString url=QString::fromWCharArray(source?source:L"");CoTaskMemFree(source);
    if(!url.startsWith(self->origin()+"/"))return S_OK;
    LPWSTR raw=nullptr;a->get_WebMessageAsJson(&raw);QString text=QString::fromWCharArray(raw?raw:L"");CoTaskMemFree(raw);
    if(text.toUtf8().size()>256*1024){emit self->failed(QStringLiteral("前端消息超过256KiB限制"));return S_OK;}
    auto message=QJsonDocument::fromJson(text.toUtf8()).object();message.remove("_nativeFiles");
    if(message.value("method")==QJsonValue("files.drop")){
     QJsonArray paths;ComPtr<ICoreWebView2WebMessageReceivedEventArgs2> extended;a->QueryInterface(IID_PPV_ARGS(&extended));
     if(extended){ComPtr<ICoreWebView2ObjectCollectionView> objects;extended->get_AdditionalObjects(&objects);UINT32 count=0;if(objects)objects->get_Count(&count);
      for(UINT32 n=0;n<qMin<UINT32>(count,2000);++n){ComPtr<IUnknown> object;objects->GetValueAtIndex(n,&object);ComPtr<ICoreWebView2File> file;if(object)object.As(&file);if(file){LPWSTR path=nullptr;file->get_Path(&path);auto value=QString::fromWCharArray(path?path:L"");CoTaskMemFree(path);if(QFileInfo(value).isFile()&&!Files::reparse(value))paths.append(value);}}
     }message["_nativeFiles"]=paths;
    }
    // Never enter Qt modal/nested loops while a COM browser callback is on the stack.
    QTimer::singleShot(0,self,[self,message]{if(self)emit self->message(message);});return S_OK;
   }).Get(),&token);
   i.web->add_NavigationCompleted(Callback<ICoreWebView2NavigationCompletedEventHandler>([self](ICoreWebView2*,ICoreWebView2NavigationCompletedEventArgs *a)->HRESULT {
    if(!self)return S_OK;BOOL ok=FALSE;a->get_IsSuccess(&ok);if(ok)emit self->loaded();else emit self->failed(QStringLiteral("工具前端载入失败"));return S_OK;
   }).Get(),&token);
   i.web->add_ProcessFailed(Callback<ICoreWebView2ProcessFailedEventHandler>([self](ICoreWebView2*,ICoreWebView2ProcessFailedEventArgs *args)->HRESULT{if(self){COREWEBVIEW2_PROCESS_FAILED_KIND kind;args->get_ProcessFailedKind(&kind);self->impl->processDead=kind==COREWEBVIEW2_PROCESS_FAILED_KIND_BROWSER_PROCESS_EXITED;emit self->failed(QStringLiteral("工具前端进程异常，可重载界面；后台作业不会重复提交"));}return S_OK;}).Get(),&token);
   i.web->AddWebResourceRequestedFilter(L"*",COREWEBVIEW2_WEB_RESOURCE_CONTEXT_ALL);
   i.web->add_WebResourceRequested(Callback<ICoreWebView2WebResourceRequestedEventHandler>([self](ICoreWebView2*,ICoreWebView2WebResourceRequestedEventArgs *a)->HRESULT {
    if(!self)return S_OK;ComPtr<ICoreWebView2WebResourceRequest> req;a->get_Request(&req);LPWSTR raw=nullptr;req->get_Uri(&raw);QString url=QString::fromWCharArray(raw?raw:L"");CoTaskMemFree(raw);
    ComPtr<ICoreWebView2WebResourceResponse> response;
    if(!url.startsWith(self->origin()+"/")&&!url.startsWith("https://media."+self->impl->host+"/")){
     self->impl->environment->CreateWebResourceResponse(nullptr,403,L"Blocked",L"",&response);a->put_Response(response.Get());return S_OK;
    }
    const auto prefix="https://media."+self->impl->host+"/__file/";if(!url.startsWith(prefix))return S_OK;
    auto token=url.mid(prefix.size()).split('?').first();auto path=self->impl->files.value(token);QFileInfo file(path);
    if(path.isEmpty()||!file.isFile()||Files::reparse(path)){
     self->impl->environment->CreateWebResourceResponse(nullptr,404,L"Not found",L"",&response);a->put_Response(response.Get());return S_OK;
    }
    qint64 from=0,to=file.size()-1;bool partial=false;ComPtr<ICoreWebView2HttpRequestHeaders> headers;req->get_Headers(&headers);LPWSTR range=nullptr;headers->GetHeader(L"Range",&range);QString value=QString::fromWCharArray(range?range:L"");CoTaskMemFree(range);
    auto match=QRegularExpression("^bytes=(\\d*)-(\\d*)$").match(value);
    if(match.hasMatch()){
     partial=true;if(match.captured(1).isEmpty())from=qMax<qint64>(0,file.size()-match.captured(2).toLongLong());
     else from=match.captured(1).toLongLong();
     if(!match.captured(2).isEmpty()&&!match.captured(1).isEmpty())to=qMin(to,match.captured(2).toLongLong());
    }
    if(from>to||from<0){self->impl->environment->CreateWebResourceResponse(nullptr,416,L"Invalid range",L"",&response);a->put_Response(response.Get());return S_OK;}
    auto stream=Make<RangeStream>();if(FAILED(stream->open(path,from,to-from+1)))return S_OK;
    auto h=QString("Content-Type: %1\r\nContent-Length: %2\r\nAccept-Ranges: bytes\r\nCache-Control: no-store\r\nAccess-Control-Allow-Origin: *\r\n").arg(mime(path)).arg(to-from+1);
    if(partial)h+=QString("Content-Range: bytes %1-%2/%3\r\n").arg(from).arg(to).arg(file.size());
    self->impl->environment->CreateWebResourceResponse(stream.Get(),partial?206:200,L"OK",(LPCWSTR)h.utf16(),&response);a->put_Response(response.Get());return S_OK;
   }).Get(),&token);
   controller->add_MoveFocusRequested(Callback<ICoreWebView2MoveFocusRequestedEventHandler>([self](ICoreWebView2Controller*,ICoreWebView2MoveFocusRequestedEventArgs *a)->HRESULT{
    if(!self)return S_OK;COREWEBVIEW2_MOVE_FOCUS_REASON reason;a->get_Reason(&reason);a->put_Handled(TRUE);QTimer::singleShot(0,self,[self,reason]{if(self)self->focusNextPrevChild(reason!=COREWEBVIEW2_MOVE_FOCUS_REASON_PREVIOUS);});return S_OK;
   }).Get(),&token);
   controller->add_AcceleratorKeyPressed(Callback<ICoreWebView2AcceleratorKeyPressedEventHandler>([self](ICoreWebView2Controller*,ICoreWebView2AcceleratorKeyPressedEventArgs *a)->HRESULT{
    if(!self)return S_OK;COREWEBVIEW2_KEY_EVENT_KIND kind;a->get_KeyEventKind(&kind);if(kind!=COREWEBVIEW2_KEY_EVENT_KIND_KEY_DOWN)return S_OK;
    UINT key=0;a->get_VirtualKey(&key);if(!(GetKeyState(VK_CONTROL)&0x8000))return S_OK;
    int qt=key=='K'?Qt::Key_K:key=='N'?Qt::Key_N:key==VK_TAB?Qt::Key_Tab:key==VK_OEM_COMMA?Qt::Key_Comma:0;
    if(qt){a->put_Handled(TRUE);QTimer::singleShot(0,self,[self,qt]{if(self)QCoreApplication::postEvent(self->window(),new QKeyEvent(QEvent::KeyPress,qt,Qt::ControlModifier));});}return S_OK;
   }).Get(),&token);
   self->synchronize();const auto url=self->origin()+"/"+i.entry;i.web->Navigate((LPCWSTR)url.utf16());return S_OK;
  }).Get());
 }).Get());
 if(FAILED(hr))emit failed(QStringLiteral("WebView2启动失败：%1").arg((quint32)hr,8,16,QChar('0')));
}
void WebViewSurface::scheduleSynchronize(){
 auto *top=window();if(impl->observedWindow!=top){if(impl->observedWindow)impl->observedWindow->removeEventFilter(this);impl->observedWindow=top;if(top)top->installEventFilter(this);}
 if(impl->syncQueued)return;impl->syncQueued=true;QTimer::singleShot(0,this,[this]{impl->syncQueued=false;synchronize();});
}
void WebViewSurface::synchronize(){
 if(!impl->controller)return;
 const auto parent=(HWND)winId();
 const auto scale=devicePixelRatioF();
 if(impl->scaling&&!qFuzzyCompare(impl->appliedScale,scale)){impl->scaling->put_RasterizationScale(scale);impl->appliedScale=scale;setProperty("rasterScale",scale);}
 if(impl->controllerParent!=parent){impl->controller->put_ParentWindow(parent);impl->controllerParent=parent;setProperty("parentUpdates",property("parentUpdates").toInt()+1);}
 RECT bounds{0,0,qRound(width()*devicePixelRatioF()),qRound(height()*devicePixelRatioF())};
 if(!EqualRect(&bounds,&impl->appliedBounds)){impl->controller->put_Bounds(bounds);impl->appliedBounds=bounds;setProperty("boundsUpdates",property("boundsUpdates").toInt()+1);}
 POINT origin{0,0};ClientToScreen(parent,&origin);
 if(origin.x!=impl->appliedOrigin.x||origin.y!=impl->appliedOrigin.y){impl->controller->NotifyParentWindowPositionChanged();impl->appliedOrigin=origin;setProperty("positionUpdates",property("positionUpdates").toInt()+1);}
 const bool visible=isVisible();
 if(!impl->visibilityKnown||impl->appliedVisible!=visible){impl->controller->put_IsVisible(visible);impl->visibilityKnown=true;impl->appliedVisible=visible;setProperty("visibilityUpdates",property("visibilityUpdates").toInt()+1);}
}
bool WebViewSurface::eventFilter(QObject *object,QEvent *event){
 if(object==impl->observedWindow&&(event->type()==QEvent::Move||event->type()==QEvent::Resize||event->type()==QEvent::WindowStateChange||event->type()==QEvent::DevicePixelRatioChange))scheduleSynchronize();
 return QWidget::eventFilter(object,event);
}
void WebViewSurface::moveEvent(QMoveEvent *event){QWidget::moveEvent(event);scheduleSynchronize();}
bool WebViewSurface::event(QEvent *e){
 if((e->type()==QEvent::ParentAboutToChange || (e->type()==QEvent::PlatformSurface && static_cast<QPlatformSurfaceEvent*>(e)->surfaceEventType()==QPlatformSurfaceEvent::SurfaceAboutToBeDestroyed))&&impl->controller&&impl->parking)
  {impl->controller->put_ParentWindow(impl->parking);impl->controllerParent=impl->parking;}
 const auto result=QWidget::event(e);
 if(e->type()==QEvent::ParentChange||e->type()==QEvent::WinIdChange||e->type()==QEvent::DevicePixelRatioChange)scheduleSynchronize();return result;
}
void WebViewSurface::resizeEvent(QResizeEvent *e){
 QWidget::resizeEvent(e);
 // The native sizing loop must show live bounds, without waiting for a posted zero timer.
 if(window()->property("nativeSizing").toBool())synchronize();
 scheduleSynchronize();
}
void WebViewSurface::focusInEvent(QFocusEvent *e){QWidget::focusInEvent(e);if(impl->controller)impl->controller->MoveFocus(e->reason()==Qt::BacktabFocusReason?COREWEBVIEW2_MOVE_FOCUS_REASON_PREVIOUS:COREWEBVIEW2_MOVE_FOCUS_REASON_NEXT);}
void WebViewSurface::showEvent(QShowEvent *e){QWidget::showEvent(e);scheduleSynchronize();synchronize();}
void WebViewSurface::hideEvent(QHideEvent *e){
 QWidget::hideEvent(e);
 // A hide can be part of native reparenting. Never move a parked controller back to a retiring HWND.
 if(impl->controller&&(!impl->visibilityKnown||impl->appliedVisible)){
  impl->controller->put_IsVisible(FALSE);impl->visibilityKnown=true;impl->appliedVisible=false;setProperty("visibilityUpdates",property("visibilityUpdates").toInt()+1);
 }
}
void WebViewSurface::post(const QJsonObject &m){
 if(!impl->web)return;auto text=QString::fromUtf8(QJsonDocument(m).toJson(QJsonDocument::Compact));
 if(impl->queuedChars+text.size()>32*1024*1024){emit failed(QStringLiteral("前端消息队列超限，请重载界面查询最新状态"));return;}
 if(text.size()<60000){impl->outbound.enqueue(text);impl->queuedChars+=text.size();}
 else{
  auto token=QUuid::createUuid().toString(QUuid::Id128);QStringList parts;
  for(int pos=0;pos<text.size();){int count=qMin(60000,(int)text.size()-pos);if(pos+count<text.size()&&text.at(pos+count-1).isHighSurrogate())--count;parts.append(text.mid(pos,count));pos+=count;}
  for(int n=0;n<parts.size();++n){auto packet=QString::fromUtf8(QJsonDocument(QJsonObject{{"packet",token},{"total",parts.size()},{"index",n},{"data",parts[n]}}).toJson(QJsonDocument::Compact));impl->queuedChars+=packet.size();impl->outbound.enqueue(packet);}
 }
 if(!impl->draining){impl->draining=true;QTimer::singleShot(0,this,&WebViewSurface::drain);}
}
void WebViewSurface::drain(){
 if(impl->outbound.isEmpty()||!impl->web){impl->outbound.clear();impl->queuedChars=0;impl->draining=false;return;}
 auto frame=impl->outbound.dequeue();impl->queuedChars-=frame.size();impl->web->PostWebMessageAsJson((LPCWSTR)frame.utf16());QTimer::singleShot(1,this,&WebViewSurface::drain);
}
void WebViewSurface::evaluate(const QString &script,std::function<void(QString)> callback){if(impl->web)impl->web->ExecuteScript((LPCWSTR)script.utf16(),Callback<ICoreWebView2ExecuteScriptCompletedHandler>([callback](HRESULT hr,LPCWSTR s)->HRESULT{if(callback)callback(SUCCEEDED(hr)?QString::fromWCharArray(s?s:L""):QString());return S_OK;}).Get());else if(callback)callback({});}
void WebViewSurface::capture(const QString &path,std::function<void(bool)> callback){
 if(!impl->web){callback(false);return;}ComPtr<IStream> stream;auto hr=SHCreateStreamOnFileEx((LPCWSTR)path.utf16(),STGM_CREATE|STGM_WRITE|STGM_SHARE_EXCLUSIVE,FILE_ATTRIBUTE_NORMAL,TRUE,nullptr,&stream);if(FAILED(hr)){callback(false);return;}
 impl->web->CapturePreview(COREWEBVIEW2_CAPTURE_PREVIEW_IMAGE_FORMAT_PNG,stream.Get(),Callback<ICoreWebView2CapturePreviewCompletedHandler>([stream,callback](HRESULT hr)->HRESULT{callback(SUCCEEDED(hr));return S_OK;}).Get());
}
quint32 WebViewSurface::browserProcessId() const{UINT32 pid=0;if(impl->web)impl->web->get_BrowserProcessId(&pid);return pid;}
void WebViewSurface::reload(){
 if(impl->processDead){if(impl->controller)impl->controller->Close();impl->web.Reset();impl->scaling.Reset();impl->controller.Reset();impl->environment.Reset();impl->processDead=false;impl->started=false;start();}
 else if(impl->web)impl->web->Reload();
}
void WebViewSurface::allowFile(const QString &token,const QString &path){impl->files[token]=path;}

void WebViewSurface::testInput(const QString &method,const QJsonObject &params,std::function<void(bool)> done) {
    if(!qEnvironmentVariableIsSet("MODYN_BACKGROUND_VALIDATION")||!impl->web||
       !QStringList{"Emulation.setFocusEmulationEnabled","Input.dispatchMouseEvent","Input.dispatchKeyEvent"}.contains(method)) {done(false);return;}
    auto json=QString::fromUtf8(QJsonDocument(params).toJson(QJsonDocument::Compact));
    auto result=impl->web->CallDevToolsProtocolMethod((LPCWSTR)method.utf16(),(LPCWSTR)json.utf16(),
        Callback<ICoreWebView2CallDevToolsProtocolMethodCompletedHandler>([done](HRESULT hr,LPCWSTR)->HRESULT{done(SUCCEEDED(hr));return S_OK;}).Get());
    if(FAILED(result))done(false);
}
