#pragma once
#include <QIcon>
#include <QColor>
enum class UiIcon { Search, Refresh, Add, External, Return, Folder, Image, Code, Files, Minimize, Maximize, Restore, Close };
QIcon uiIcon(UiIcon icon, const QColor &color);
QIcon brandIcon();
