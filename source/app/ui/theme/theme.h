#pragma once
#include <QColor>
namespace Theme {
inline const QColor background("#f3f7fc");
inline const QColor navigation("#eff5fd");
inline const QColor panel("#ffffff");
inline const QColor text("#142238");
inline const QColor muted("#61718a");
inline const QColor border("#d5e1f0");
inline const QColor accent("#0866f5");
inline const QColor selection("#dceaff");
inline const QColor closeHover("#e81123");
inline const QColor closePressed("#c42b1c");
inline const QColor videoBackground("#122034");
inline const QColor videoForeground("#e1ebfa");
inline constexpr int sidebarWidth = 190;
inline constexpr int pageInset = 24;
inline constexpr int panelInset = 20;
inline constexpr int gap = 12;
inline constexpr int titleBarHeight = 62;
inline constexpr int titleBrandSpacing = 3;
inline constexpr int navigationIndent = 16;
inline constexpr int navigationActionWidth = 28;
void apply();
void setPreferences(int fontSize, bool compact);
} // namespace Theme
