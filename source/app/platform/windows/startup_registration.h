#pragma once
#include <QString>
struct StartupTarget {QString program,dataRoot,packagesRoot;};
struct StartupStatus {
    bool exists=false,owned=false,current=false;
    QString error,program,arguments,workingDirectory,linkPath;
};
// No writes until setEnabled() is explicitly requested from Settings.
class StartupRegistration {
public:
    explicit StartupRegistration(StartupTarget target,QString startupFolder = {});
    StartupStatus status() const;
    QString setEnabled(bool enabled);
    QString arguments() const;
    QString linkPath() const;
private:
    StartupTarget target;
    QString folder,folderError;
};
