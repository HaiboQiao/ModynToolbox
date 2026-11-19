#include "ui/workbench.h"
#include "ui/tool_runtime/web_tool_view.h"
#include "platform/windows/webview_surface.h"
#include "ui/components/tool_navigation.h"
#include "ui/theme/theme.h"
#include "core/files.h"
#include <QApplication>
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonArray>
#include <QTemporaryDir>
#include <QPointer>
#include <QImage>
#include <QProcess>
#include <QPainter>
#define NOMINMAX
#include <windows.h>
#include <psapi.h>
#include <tlhelp32.h>
#include <QHBoxLayout>
#include <QStackedWidget>
#include "jobs/scheduler.h"
#include <QtTest>
class WebFrontendTests:public QObject {
 Q_OBJECT
 QString evaluate(WebViewSurface *view,const QString &js){
  auto value=std::make_shared<QString>();auto done=std::make_shared<bool>(false);
  view->evaluate(js,[value,done](QString s){*value=s;*done=true;});QElapsedTimer timer;timer.start();while(!*done&&timer.elapsed()<5000)QTest::qWait(10);return *value;
 }
 bool waitExpression(WebViewSurface *view,const QString &js){QElapsedTimer t;t.start();while(t.elapsed()<15000){if(evaluate(view,js)=="true")return true;QTest::qWait(30);}return false;}
 bool nativeInput(WebViewSurface *view,const QString &method,const QJsonObject &params) {
  auto done=std::make_shared<bool>(false),ok=std::make_shared<bool>(false);
  view->testInput(method,params,[done,ok](bool value){*done=true;*ok=value;});QElapsedTimer timer;timer.start();while(!*done&&timer.elapsed()<5000)QTest::qWait(10);return *done&&*ok;
 }
private slots:
 void completePageSurvivesWindowMoves(){
  qputenv("MODYN_BACKGROUND_VALIDATION","1");
  auto source=qEnvironmentVariable("MODYN_SOURCE_ROOT");QTemporaryDir temp;QVERIFY(temp.isValid());
  auto pkg=temp.path()+"/packages/web-proof/1.0.0";QDir().mkpath(pkg+"/frontend");
  QFile html(pkg+"/frontend/index.html");QVERIFY(html.open(QIODevice::WriteOnly));html.write(R"(<html><meta charset='utf-8'><input id='edit'><script>window.boots=1;window.custom={};chrome.webview.addEventListener('message',e=>{if(e.data.event==='bootstrap')window.custom=e.data.payload});chrome.webview.postMessage({bridgeVersion:1,requestId:'boot',method:'ready'});</script></html>)");html.close();
  Files::writeObject(pkg+"/checksums.json",{{"frontend/index.html",Files::sha256(pkg+"/frontend/index.html")}});
  Files::writeObject(pkg+"/manifest.json",{{"integrity_sha256",Files::sha256(pkg+"/checksums.json")}});
  ToolDefinition tool;tool.id="web-proof";tool.name="完整前端验证";tool.version="1.0.0";tool.packageRoot=pkg;tool.frontend="frontend/index.html";
  Workbench window({tool},WorkbenchConfig{temp.path()+"/data",{}, {}});
  window.setAttribute(Qt::WA_ShowWithoutActivating);window.setWindowFlag(Qt::Tool);window.move(-20000,-20000);window.show();
  auto *page=qobject_cast<WebToolView*>(window.panel(1));QVERIFY(page);auto *browser=page->browser();
  QString answer;auto query=[&](const QString &js){answer.clear();browser->evaluate(js,[&](QString s){answer=s;});};
  QTest::qWait(300);
  // Wait for the document rather than only the controller's existence.
  bool loaded=false;for(int i=0;i<150&&!loaded;i++){query("String(window.boots||'')");QTest::qWait(50);loaded=answer=="\"1\"";}QVERIFY2(loaded,qPrintable(answer));
  browser->evaluate("document.querySelector('#edit').value='未保存编辑';window.keep=crypto.randomUUID()");QTest::qWait(50);
  query("window.keep");QTest::qWait(30);auto identity=answer;QVERIFY(!identity.isEmpty());
  for(int i=0;i<100;i++){
   window.detachSession(1);auto *floating=window.sessionView(1)->window();floating->setAttribute(Qt::WA_ShowWithoutActivating);floating->move(-20000,-20000);
   window.attachSession(1);QCoreApplication::processEvents();
  }
  QTest::qWait(100);query("window.keep");QTest::qWait(50);QCOMPARE(answer,identity);
  query("document.querySelector('#edit').value");QTest::qWait(50);QVERIFY(answer.contains(QStringLiteral("未保存编辑")));
  auto folder=source+"/.local/logs";bool captured=false;browser->capture(folder+"/IF1-web-native.png",[&](bool ok){captured=ok;});QTRY_VERIFY_WITH_TIMEOUT(captured,10000);
  window.closeSession(1,true);
 }
 void warmSwitchAndBoundedCheckpointCache(){
  qputenv("MODYN_BACKGROUND_VALIDATION","1"); auto source=qEnvironmentVariable("MODYN_SOURCE_ROOT");
  QTemporaryDir root; QVERIFY(root.isValid()); QVector<ToolDefinition> tools;
  for (const auto &id:{"json","file-hash","rename"}) {
   auto path=Files::readObject(source+"/.local/tools/"+id+"/latest.json",4096).value("package").toString(); QString error;
   tools.append(definitionFromManifest(path,Files::readObject(path+"/manifest.json",65536),&error)); QVERIFY2(error.isEmpty(),qPrintable(error));
  }
  WorkbenchConfig options{root.path()+"/data",{}, {}}; options.webCacheLimit=1;
  Workbench window(tools,options); window.setWindowFlag(Qt::Tool);window.setAttribute(Qt::WA_ShowWithoutActivating);window.move(-20000,-20000);window.show();
  int a=window.open("json"); auto *page=qobject_cast<WebToolView *>(window.panel(a)); QVERIFY(page);
  QVERIFY(waitExpression(page->browser(),"Boolean(window.toolApp)"));
  evaluate(page->browser(),"window.r7Token='warm';window.toolApp.text='unsaved cache state'");
  const auto identity=page;
  int b=window.open("file-hash"); auto *hash=qobject_cast<WebToolView *>(window.panel(b)); QVERIFY(hash);
  QVERIFY(waitExpression(hash->browser(),"Boolean(document.querySelector('#calculate'))"));
  QElapsedTimer switchTime;switchTime.start(); QCOMPARE(window.open("json"),a); QCOMPARE(window.panel(a),identity);
  QCOMPARE(evaluate(page->browser(),"window.r7Token"),QString("\"warm\""));
  QVERIFY(waitExpression(page->browser(),"window.toolApp.text==='unsaved cache state'"));
  qInfo()<<"UIR7 warm switch including evaluation ms"<<switchTime.elapsed();
  // A running hidden page is protected even if it falls outside the LRU allowance.
  page->setJobState("running","protected",0,1,false);
  window.open("file-hash"); int c=window.open("rename");
  auto *rename=qobject_cast<WebToolView *>(window.panel(c)); QVERIFY(rename);
  QVERIFY(waitExpression(rename->browser(),"Boolean(document.querySelector('#preview'))"));
  QTest::qWait(200); QCOMPARE(window.panel(a),identity);
  QPointer<WebToolView> old=page;
  page->setJobState("succeeded","done",1,1,false);window.open("rename");
  QTRY_VERIFY_WITH_TIMEOUT(old.isNull(),10000); // panel() would lazily recreate the page.
  window.open("json");page=qobject_cast<WebToolView *>(window.panel(a));QVERIFY(page);
  QVERIFY(waitExpression(page->browser(),"Boolean(window.toolApp)&&window.toolApp.text==='unsaved cache state'"));
  QCOMPARE(evaluate(page->browser(),"typeof window.r7Token"),QString("\"undefined\""));
  window.closeSession(a,true);window.closeSession(b,true);window.closeSession(c,true);
 }
 void idleCacheReclaimsOnlyUnpinnedPages(){
  qputenv("MODYN_BACKGROUND_VALIDATION","1");auto source=qEnvironmentVariable("MODYN_SOURCE_ROOT");QTemporaryDir root;QVERIFY(root.isValid());
  auto path=Files::readObject(source+"/.local/tools/json/latest.json",4096).value("package").toString();QString error;
  auto tool=definitionFromManifest(path,Files::readObject(path+"/manifest.json",65536),&error);QVERIFY2(error.isEmpty(),qPrintable(error));
  WorkbenchConfig options{root.path()+"/data",{}, {}};options.webCacheIdleMs=800;
  Workbench window({tool},options);window.setWindowFlag(Qt::Tool);window.setAttribute(Qt::WA_ShowWithoutActivating);window.move(-20000,-20000);window.show();
  int a=window.open("json");auto *page=qobject_cast<WebToolView *>(window.panel(a));QVERIFY(page);QVERIFY(waitExpression(page->browser(),"Boolean(window.toolApp)"));
  evaluate(page->browser(),"window.toolApp.text='idle checkpoint'");
  window.detachSession(a);int b=window.open("json",true);auto *other=qobject_cast<WebToolView *>(window.panel(b));QVERIFY(other);QVERIFY(waitExpression(other->browser(),"Boolean(window.toolApp)"));
  QTest::qWait(1200);QCOMPARE(window.panel(a),page); // Detached stays resident.
  window.attachSession(a);window.open("json",true); // Now a becomes inactive, so timeout applies.
  QPointer<WebToolView> old=page;QTRY_VERIFY_WITH_TIMEOUT(old.isNull(),10000);
  window.open("json"); // Most recent session remains active.
  QVERIFY(window.panel(3));window.closeSession(3,true);window.closeSession(b,true);window.closeSession(a,true);
 }
 void trustedWheelAndRealFocusInOwnBrowser(){
  qputenv("MODYN_BACKGROUND_VALIDATION","1");auto source=qEnvironmentVariable("MODYN_SOURCE_ROOT");QTemporaryDir root;QVERIFY(root.isValid());QVector<ToolDefinition> tools;
  for(const auto &id:{"rename","file-hash"}){auto path=Files::readObject(source+"/.local/tools/"+id+"/latest.json",4096).value("package").toString();QString error;tools.append(definitionFromManifest(path,Files::readObject(path+"/manifest.json",65536),&error));QVERIFY2(error.isEmpty(),qPrintable(error));}
  Workbench window(tools,{root.path()+"/data",{}, {}});window.setWindowFlag(Qt::Tool);window.setAttribute(Qt::WA_ShowWithoutActivating);window.move(-20000,-20000);window.show();
  auto edgeChecks=[](QWidget *root) {
   auto hwnd=reinterpret_cast<HWND>(root->winId());RECT b{};if(!GetWindowRect(hwnd,&b))return false;const int x=(b.left+b.right)/2,y=(b.top+b.bottom)/2;
   const struct{int x,y,hit;} points[]={{b.left+1,y,10},{b.right-1,y,11},{x,b.top+1,12},{b.left+1,b.top+1,13},{b.right-1,b.top+1,14},{x,b.bottom-1,15},{b.left+1,b.bottom-1,16},{b.right-1,b.bottom-1,17}};
   for(const auto &p:points){POINT local{p.x,p.y};ScreenToClient(hwnd,&local);auto child=ChildWindowFromPointEx(hwnd,local,CWP_SKIPINVISIBLE|CWP_SKIPDISABLED|CWP_SKIPTRANSPARENT);if(!child||child==hwnd)return false;
    auto point=(LPARAM)((p.x&65535)|((p.y&65535)<<16));if(SendMessageW(child,WM_NCHITTEST,0,point)!=p.hit)return false;
    COLORREF key{};BYTE alpha{};DWORD flags{};if(!GetLayeredWindowAttributes(child,&key,&alpha,&flags)||alpha!=1)return false;
   }return true;
  };
  for(const auto &id:{"rename","file-hash"}){
   auto session=window.open(id);auto *page=qobject_cast<WebToolView *>(window.panel(session));QVERIFY(page);auto *browser=page->browser();
   QVERIFY(waitExpression(browser,"Boolean(document.querySelector('main'))&&Boolean(window.modyn||document.querySelector('#preview')||document.querySelector('#calculate'))"));
   QTRY_VERIFY_WITH_TIMEOUT(page->property("frontendReadyMs").isValid(),5000);
   evaluate(browser,"window.r9Identity='kept'");
   const auto beforeParent=browser->property("parentUpdates").toInt(),beforeVisible=browser->property("visibilityUpdates").toInt();
   auto handles=window.findChildren<QWidget *>("windowResizeHandle");QVector<int> applied;for(auto *h:handles)applied.append(h->property("nativeLayerApplyCount").toInt());
   auto resizingHwnd=reinterpret_cast<HWND>(window.winId());SendMessageW(resizingHwnd,WM_ENTERSIZEMOVE,0,0);QVERIFY(window.property("nativeSizing").toBool());
   for(int n=0;n<30;++n){window.resize(950+n%5*20,650+n%3*12);QTest::qWait(8);}
   SendMessageW(resizingHwnd,WM_EXITSIZEMOVE,0,0);QVERIFY(!window.property("nativeSizing").toBool());
   QTest::qWait(80);QCOMPARE(browser->property("parentUpdates").toInt(),beforeParent);QCOMPARE(browser->property("visibilityUpdates").toInt(),beforeVisible);
   for(int n=0;n<handles.size();++n)QCOMPARE(handles[n]->property("nativeLayerApplyCount").toInt(),applied[n]);
   QCOMPARE(evaluate(browser,"window.r9Identity"),QString("\"kept\""));
   QVERIFY2(waitExpression(browser,QString("Math.abs(window.innerWidth-%1)<=2 && document.documentElement.scrollWidth<=window.innerWidth+2").arg(browser->width())),qPrintable(QString("Qt=%1x%2 DPR=%3 boundsUpdates=%4 JS=%5").arg(browser->width()).arg(browser->height()).arg(browser->devicePixelRatioF()).arg(browser->property("boundsUpdates").toInt()).arg(evaluate(browser,"JSON.stringify({inner:innerWidth,client:document.documentElement.clientWidth,scroll:document.documentElement.scrollWidth,dpr:devicePixelRatio})"))));
   QVERIFY(edgeChecks(&window));window.detachSession(session);QCoreApplication::processEvents();QVERIFY(edgeChecks(page->window()));window.attachSession(session);
   QVERIFY(nativeInput(browser,"Emulation.setFocusEmulationEnabled",{{"enabled",true}}));
   auto selector=QString(id)=="rename"?QString("#start"):QString("select");
   auto setup=QString(R"JS((()=>{const n=document.querySelector('%1');window.r8Control=n;const spacer=document.createElement('div');spacer.style.height='2500px';document.body.append(spacer);n.focus();n.scrollIntoView({block:'center'});window.r8Before=n.value;window.r8Scroll=document.scrollingElement.scrollTop;const r=n.getBoundingClientRect();return {x:r.x+r.width/2,y:r.y+r.height/2}})())JS").arg(selector);
   auto coords=QJsonDocument::fromJson(evaluate(browser,setup).toUtf8()).object();QVERIFY(!coords.isEmpty());
   QVERIFY2(waitExpression(browser,"document.hasFocus()&&window.r8Control.matches(':focus')&&getComputedStyle(window.r8Control).outlineStyle==='none'&&parseFloat(getComputedStyle(window.r8Control).borderTopWidth)<=1.01&&getComputedStyle(window.r8Control).borderTopColor==='rgb(112, 84, 216)'"),qPrintable(evaluate(browser,"JSON.stringify({focus:document.hasFocus(),matches:r8Control.matches(':focus'),border:getComputedStyle(r8Control).borderTopColor})")));
   // These are browser-generated trusted wheel events, not dispatchEvent mocks or global input.
   QVERIFY(nativeInput(browser,"Input.dispatchMouseEvent",{{"type","mouseWheel"},{"x",coords.value("x")},{"y",coords.value("y")},{"deltaX",0},{"deltaY",120}}));
   QVERIFY(waitExpression(browser,"r8Control.value===r8Before&&document.activeElement===r8Control&&document.scrollingElement.scrollTop>r8Scroll"));
   const bool number=QString(id)=="rename";
   auto key=number?QString("ArrowUp"):QString("ArrowDown");const int virtualKey=number?38:40;
   QVERIFY(nativeInput(browser,"Input.dispatchKeyEvent",{{"type","keyDown"},{"key",key},{"code",key},{"windowsVirtualKeyCode",virtualKey}}));
   QVERIFY(nativeInput(browser,"Input.dispatchKeyEvent",{{"type","keyUp"},{"key",key},{"code",key},{"windowsVirtualKeyCode",virtualKey}}));
   QVERIFY(waitExpression(browser,"r8Control.value!==r8Before"));
   bool captured=false;browser->capture(source+"/.local/logs/UIR8-"+id+"-focus.png",[&](bool ok){captured=ok;});QTRY_VERIFY_WITH_TIMEOUT(captured,5000);
  }
  window.closeSession(1,true);window.closeSession(2,true);
 }
 void realIndependentToolPages(){
  qputenv("MODYN_BACKGROUND_VALIDATION","1");const auto source=qEnvironmentVariable("MODYN_SOURCE_ROOT");
  QTemporaryDir root;QVERIFY(root.isValid());QVector<ToolDefinition> definitions;
  for(const auto &id:{"file-hash","image","json","rename","video-clip"}){
   auto record=Files::readObject(source+"/.local/tools/"+id+"/latest.json",4096);auto path=record.value("package").toString();QString error;
   auto tool=definitionFromManifest(path,Files::readObject(path+"/manifest.json",65536),&error);QVERIFY2(error.isEmpty(),qPrintable(error));definitions.append(tool);
  }
  auto data=root.path()+"/data";Workbench window(definitions,{data,{},QCoreApplication::applicationDirPath()+"/ModynToolbox.WorkerHost.exe"});
  window.setWindowFlag(Qt::Tool);window.setAttribute(Qt::WA_ShowWithoutActivating);window.move(-20000,-20000);window.show();
  auto file=root.path()+"/sample.txt";QFile f(file);QVERIFY(f.open(QIODevice::WriteOnly));f.write("original");f.close();
  QImage image(40,20,QImage::Format_RGB32);image.fill(QColor("#7054d8"));auto picture=root.path()+"/sample.png";QVERIFY(image.save(picture));
  auto capture=[&](WebToolView *page,const QString &name){bool ok=false;page->browser()->capture(source+"/.local/logs/IF1-"+name+".png",[&](bool v){ok=v;});QElapsedTimer t;t.start();while(!ok&&t.elapsed()<10000)QTest::qWait(20);return ok;};
  auto execute=[&](WebToolView *page,const QString &js){auto before=page->jobSummary().value("id").toString();evaluate(page->browser(),js);QElapsedTimer t;t.start();while(t.elapsed()<90000){auto j=page->jobSummary();if(j.value("id").toString()!=before&&!j.value("id").toString().isEmpty()&&QStringList{"succeeded","failed","cancelled"}.contains(j.value("state").toString()))return j.value("state").toString();QTest::qWait(30);}return QString("timeout");};
  auto jsonId=window.open("json");auto *json=qobject_cast<WebToolView*>(window.panel(jsonId));QVERIFY(json);
  QVERIFY(waitExpression(json->browser(),"Boolean(window.toolApp)"));
  QVERIFY(waitExpression(json->browser(),R"JS((()=>{
   const n=document.querySelector('textarea');
   const sheet=[...document.styleSheets].find(s=>s.href?.endsWith('/base.css'));
   const rule=[...sheet.cssRules].find(r=>r.selectorText?.includes('textarea:focus'));
   if(!rule)return false;
   // The nonactivating test window cannot acquire OS focus. Force only the CSS pseudo-state
   // in this test document, using the actual tool-owned declaration (no production injection).
   const at=sheet.insertRule(rule.cssText.replaceAll(':focus','[data-r7-focus]'),sheet.cssRules.length);
   n.setAttribute('data-r7-focus','');const st=getComputedStyle(n);
   const ok=st.outlineStyle==='none'&&parseFloat(st.borderTopWidth)<=1.01&&st.borderTopColor==='rgb(112, 84, 216)';
   n.removeAttribute('data-r7-focus');sheet.deleteRule(at);return ok;
  })())JS"));
  evaluate(json->browser(),"window.toolApp.text='{\"n\":123456789012345678901234567890}'");
  QCOMPARE(execute(json,"window.toolApp.run('format')"),QString("succeeded"));
  QVERIFY(waitExpression(json->browser(),"window.toolApp.output.includes('123456789012345678901234567890')"));QVERIFY(capture(json,"json-page"));
  // Force transport fragmentation in both directions without changing the host contract.
  evaluate(json->browser(),"window.toolApp.text=JSON.stringify({large:'x'.repeat(300000),emoji:'😀'})");
  QCOMPARE(execute(json,"window.toolApp.run('format')"),QString("succeeded"));QVERIFY(waitExpression(json->browser(),"window.toolApp.output.includes('😀') && window.toolApp.output.length>300000"));
  auto renameId=window.open("rename");auto *rename=qobject_cast<WebToolView*>(window.panel(renameId));QVERIFY(rename);
  QVERIFY(waitExpression(rename->browser(),"Boolean(document.querySelector('#preview'))"));
  QVERIFY(rename->restore({{"ui_state",QJsonObject{{"files",QJsonArray{QJsonObject{{"path",file}}}},{"params",QJsonObject{{"prefix","new_"}}}}},{"input",QJsonObject{{"files",QJsonArray{file}},{"params",QJsonObject{{"prefix","new_"}}}}}}));QTest::qWait(200);
  QCOMPARE(execute(rename,"document.querySelector('#preview').click()"),QString("succeeded"));QVERIFY(waitExpression(rename->browser(),"document.querySelector('#results').textContent.includes('new_sample')"));QVERIFY(capture(rename,"rename-page"));
  QVERIFY(waitExpression(rename->browser(),"!document.querySelector('#apply').disabled"));
  evaluate(rename->browser(),"document.querySelector('#prefix').value='changed_';document.querySelector('#prefix').dispatchEvent(new Event('input',{bubbles:true}))");
  QCOMPARE(evaluate(rename->browser(),"document.querySelector('#apply').disabled"),QString("true"));

  auto imageId=window.open("image");auto *images=qobject_cast<WebToolView*>(window.panel(imageId));QVERIFY(images);
  QVERIFY(waitExpression(images->browser(),"Boolean(document.querySelector('#compress'))"));
  images->restore({{"ui_state",QJsonObject{{"files",QJsonArray{QJsonObject{{"path",picture}}}}}},{"input",QJsonObject{{"files",QJsonArray{picture}}}}});QTest::qWait(200);
  QCOMPARE(execute(images,"document.querySelector('#compress').click()"),QString("succeeded"));QVERIFY(QFileInfo::exists(data+"/tools/image/exports/sample.webp"));QVERIFY2(waitExpression(images->browser(),"document.querySelector('#before').naturalWidth>0 && document.querySelector('#after').naturalWidth>0"),qPrintable(evaluate(images->browser(),"JSON.stringify({before:document.querySelector('#before').src,after:document.querySelector('#after').src})")));QVERIFY(capture(images,"image-page"));
  auto hashId=window.open("file-hash");auto *hash=qobject_cast<WebToolView*>(window.panel(hashId));QVERIFY(hash);
  QVERIFY(waitExpression(hash->browser(),"Boolean(document.querySelector('#calculate'))"));hash->restore({{"ui_state",QJsonObject{{"files",QJsonArray{QJsonObject{{"path",file}}}}}},{"input",QJsonObject{{"files",QJsonArray{file}}}}});QTest::qWait(200);
  QCOMPARE(execute(hash,"document.querySelector('#calculate').click()"),QString("succeeded"));QVERIFY(waitExpression(hash->browser(),"document.querySelector('#results').textContent.length>60"));QVERIFY(capture(hash,"hash-page"));
  auto videoId=window.open("video-clip");auto *video=qobject_cast<WebToolView*>(window.panel(videoId));QVERIFY(video);
  QVERIFY(waitExpression(video->browser(),"Boolean(document.querySelector('#export'))"));
  auto sample=Files::readObject(source+"/.local/video-tests/latest.json",4096).value("sample").toString();QVERIFY(QFileInfo::exists(sample));
  QJsonObject params{{"start_s",1.24},{"end_s",4.24},{"mode",QStringLiteral("精确裁剪")},{"quality",QStringLiteral("均衡")},{"size",QStringLiteral("原尺寸")},{"output_dir",data+"/tools/video-clip/exports"}};
  video->restore({{"ui_state",QJsonObject{{"file",QJsonObject{{"path",sample}}},{"params",params}}},{"input",QJsonObject{{"files",QJsonArray{sample}}}}});QTest::qWait(200);
  QCOMPARE(execute(video,"document.querySelector('#export').click()"),QString("succeeded"));QVERIFY(waitExpression(video->browser(),"document.querySelector('#player').readyState>=1"));QVERIFY(capture(video,"video-page"));
  QVERIFY(waitExpression(video->browser(),"document.querySelector('#results').textContent.length>10"));
  evaluate(video->browser(),"document.querySelector('#mode').value='快速截取';document.querySelector('#mode').dispatchEvent(new Event('change',{bubbles:true}))");
  QVERIFY(waitExpression(video->browser(),"!document.querySelector('#prepare').hidden&&document.querySelector('#export').disabled"));
  QCOMPARE(execute(video,"document.querySelector('#prepare').click()"),QString("succeeded"));
  QVERIFY(waitExpression(video->browser(),"!document.querySelector('#export').disabled"));
  QCOMPARE(execute(video,"document.querySelector('#export').click()"),QString("succeeded"));

  for(const auto &toolId:{"rename","image","file-hash","video-clip"}){
   auto id=window.open(toolId);auto *page=qobject_cast<WebToolView*>(window.panel(id));QVERIFY(page);
   QVERIFY(waitExpression(page->browser(),"Boolean(document.querySelector('main'))"));
   // Exercise each real tool's own focus/wheel module, not injected host styles.
   const auto interaction=evaluate(page->browser(),R"JS((()=>{
    const panel=document.querySelector('#export-settings');
    if(panel&&getComputedStyle(panel).display==='none')document.querySelector('#settings-toggle').click();
    document.querySelectorAll('details').forEach(d=>d.open=true);
    const controls=[...document.querySelectorAll('input[type=number],select')].filter(n=>!n.disabled);
    if(!controls.length)return 'no parameter control';
    let spacer=document.createElement('div');spacer.style.height='2500px';document.body.append(spacer);
    let ok=true;
    for(const n of controls){n.focus();scrollTo(0,0);const value=n.value,style=getComputedStyle(n);
     const nodes=new Set([document.scrollingElement]);for(let a=n.parentElement;a;a=a.parentElement)nodes.add(a);const before=new Map([...nodes].map(a=>[a,a.scrollTop]));
     const wheel=new WheelEvent('wheel',{deltaY:120,bubbles:true,cancelable:true});n.dispatchEvent(wheel);
     const moved=[...nodes].some(a=>a.scrollTop>before.get(a));
     ok=ok&&wheel.defaultPrevented&&n.value===value&&document.activeElement===n&&moved&&style.outlineStyle==='none'&&parseFloat(style.borderTopWidth)<=1.01;
    }
    spacer.remove();scrollTo(0,0);return ok;
   })())JS");
   QCOMPARE(interaction,QString("true"));
   QVERIFY(waitExpression(page->browser(),"parseFloat(getComputedStyle(document.querySelector('main')).paddingLeft)<=12"));
   page->setFixedWidth(440);page->show();QTest::qWait(100);
   evaluate(page->browser(),"document.documentElement.style.fontSize='18px'");QTest::qWait(100);
   QVERIFY2(waitExpression(page->browser(),"document.documentElement.clientWidth<=440 && document.documentElement.scrollWidth<=document.documentElement.clientWidth+2"),qPrintable(evaluate(page->browser(),"JSON.stringify({scroll:document.documentElement.scrollWidth,client:document.documentElement.clientWidth})")));
   page->setMinimumWidth(0);page->setMaximumWidth(QWIDGETSIZE_MAX);
  }


  // Re-open an evicted page: fresh checkpoint preserves its entire opaque input.
  QCOMPARE(window.open("json"),jsonId);json=qobject_cast<WebToolView*>(window.panel(jsonId));QVERIFY(json);
  QVERIFY(waitExpression(json->browser(),"Boolean(window.toolApp)&&window.toolApp.text.length>300000"));
  QVERIFY(!QJsonDocument(window.savedState()).toJson().contains(QByteArray(100,'x')));
  window.setDraft(jsonId,true);bool flushed=false;json->flush([&](bool ok){flushed=ok;});QTRY_VERIFY_WITH_TIMEOUT(flushed,10000);QVERIFY(QJsonDocument(window.savedState()).toJson().contains(QByteArray(100,'x')));
  window.detachSession(jsonId);auto secondJson=window.open("json",true);auto *other=qobject_cast<WebToolView*>(window.panel(secondJson));QVERIFY(other);
  QVERIFY(waitExpression(other->browser(),"Boolean(window.toolApp)&&window.toolApp.text===''") );
  QVERIFY(waitExpression(json->browser(),"window.toolApp.text.length>300000"));window.attachSession(jsonId);window.closeSession(secondJson,true);
  for(const auto &job:window.jobScheduler()->records())QVERIFY2(job.state=="succeeded",qPrintable(job.message));
  window.closeSession(videoId,true);window.closeSession(hashId,true);window.closeSession(imageId,true);window.closeSession(renameId,true);window.closeSession(jsonId,true);
 }

 void browserFailureIsLocalAndRecoverable(){
  qputenv("MODYN_BACKGROUND_VALIDATION","1");auto source=qEnvironmentVariable("MODYN_SOURCE_ROOT");QTemporaryDir root;QWidget parent;
  parent.setWindowFlag(Qt::Tool);parent.setAttribute(Qt::WA_ShowWithoutActivating);parent.move(-20000,-20000);parent.resize(1100,700);auto *layout=new QHBoxLayout(&parent);
  auto load=[&](const QString &id){auto path=Files::readObject(source+"/.local/tools/"+id+"/latest.json",4096).value("package").toString();QString error;return definitionFromManifest(path,Files::readObject(path+"/manifest.json",65536),&error);};
  auto *a=new WebToolView(load("json"),root.path(),"first",&parent);auto *b=new WebToolView(load("file-hash"),root.path(),"second",&parent);layout->addWidget(a);layout->addWidget(b);parent.show();
  QVERIFY(waitExpression(a->browser(),"Boolean(window.toolApp)"));QVERIFY(waitExpression(b->browser(),"Boolean(document.querySelector('#calculate'))"));
  evaluate(a->browser(),"window.toolApp.text='retained checkpoint'");QTest::qWait(500);
  auto apid=a->browser()->browserProcessId(),bpid=b->browser()->browserProcessId();QVERIFY(apid&&bpid&&apid!=bpid);
  QSignalSpy failure(a->browser(),&WebViewSurface::failed);HANDLE process=OpenProcess(PROCESS_TERMINATE,FALSE,apid);QVERIFY(process);QVERIFY(TerminateProcess(process,97));CloseHandle(process);
  QTRY_VERIFY_WITH_TIMEOUT(!failure.isEmpty(),15000);QVERIFY(waitExpression(b->browser(),"Boolean(document.querySelector('#calculate'))"));
  a->browser()->reload();QVERIFY(waitExpression(a->browser(),"Boolean(window.toolApp)&&window.toolApp.text==='retained checkpoint'"));QVERIFY(a->browser()->browserProcessId()!=apid);
  Files::writeObject(source+"/.local/logs/IF1-failure-isolation.json",{{"different_tool_browser_groups",true},{"other_tool_survived",true},{"checkpoint_restored",true}});
  delete a;delete b;QTest::qWait(200);
 }

 void resourceProfile(){
  qputenv("MODYN_BACKGROUND_VALIDATION","1");auto source=qEnvironmentVariable("MODYN_SOURCE_ROOT");QTemporaryDir root;Workbench parent({},WorkbenchConfig{root.path()+"/host-data",{},QCoreApplication::applicationDirPath()+"/ModynToolbox.WorkerHost.exe"});
  parent.setWindowFlag(Qt::Tool);parent.setAttribute(Qt::WA_ShowWithoutActivating);parent.move(-20000,-20000);parent.resize(1000,700);auto *carrier=new QWidget;auto *layout=new QHBoxLayout(carrier);auto *workspace=parent.findChild<QStackedWidget*>("primaryWorkspace");QVERIFY(workspace);workspace->addWidget(carrier);workspace->setCurrentWidget(carrier);parent.show();QTest::qWait(100);
  QVector<WebToolView*> pages;QJsonArray samples;QElapsedTimer wall;wall.start();QVector<qint64> ticks;QElapsedTimer beatClock;beatClock.start();QTimer beat;beat.setInterval(10);connect(&beat,&QTimer::timeout,this,[&]{ticks.append(beatClock.restart());});beat.start();
  auto sample=[&](const QString &name){
   QSet<DWORD> ids;for(auto *p:pages)if(p->browser()->browserProcessId())ids.insert(p->browser()->browserProcessId());
   QMap<DWORD,DWORD> parents;auto snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);PROCESSENTRY32 e{};e.dwSize=sizeof(e);if(Process32First(snapshot,&e))do{parents[e.th32ProcessID]=e.th32ParentProcessID;}while(Process32Next(snapshot,&e));CloseHandle(snapshot);
   for(int pass=0;pass<8;++pass)for(auto it=parents.cbegin();it!=parents.cend();++it)if(ids.contains(it.value()))ids.insert(it.key());
   ids.insert(GetCurrentProcessId());
   double privateBytes=0,workingSet=0,cpuSeconds=0;int processes=0;
   for(auto id:ids){auto process=OpenProcess(PROCESS_QUERY_INFORMATION|PROCESS_VM_READ,FALSE,id);if(!process)continue;PROCESS_MEMORY_COUNTERS_EX memory{};if(GetProcessMemoryInfo(process,(PROCESS_MEMORY_COUNTERS*)&memory,sizeof(memory))){privateBytes+=memory.PrivateUsage;workingSet+=memory.WorkingSetSize;++processes;}FILETIME start,end,kernel,user;if(GetProcessTimes(process,&start,&end,&kernel,&user)){ULARGE_INTEGER k{},u{};k.LowPart=kernel.dwLowDateTime;k.HighPart=kernel.dwHighDateTime;u.LowPart=user.dwLowDateTime;u.HighPart=user.dwHighDateTime;cpuSeconds+=(k.QuadPart+u.QuadPart)/10000000.0;}CloseHandle(process);}
   samples.append(QJsonObject{{"stage",name},{"pages",pages.size()},{"processes",processes},{"private_MiB",privateBytes/1048576},{"sum_working_set_MiB",workingSet/1048576},{"cpu_seconds_cumulative",cpuSeconds},{"elapsed_ms",double(wall.elapsed())}});
  };
  auto load=[&](const QString &id){auto path=Files::readObject(source+"/.local/tools/"+id+"/latest.json",4096).value("package").toString();QString error;return definitionFromManifest(path,Files::readObject(path+"/manifest.json",65536),&error);};
  auto append=[&](const QString &id){for(auto *p:pages)p->hide();auto *p=new WebToolView(load(id),root.path(),QString::number(pages.size()),&parent);layout->addWidget(p);pages.append(p);p->show();return waitExpression(p->browser(),"Boolean(document.querySelector('main'))");};
  sample("shell_baseline");QVERIFY(append("json"));sample("one_tool");QVERIFY(append("json"));sample("same_tool_two_sessions");for(int n=0;n<3;++n)QVERIFY(append("json"));sample("same_tool_five_sessions");
  qDeleteAll(pages);pages.clear();QTest::qWait(500);sample("after_release_same_tool");
  for(const auto &id:{"json","file-hash","image","rename","video-clip"}){QVERIFY(append(id));if(pages.size()==2)sample("two_different_tools");}sample("five_different_tools");
  qDeleteAll(pages);pages.clear();QTest::qWait(1000);sample("after_release_all_tools");
  QElapsedTimer catalogTime;auto catalogRoot=root.path()+"/catalog";
  for(int n=0;n<100;++n){auto pkg=catalogRoot+"/scan-"+QString::number(n)+"/1.0.0";QDir().mkpath(pkg+"/frontend");QFile f(pkg+"/frontend/index.html");QVERIFY(f.open(QIODevice::WriteOnly));f.write("<html></html>");f.close();Files::writeObject(pkg+"/manifest.json",{{"manifest_version",2},{"tool_id","scan-"+QString::number(n)},{"name","Scan"},{"package_version","1.0.0"},{"frontend",QJsonObject{{"type","web"},{"entry","frontend/index.html"},{"bridge_version",1},{"state_version",1}}}});}
  catalogTime.start();auto catalog=readCatalog(catalogRoot);QCOMPARE(catalog.tools.size(),100);const auto scanMs=catalogTime.elapsed();sample("scan_100_unopened_packages");
  beat.stop();std::sort(ticks.begin(),ticks.end());auto p95=ticks.isEmpty()?0:ticks[qMin(int(ticks.size()-1),int(ticks.size()*.95))];
  QVERIFY(Files::writeObject(qEnvironmentVariable("MODYN_RESOURCE_REPORT",source+"/.local/logs/IF1-resource-profile.json"),{{"samples",samples},{"scan_100_ms",double(scanMs)},{"heartbeat_10ms_p95_observed_ms",double(p95)},{"note","Background native windows; working sets include shared pages and must not be interpreted as unique physical memory. Full Workbench plus manually retained background tool surfaces. The separate cold process run avoids earlier-test caches. No hardware-wide memory guarantee."}}));
 }

 void videoEditorV2Workflow() {
  qputenv("MODYN_BACKGROUND_VALIDATION","1");const auto source=qEnvironmentVariable("MODYN_SOURCE_ROOT");QTemporaryDir root;QVERIFY(root.isValid());
  const auto package=Files::readObject(source+"/.local/tools/video-clip/latest.json",4096).value("package").toString();QString error;
  const auto tool=definitionFromManifest(package,Files::readObject(package+"/manifest.json",65536),&error);QVERIFY2(error.isEmpty(),qPrintable(error));
  const auto data=root.path()+"/data",sample=root.path()+"/editor.mp4";
  QCOMPARE(QProcess::execute(package+"/backend/ffmpeg.exe",{"-v","error","-f","lavfi","-i","testsrc2=size=320x180:rate=25","-t","6","-c:v","libx264","-preset","ultrafast","-threads","2",sample}),0);
  Workbench window({tool},{data,{},QCoreApplication::applicationDirPath()+"/ModynToolbox.WorkerHost.exe"});window.setAttribute(Qt::WA_ShowWithoutActivating);window.setWindowFlag(Qt::Tool);window.move(-20000,-20000);window.show();
  auto *page=qobject_cast<WebToolView*>(window.panel(1));QVERIFY(page);auto *browser=page->browser();QVERIFY(waitExpression(browser,"Boolean(window.videoEditor)"));
  page->restore({{"frontend_state_version",1},{"ui_state",QJsonObject{{"file",QJsonObject{{"path",sample}}},{"params",QJsonObject{{"start_s",1},{"end_s",3},{"output_dir",root.path()+"/outputs"}}}}},{"input",QJsonObject{{"files",QJsonArray{sample}}}}});
  QVERIFY(waitExpression(browser,"document.querySelector('#player').readyState>=2&&window.videoEditor.range().end===3"));
  auto execute=[&](const QString &script){const auto before=page->jobSummary().value("id");evaluate(browser,script);QElapsedTimer t;t.start();while(t.elapsed()<90000){const auto j=page->jobSummary();if(j.value("id")!=before&&QStringList{"succeeded","failed","cancelled"}.contains(j.value("state").toString()))return j.value("state").toString();QTest::qWait(20);}return QString("timeout");};
  QCOMPARE(execute("document.querySelector('#refresh-thumbs').click()"),QString("succeeded"));
  QVERIFY(waitExpression(browser,"document.querySelectorAll('#thumbnail-strip img').length===16&&[...document.querySelectorAll('#thumbnail-strip img')].every(n=>n.naturalWidth===160&&n.naturalHeight===90)"));
  evaluate(browser,"document.querySelector('#length_s').value='00:00:01.500';document.querySelector('#length_s').dispatchEvent(new Event('change',{bubbles:true}))");
  QVERIFY(waitExpression(browser,"Math.abs(window.videoEditor.range().end-2.5)<.001"));
  evaluate(browser,"document.querySelector('#reset').click();document.querySelector('#undo').click()");QVERIFY(waitExpression(browser,"window.videoEditor.range().end===2.5"));
  evaluate(browser,"document.querySelector('#player').muted=true;document.querySelector('#preview').click()");QVERIFY(waitExpression(browser,"document.querySelector('#player').paused&&document.querySelector('#player').currentTime>=2.4"));
  evaluate(browser,"document.querySelector('#tab-list').click();document.querySelector('#add-clip').click()");QVERIFY(waitExpression(browser,"document.querySelectorAll('.clip-row').length===2"));
  evaluate(browser,"document.querySelector('#tab-edit').click();document.querySelector('#filename').value='frozen-name';document.querySelector('#filename').dispatchEvent(new Event('change',{bubbles:true}))");
  QCOMPARE(execute("document.querySelector('#export').click();document.querySelector('#filename').value='later-name'"),QString("succeeded"));
  QVERIFY(waitExpression(browser,"document.querySelectorAll('.queue-row .succeeded').length===2"));
  auto records=QDir(data+"/tools/video-clip/batches").entryInfoList({"*.json"},QDir::Files,QDir::Time);QVERIFY(!records.isEmpty());
  const auto journal=Files::readObject(records.first().filePath(),2*1024*1024);const auto rows=journal.value("rows").toArray();QCOMPARE(rows.size(),2);for(const auto &v:rows){const auto path=v.toObject().value("file").toString();QVERIFY(QFileInfo::exists(path));QVERIFY(path.contains("frozen-name"));QVERIFY(!path.contains("later-name"));}
  auto jobs=window.jobScheduler()->records().size();window.detachSession(1);window.attachSession(1);QCOMPARE(window.jobScheduler()->records().size(),jobs);
  const auto longVideo=root.path()+"/long.mp4";
  QCOMPARE(QProcess::execute(package+"/backend/ffmpeg.exe",{"-v","error","-f","lavfi","-i","testsrc2=size=960x540:rate=25","-t","70","-c:v","libx264","-preset","ultrafast","-threads","2",longVideo}),0);
  const auto before=page->jobSummary().value("id");
  window.runAction(1,"batch-export",{{"files",QJsonArray{longVideo}},{"params",QJsonObject{{"mode",QStringLiteral("精确裁剪")},{"quality",QStringLiteral("较高画质")},{"output_dir",root.path()+"/cancel"}}},{"clips",QJsonArray{QJsonObject{{"id","first"},{"start_ms",0},{"end_ms",500}},QJsonObject{{"id","second"},{"start_ms",1000},{"end_ms",69000}}}}});
  QElapsedTimer timer;timer.start();bool second=false;
  while(timer.elapsed()<30000){auto j=page->jobSummary();if(j.value("id")!=before&&j.value("message").toString().contains(QStringLiteral("片段 2/2"))){second=true;break;}QTest::qWait(20);}
  QVERIFY(second);emit page->cancelRequested();
  QTRY_COMPARE_WITH_TIMEOUT(page->jobSummary().value("state").toString(),QString("cancelled"),20000);
  records=QDir(data+"/tools/video-clip/batches").entryInfoList({"*.json"},QDir::Files,QDir::Time);auto cancelled=Files::readObject(records.first().filePath(),2*1024*1024);auto cancelledRows=cancelled.value("rows").toArray();QCOMPARE(cancelledRows.first().toObject().value("state").toString(),QString("succeeded"));QVERIFY(QFileInfo::exists(cancelledRows.first().toObject().value("file").toString()));
  evaluate(browser,"document.querySelector('#filename').value='must-not-change-retry'");
  QCOMPARE(execute("document.querySelector('#retry').click()"),QString("succeeded"));
  records=QDir(data+"/tools/video-clip/batches").entryInfoList({"*.json"},QDir::Files,QDir::Time);
  const auto retried=Files::readObject(records.first().filePath(),2*1024*1024).value("rows").toArray();QCOMPARE(retried.size(),1);
  QCOMPARE(retried.first().toObject().value("start_ms").toInt(),1000);QCOMPARE(retried.first().toObject().value("end_ms").toInt(),69000);
  QVERIFY(!retried.first().toObject().value("file").toString().contains("must-not-change-retry"));QVERIFY(QFileInfo::exists(cancelledRows.first().toObject().value("file").toString()));
  QVERIFY(waitExpression(browser,"document.querySelector('#file-info').textContent.includes('editor.mp4')"));
  evaluate(browser,"document.querySelector('.queue-row button').click()");QVERIFY(waitExpression(browser,"document.querySelector('#artifact-dialog').open&&document.querySelector('#artifact-video').readyState>=1"));evaluate(browser,"document.querySelector('#artifact-close').click()");
  const auto neighbor=root.path()+"/cancel/not-an-artifact.txt";QFile secret(neighbor);QVERIFY(secret.open(QIODevice::WriteOnly));secret.write("not granted");secret.close();
  const auto argument=QString::fromUtf8(QJsonDocument(QJsonArray{neighbor}).toJson(QJsonDocument::Compact));
  evaluate(browser,"import('./modyn.js').then(m=>m.modyn.url("+argument+"[0]).then(()=>window.neighborDenied=false,()=>window.neighborDenied=true))");QVERIFY(waitExpression(browser,"window.neighborDenied===true"));
  QVERIFY(Files::writeObject(source+"/.local/logs/VID2-native-workflow.json",{{"success",true},{"thumbnails",16},{"multiple_outputs",2},{"frozen_parameters",true},{"cancel_after_first_commit",true},{"completed_file_retained",true},{"retry_uses_original_snapshot",true},{"output_preview",true},{"neighbor_file_not_granted",true},{"detach_does_not_resubmit",true}}));
 }

 void translationButtonsRespectFontPreference() {
  qputenv("MODYN_BACKGROUND_VALIDATION","1");const auto source=qEnvironmentVariable("MODYN_SOURCE_ROOT");QTemporaryDir root;
  const auto package=Files::readObject(source+"/.local/tools/translate/latest.json",4096).value("package").toString();QString error;
  const auto tool=definitionFromManifest(package,Files::readObject(package+"/manifest.json",65536),&error);QVERIFY(error.isEmpty());
  Workbench window({tool},{root.path()+"/data",{}, {}});window.setAttribute(Qt::WA_ShowWithoutActivating);window.setWindowFlag(Qt::Tool);window.move(-20000,-20000);window.show();
  auto *page=qobject_cast<WebToolView *>(window.panel(1));QVERIFY(page);auto *browser=page->browser();QVERIFY(waitExpression(browser,"Boolean(document.querySelector('button.primary'))"));
  QVERIFY(waitExpression(browser,"Boolean(window.translationTool)"));QTest::qWait(200);
  page->notifyTheme({{"fontSize",18}});
  QVERIFY2(waitExpression(browser,"(()=>{const b=document.querySelector('button.primary'),s=getComputedStyle(b);return s.fontSize==='18px'&&s.lineHeight==='24px'&&b.getBoundingClientRect().height>=40&&document.scrollingElement.scrollWidth<=innerWidth+1})()"),qPrintable(evaluate(browser,"(()=>{const b=document.querySelector('button.primary'),s=getComputedStyle(b);return JSON.stringify({font:s.fontSize,line:s.lineHeight,height:b.getBoundingClientRect().height,root:getComputedStyle(document.documentElement).fontSize})})()")));
 }

 void blueVideoCompactTimeline() {
  qputenv("MODYN_BACKGROUND_VALIDATION","1");Theme::apply();const auto source=qEnvironmentVariable("MODYN_SOURCE_ROOT");QTemporaryDir root;QVERIFY(root.isValid());
  const auto package=Files::readObject(source+"/.local/tools/video-clip/latest.json",4096).value("package").toString();QString error;
  const auto tool=definitionFromManifest(package,Files::readObject(package+"/manifest.json",65536),&error);QVERIFY2(error.isEmpty(),qPrintable(error));
  Workbench window({tool},{root.path()+"/data",{}, {}});window.setAttribute(Qt::WA_ShowWithoutActivating);window.setWindowFlag(Qt::Tool);window.move(-20000,-20000);window.show();
  auto *page=qobject_cast<WebToolView *>(window.panel(1));QVERIFY(page);auto *browser=page->browser();QVERIFY(waitExpression(browser,"Boolean(document.querySelector('#trim-track'))"));
  const auto sample=root.path()+"/timeline.mp4";
  QCOMPARE(QProcess::execute(package+"/backend/ffmpeg.exe",{"-v","error","-f","lavfi","-i","testsrc2=size=640x360:rate=10","-t","60","-c:v","libx264","-preset","ultrafast","-pix_fmt","yuv420p",sample}),0);
  page->restore({{"frontend_state_version",1},{"ui_state",QJsonObject{{"file",QJsonObject{{"path",sample}}},{"params",QJsonObject{{"start_s","0"},{"end_s","60"}}}}},{"input",QJsonObject{{"files",QJsonArray{sample}}}}});
  page->setResult({{"context",QJsonObject{{"media",QJsonObject{{"duration_ms",60000},{"audio_tracks",QJsonArray{}}}}}}});
  QVERIFY(waitExpression(browser,"document.querySelector('#player').readyState>=2"));
  QVERIFY(waitExpression(browser,"Number(document.querySelector('#out-range').max)===60"));
  for(const auto &size:{QSize(1200,780),QSize(900,600),QSize(760,480)}) {
   window.resize(size);QTest::qWait(150);
   const auto check=QString(R"JS((()=>{const ids=['add','trim-track','start_s','end_s','export'];return ids.every(id=>{const r=document.getElementById(id).getBoundingClientRect();return r.x>=-1&&r.right<=innerWidth+1&&r.y>=0&&r.bottom<=innerHeight+1})&&document.scrollingElement.scrollWidth<=innerWidth+1&&document.scrollingElement.scrollHeight<=innerHeight+1})())JS");
   QVERIFY2(waitExpression(browser,check),qPrintable(evaluate(browser,"JSON.stringify({w:innerWidth,h:innerHeight,scroll:document.scrollingElement.scrollHeight,export:document.querySelector('#export').getBoundingClientRect().toJSON()})")));
   QVERIFY(waitExpression(browser,R"JS((()=>{const b=document.querySelector('#export'),s=getComputedStyle(b),r=b.getBoundingClientRect();return s.fontFamily.includes('Microsoft YaHei')&&s.fontSize==='14px'&&s.lineHeight==='20px'&&Number(s.fontWeight)<=500&&r.height>=32&&r.height<=40&&r.width>b.scrollWidth-1})())JS"));
   bool captured=false;browser->capture(source+"/.local/logs/UIR10-video-"+QString::number(size.width())+".png",[&](bool ok){captured=ok;});QTRY_VERIFY_WITH_TIMEOUT(captured,10000);
  }
  QVERIFY(nativeInput(browser,"Emulation.setFocusEmulationEnabled",{{"enabled",true}}));
  auto drag=[&](const QString &selector,qreal from,qreal to) {
   const auto coords=QJsonDocument::fromJson(evaluate(browser,QString(R"JS((()=>{const r=document.querySelector('#trim-track').getBoundingClientRect();const el=document.querySelector('%1').getBoundingClientRect();return {left:r.left,width:r.width,y:el.top+el.height/2}})())JS").arg(selector)).toUtf8()).object();
   const auto y=coords.value("y").toDouble();const auto a=coords.value("left").toDouble()+coords.value("width").toDouble()*from;const auto b=coords.value("left").toDouble()+coords.value("width").toDouble()*to;
   return nativeInput(browser,"Input.dispatchMouseEvent",{{"type","mousePressed"},{"x",a},{"y",y},{"button","left"},{"buttons",1},{"clickCount",1}})&&nativeInput(browser,"Input.dispatchMouseEvent",{{"type","mouseMoved"},{"x",b},{"y",y},{"button","left"},{"buttons",1}})&&nativeInput(browser,"Input.dispatchMouseEvent",{{"type","mouseReleased"},{"x",b},{"y",y},{"button","left"},{"buttons",0}});
  };
  QVERIFY(drag("#in-range",0,.2));QVERIFY2(waitExpression(browser,"window.videoEditor.parseTime(document.querySelector('#start_s').value)>5"),qPrintable(evaluate(browser,"document.querySelector('#start_s').value")));
  QVERIFY(drag("#playhead",0,.4));QVERIFY(waitExpression(browser,"document.querySelector('#player').currentTime>20&&document.querySelector('#player').currentTime<30"));
  QVERIFY(drag("#out-range",1,.7));QVERIFY(waitExpression(browser,"window.videoEditor.parseTime(document.querySelector('#end_s').value)<55&&window.videoEditor.parseTime(document.querySelector('#end_s').value)>25"));
  evaluate(browser,"document.querySelector('#start_s').value='12.345';document.querySelector('#start_s').dispatchEvent(new Event('change',{bubbles:true}))");
  QVERIFY(waitExpression(browser,"Math.abs(Number(document.querySelector('#in-range').value)-12.345)<0.01"));
  evaluate(browser,"document.querySelector('#settings-toggle').click()");QVERIFY(waitExpression(browser,"document.querySelector('#export-settings').classList.contains('open')"));
  bool captured=false;browser->capture(source+"/.local/logs/UIR10-video-settings.png",[&](bool ok){captured=ok;});QTRY_VERIFY_WITH_TIMEOUT(captured,10000);
  window.resize(1200,780);QTest::qWait(400);bool fullCaptured=false;
  browser->capture(source+"/.local/logs/UIR10-app-page.png",[&](bool ok){fullCaptured=ok;});QTRY_VERIFY_WITH_TIMEOUT(fullCaptured,10000);
  auto image=window.grab().toImage();image.setDevicePixelRatio(1.0);QPainter painter(&image);const auto origin=browser->mapTo(&window,QPoint());const auto scale=window.devicePixelRatioF();
  painter.drawImage(QRect(qRound(origin.x()*scale),qRound(origin.y()*scale),qRound(browser->width()*scale),qRound(browser->height()*scale)),QImage(source+"/.local/logs/UIR10-app-page.png"));painter.end();QVERIFY(image.save(source+"/.local/logs/UIR10-app-blue.png"));
  auto field=QJsonDocument::fromJson(evaluate(browser,"(()=>{const n=document.querySelector('#quality');n.focus();window.qualityBefore=n.value;const r=n.getBoundingClientRect();return {x:r.x+r.width/2,y:r.y+r.height/2}})()").toUtf8()).object();
  QVERIFY(nativeInput(browser,"Input.dispatchMouseEvent",{{"type","mouseWheel"},{"x",field.value("x")},{"y",field.value("y")},{"deltaX",0},{"deltaY",120}}));
  QVERIFY(waitExpression(browser,"document.activeElement.id==='quality'&&document.querySelector('#quality').value===window.qualityBefore"));
  QVERIFY(nativeInput(browser,"Input.dispatchKeyEvent",{{"type","keyDown"},{"key","ArrowDown"},{"code","ArrowDown"},{"windowsVirtualKeyCode",40}}));
  QVERIFY(nativeInput(browser,"Input.dispatchKeyEvent",{{"type","keyUp"},{"key","ArrowDown"},{"code","ArrowDown"},{"windowsVirtualKeyCode",40}}));
  QVERIFY(waitExpression(browser,"document.querySelector('#quality').value!==window.qualityBefore"));

 }

};
QTEST_MAIN(WebFrontendTests)
#include "test_web_frontend.moc"
