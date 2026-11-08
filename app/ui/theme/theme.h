#pragma once
#include <QColor>
namespace Theme {
inline const QColor background("#f4f3fa");
inline const QColor navigation("#f2effc");
inline const QColor panel("#ffffff");
inline const QColor text("#292735");
inline const QColor muted("#6c687c");
inline const QColor border("#ddd7e9");
inline const QColor accent("#7054d8");
inline const QColor selection("#ece7fd");
inline constexpr int sidebarWidth = 190;
inline constexpr int pageInset = 24;
inline constexpr int panelInset = 20;
inline constexpr int gap = 12;
inline constexpr int titleBarHeight = 54;
void apply();
}
