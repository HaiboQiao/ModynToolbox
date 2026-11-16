#include "startup_registration.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QUuid>
#define NOMINMAX
#include <windows.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <wrl/client.h>
using Microsoft::WRL::ComPtr;
namespace {
constexpr auto marker=L"ModynToolbox managed startup v1";
struct ComApartment {HRESULT result=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);~ComApartment(){if(SUCCEEDED(result))CoUninitialize();}};
QString failure(const QString &text,HRESULT hr){return text+QStringLiteral("（%1）").arg(quint32(hr),8,16,QChar('0'));}
QString path(const QString &s){return QDir::toNativeSeparators(QFileInfo(s).absoluteFilePath());}
QString quote(const QString &value){
    QString out="\"";int slashes=0;
    for(auto c:value){if(c=='\\'){++slashes;continue;}out+=QString(slashes*(c=='\"'?2:1)+(c=='\"'?1:0),'\\');slashes=0;out+=c;}
    return out+QString(slashes*2,'\\')+"\"";
}
HRESULT shellLink(ComPtr<IShellLinkW> &link){return CoCreateInstance(CLSID_ShellLink,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&link));}
}
StartupRegistration::StartupRegistration(StartupTarget value,QString startupFolder):target(std::move(value)),folder(std::move(startupFolder)) {
    target.program=path(target.program);target.dataRoot=path(target.dataRoot);target.packagesRoot=path(target.packagesRoot);
    if(folder.isEmpty()){PWSTR value=nullptr;const auto hr=SHGetKnownFolderPath(FOLDERID_Startup,KF_FLAG_DONT_VERIFY,nullptr,&value);if(SUCCEEDED(hr))folder=QString::fromWCharArray(value);else folderError=failure(QStringLiteral("无法定位当前用户启动目录"),hr);CoTaskMemFree(value);}
}
QString StartupRegistration::arguments() const{return "--autostart --data-root "+quote(target.dataRoot)+" --packages-root "+quote(target.packagesRoot);}
QString StartupRegistration::linkPath() const{return folder.isEmpty()?QString():QDir(folder).absoluteFilePath("ModynToolbox.lnk");}
StartupStatus StartupRegistration::status() const {
    StartupStatus state;state.linkPath=linkPath();state.error=folderError;if(!state.error.isEmpty())return state;
    // QFileInfo follows Windows shortcuts; even a broken/foreign .lnk must count as an existing file.
    const auto attributes=GetFileAttributesW(reinterpret_cast<LPCWSTR>(QDir::toNativeSeparators(state.linkPath).utf16()));
    if(attributes==INVALID_FILE_ATTRIBUTES){const auto error=GetLastError();if(error!=ERROR_FILE_NOT_FOUND&&error!=ERROR_PATH_NOT_FOUND)state.error=QStringLiteral("无法检查启动项（%1），未改变系统设置。").arg(error);return state;}
    state.exists=true;
    if(attributes&FILE_ATTRIBUTE_DIRECTORY){state.error=QStringLiteral("启动项路径被同名文件夹占用，已保留。");return state;}
    ComApartment com;if(FAILED(com.result)){state.error=failure(QStringLiteral("无法读取启动项"),com.result);return state;}
    ComPtr<IShellLinkW> link;auto hr=shellLink(link);ComPtr<IPersistFile> file;if(SUCCEEDED(hr))hr=link.As(&file);if(SUCCEEDED(hr))hr=file->Load(reinterpret_cast<LPCWSTR>(QDir::toNativeSeparators(state.linkPath).utf16()),STGM_READ);
    if(FAILED(hr)){state.error=failure(QStringLiteral("同名启动项无法读取，已保留原文件"),hr);return state;}
    wchar_t description[256]{};link->GetDescription(description,256);state.owned=QString::fromWCharArray(description)==QString::fromWCharArray(marker);
    wchar_t buffer[32768]{};hr=link->GetPath(buffer,32768,nullptr,SLGP_RAWPATH);if(SUCCEEDED(hr))state.program=QString::fromWCharArray(buffer);
    hr=link->GetArguments(buffer,32768);if(SUCCEEDED(hr))state.arguments=QString::fromWCharArray(buffer);
    hr=link->GetWorkingDirectory(buffer,32768);if(SUCCEEDED(hr))state.workingDirectory=QString::fromWCharArray(buffer);
    state.current=state.owned&&path(state.program).compare(target.program,Qt::CaseInsensitive)==0&&state.arguments==arguments();
    if(!state.owned)state.error=QStringLiteral("启动目录已有同名快捷方式，非本程序管理；请先检查该文件。");return state;
}
QString StartupRegistration::setEnabled(bool enabled) {
    const auto before=status();if(!before.error.isEmpty())return before.error;
    if(!enabled){if(!before.exists)return {};if(!before.owned)return QStringLiteral("原启动项不属于本程序，未删除。");if(!QFile::remove(before.linkPath))return QStringLiteral("无法移除启动项，请检查启动目录权限。");return {};}
    if(!QFileInfo(target.program).isFile())return QStringLiteral("当前程序文件不存在，未设置自启动。");
    if(!QDir().mkpath(folder))return QStringLiteral("无法写入当前用户启动目录。");
    ComApartment com;if(FAILED(com.result))return failure(QStringLiteral("无法创建启动项"),com.result);
    ComPtr<IShellLinkW> link;auto hr=shellLink(link);auto working=path(QFileInfo(target.program).absolutePath());auto args=arguments();
    if(SUCCEEDED(hr))hr=link->SetPath(reinterpret_cast<LPCWSTR>(target.program.utf16()));
    if(SUCCEEDED(hr))hr=link->SetArguments(reinterpret_cast<LPCWSTR>(args.utf16()));
    if(SUCCEEDED(hr))hr=link->SetWorkingDirectory(reinterpret_cast<LPCWSTR>(working.utf16()));
    if(SUCCEEDED(hr))hr=link->SetDescription(marker);
    if(SUCCEEDED(hr))hr=link->SetIconLocation(reinterpret_cast<LPCWSTR>(target.program.utf16()),0);
    if(SUCCEEDED(hr))hr=link->SetShowCmd(SW_SHOWNORMAL);
    ComPtr<IPersistFile> file;if(SUCCEEDED(hr))hr=link.As(&file);
    const auto stage=QDir::toNativeSeparators(linkPath()+".tmp-"+QUuid::createUuid().toString(QUuid::WithoutBraces));
    if(SUCCEEDED(hr))hr=file->Save(reinterpret_cast<LPCWSTR>(stage.utf16()),TRUE);
    if(FAILED(hr)){QFile::remove(stage);return failure(QStringLiteral("启动项保存失败，原启动项未改变"),hr);}
    if(!MoveFileExW(reinterpret_cast<LPCWSTR>(stage.utf16()),reinterpret_cast<LPCWSTR>(QDir::toNativeSeparators(linkPath()).utf16()),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){
        const auto error=GetLastError();QFile::remove(stage);return QStringLiteral("启动项替换失败（%1），原启动项已保留。").arg(error);
    }
    const auto after=status();return after.current?QString():QStringLiteral("启动项写入后核对失败：")+after.error;
}
