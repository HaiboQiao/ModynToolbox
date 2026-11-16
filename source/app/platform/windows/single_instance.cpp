#include "single_instance.h"
#include <QApplication>
#include <QCryptographicHash>
#include <QElapsedTimer>
#include <QThread>
#include <QTimer>
#include <QWidget>
#define NOMINMAX
#include <windows.h>
namespace {
const wchar_t *brokerClass = L"ModynToolbox.InstanceBroker.v1";
constexpr LPARAM protocol = 0x4d544201;
UINT activationMessage() { static const UINT value=RegisterWindowMessageW(L"ModynToolbox.Activate.v1");return value; }
QString userIdentity() {
    HANDLE token=nullptr;if(!OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&token))return {};
    DWORD size=0;GetTokenInformation(token,TokenUser,nullptr,0,&size);QByteArray bytes(size,0);
    const bool ok=GetTokenInformation(token,TokenUser,bytes.data(),size,&size);CloseHandle(token);
    if(!ok)return {};auto *user=reinterpret_cast<TOKEN_USER*>(bytes.data());
    return QString::fromLatin1(QCryptographicHash::hash(QByteArray(static_cast<const char*>(user->User.Sid),GetLengthSid(user->User.Sid)),QCryptographicHash::Sha256).toHex());
}
}
struct SingleInstance::Impl {
    QString scope,error;HANDLE mutex=nullptr;HWND broker=nullptr;
    bool pending=false;std::function<void()> handler;
    static LRESULT CALLBACK procedure(HWND window,UINT message,WPARAM wp,LPARAM lp) {
        auto *self=reinterpret_cast<SingleInstance*>(GetWindowLongPtrW(window,GWLP_USERDATA));
        if(message==WM_NCCREATE){self=static_cast<SingleInstance*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);SetWindowLongPtrW(window,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));}
        if(self&&message==activationMessage()&&lp==protocol){if(wp==1)self->activationRequested();return 1;}
        return DefWindowProcW(window,message,wp,lp);
    }
};
SingleInstance::SingleInstance(QString scope,QObject *parent):QObject(parent),impl(std::make_unique<Impl>()) {impl->scope=std::move(scope);}
SingleInstance::~SingleInstance(){if(impl->broker)DestroyWindow(impl->broker);if(impl->mutex)CloseHandle(impl->mutex);}
QString SingleInstance::errorString() const{return impl->error;}
SingleInstance::Result SingleInstance::acquire(bool activateExisting) {
    const auto identity=userIdentity();if(identity.isEmpty()){impl->error=QStringLiteral("无法确认当前Windows用户，未启动第二个实例。");return Result::Error;}
    const auto suffix=QString::fromLatin1(QCryptographicHash::hash((identity+":"+impl->scope).toUtf8(),QCryptographicHash::Sha256).toHex());
    const auto mutexName=("Local\\ModynToolbox-"+suffix).toStdWString(),brokerName=("ModynToolbox-"+suffix).toStdWString();
    QElapsedTimer timer;timer.start();
    do {
        SetLastError(ERROR_SUCCESS);auto handle=CreateMutexW(nullptr,FALSE,mutexName.c_str());const auto error=GetLastError();
        if(!handle){impl->error=QStringLiteral("单实例保护初始化失败（%1）。").arg(error);return Result::Error;}
        if(error!=ERROR_ALREADY_EXISTS){
            impl->mutex=handle;WNDCLASSW type{};type.lpfnWndProc=Impl::procedure;type.hInstance=GetModuleHandleW(nullptr);type.lpszClassName=brokerClass;
            if(!RegisterClassW(&type)&&GetLastError()!=ERROR_CLASS_ALREADY_EXISTS){impl->error=QStringLiteral("单实例消息窗口注册失败。");return Result::Error;}
            impl->broker=CreateWindowExW(0,brokerClass,brokerName.c_str(),0,0,0,0,0,HWND_MESSAGE,nullptr,type.hInstance,this);
            if(!impl->broker){impl->error=QStringLiteral("单实例消息窗口创建失败。");return Result::Error;}return Result::Primary;
        }
        CloseHandle(handle);
        if(!activateExisting)return Result::Secondary;
        auto broker=FindWindowExW(HWND_MESSAGE,nullptr,brokerClass,brokerName.c_str());
        if(broker){DWORD pid=0;GetWindowThreadProcessId(broker,&pid);if(pid)AllowSetForegroundWindow(pid);DWORD_PTR result=0;
            if(SendMessageTimeoutW(broker,activationMessage(),1,protocol,SMTO_ABORTIFHUNG|SMTO_BLOCK|SMTO_ERRORONEXIT,100,&result)&&result==1)return Result::Secondary;
        }
        QThread::msleep(25);
    }while(timer.elapsed()<4000);
    impl->error=QStringLiteral("ModynToolbox已在运行，但暂时没有响应。请稍后再次打开，或检查已有窗口。");return Result::Unresponsive;
}
void SingleInstance::activationRequested(){QTimer::singleShot(0,this,[this]{if(impl->handler)impl->handler();else impl->pending=true;});}
void SingleInstance::setActivationHandler(std::function<void()> handler){impl->handler=std::move(handler);if(impl->pending){impl->pending=false;activationRequested();}}
void restoreWorkbenchWindow(QWidget *window) {
    if(!window)return;
    // Validation windows must never acquire foreground focus, including via a duplicate launch.
    const bool background=qEnvironmentVariableIsSet("MODYN_BACKGROUND_VALIDATION");
    window->setWindowState(window->windowState()&~Qt::WindowMinimized);window->show();
    if(background)return;
    auto *target=QApplication::activeModalWidget();if(!target)target=window;
    target->raise();target->activateWindow();
    if(!SetForegroundWindow(reinterpret_cast<HWND>(target->winId())))QApplication::alert(target);
}
