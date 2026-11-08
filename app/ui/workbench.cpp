#include "workbench.h"
#include "platform/windows/window_theme.h"
#include <QApplication>
#include <QCloseEvent>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <functional>
#include <algorithm>
namespace {
class ToolWindow : public QMainWindow {
public:
    std::function<void()> returnToHost;
protected:
    void closeEvent(QCloseEvent *event) override {
        event->ignore();
        if (returnToHost) returnToHost();
    }
};
}
Workbench::Workbench(QVector<ToolDefinition> tools, QWidget *parent)
    : QMainWindow(parent), definitions(std::move(tools)) {
    setWindowTitle(QStringLiteral("石墨工具箱"));
    resize(1060, 700);
    auto *root = new QWidget;
    auto *layout = new QHBoxLayout(root);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    auto *sidebar = new QWidget;
    sidebar->setObjectName("sidebar");
    sidebar->setFixedWidth(190);
    auto *sideLayout = new QVBoxLayout(sidebar);
    sideLayout->setContentsMargins(14, 20, 14, 16);
    sideLayout->addWidget(new QLabel(QStringLiteral("石 墨")));
    auto *search = new QLineEdit;
    search->setObjectName("toolSearch");
    search->setPlaceholderText(QStringLiteral("搜索工具"));
    sideLayout->addWidget(search);
    navigation = new QListWidget;
    navigation->setObjectName("toolNavigation");
    sideLayout->addWidget(navigation);
    auto *noMatches = new QLabel(QStringLiteral("没有匹配的工具"));
    noMatches->hide();
    sideLayout->addWidget(noMatches);
    auto *content = new QWidget;
    auto *contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    selector = new QComboBox;
    selector->setObjectName("sessionSelector");
    selector->hide();
    contentLayout->addWidget(selector);
    workspace = new QStackedWidget;
    contentLayout->addWidget(workspace, 1);
    floatingNotice = new QWidget;
    floatingNotice->setObjectName("floatingNotice");
    auto *noticeLayout = new QVBoxLayout(floatingNotice);
    noticeLayout->addStretch();
    auto *notice = new QLabel(QStringLiteral("此会话已在独立窗口打开"));
    notice->setAlignment(Qt::AlignCenter);
    noticeLayout->addWidget(notice);
    auto *returnButton = new QPushButton(QStringLiteral("收回主窗口"));
    noticeLayout->addWidget(returnButton, 0, Qt::AlignHCenter);
    noticeLayout->addStretch();
    workspace->addWidget(floatingNotice);
    connect(returnButton, &QPushButton::clicked, this, [this] {
        attachSession(recentSessions.value(currentTool));
    });
    layout->addWidget(sidebar);
    layout->addWidget(content, 1);
    setCentralWidget(root);
    qApp->setStyleSheet(QStringLiteral(
        "QWidget{background:#25282e;color:#eff1f5;font-family:'Segoe UI','Microsoft YaHei UI';font-size:14px;}"
        "QWidget#sidebar,QListWidget{background:#1e2126;}"
        "QLineEdit,QPlainTextEdit,QComboBox{background:#2c3037;border:1px solid #748094;border-radius:5px;padding:8px;}"
        "QPushButton{background:#363e4a;border:1px solid #748094;border-radius:5px;padding:8px 12px;}"
        "QPushButton:hover{background:#454f5d;}QPushButton:focus{border:2px solid #b6c6e5;}"
        "QListWidget{border:0;}QListWidget::item{padding:12px 6px;}QListWidget::item:selected{background:#363e4a;}"
        "QLabel#heading{font-size:20px;font-weight:600;}"
    ));
    for (const auto &tool : definitions) {
        auto *item = new QListWidgetItem(tool.name, navigation);
        item->setData(Qt::UserRole, tool.id);
        item->setData(Qt::UserRole + 1, tool.description);
    }
    connect(navigation, &QListWidget::currentRowChanged, this, [this](int row) {
        if (auto *item = navigation->item(row)) openTool(item->data(Qt::UserRole).toString());
    });
    connect(search, &QLineEdit::textChanged, this, [this, noMatches](const QString &query) {
        int matches = 0;
        for (int row = 0; row < navigation->count(); ++row) {
            auto *item = navigation->item(row);
            const bool visible = (item->text() + " " + item->data(Qt::UserRole + 1).toString()).contains(query.trimmed(), Qt::CaseInsensitive);
            item->setHidden(!visible);
            if (visible) ++matches;
        }
        noMatches->setVisible(matches == 0);
    });
    connect(selector, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (index >= 0) activateSession(selector->itemData(index).toInt());
    });
    if (!definitions.isEmpty()) navigation->setCurrentRow(0);
    else contentLayout->addWidget(new QLabel(QStringLiteral("工具目录为空，请添加兼容工具包后重新打开。")));
    applyDarkFrame(this);
}
Workbench::~Workbench() {
    for (auto &session : sessions) {
        if (session.floating) {
            session.floating->takeCentralWidget();
            delete session.view;
            delete session.floating;
        }
    }
}
int Workbench::sessionCount() const { return sessions.size(); }
QWidget *Workbench::sessionView(int id) const { return sessions.value(id).view; }
QPlainTextEdit *Workbench::editor(int id) const { return sessions.value(id).editor; }
bool Workbench::isDetached(int id) const { return sessions.value(id).floating != nullptr; }
void Workbench::openTool(const QString &toolId) {
    currentTool = toolId;
    if (recentSessions.contains(toolId)) activateSession(recentSessions.value(toolId));
    else addSession();
}
void Workbench::updateSelector(int activeId) {
    const QSignalBlocker blocker(selector);
    selector->clear();
    for (auto it = sessions.cbegin(); it != sessions.cend(); ++it) {
        if (it->toolId == currentTool) {
            selector->addItem(QStringLiteral("会话 %1%2").arg(it.key()).arg(it->floating ? QStringLiteral(" · 独立窗口") : QString()), it.key());
        }
    }
    selector->setCurrentIndex(selector->findData(activeId));
    selector->setVisible(selector->count() > 1);
}
void Workbench::activateSession(int id) {
    auto it = sessions.find(id);
    if (it == sessions.end()) return;
    currentTool = it->toolId;
    recentSessions[currentTool] = id;
    const QSignalBlocker navigationBlocker(navigation);
    for (int row = 0; row < navigation->count(); ++row) {
        if (navigation->item(row)->data(Qt::UserRole).toString() == currentTool) {
            navigation->setCurrentRow(row);
            break;
        }
    }
    updateSelector(id);
    if (it->floating) {
        workspace->setCurrentWidget(floatingNotice);
        it->floating->showNormal();
        it->floating->raise();
        it->floating->activateWindow();
    } else workspace->setCurrentWidget(it->view);
}
int Workbench::addSession() {
    const auto tool = std::find_if(definitions.cbegin(), definitions.cend(), [this](const auto &entry) { return entry.id == currentTool; });
    if (tool == definitions.cend()) return 0;
    const int id = nextId++;
    auto *view = new QWidget;
    auto *layout = new QVBoxLayout(view);
    layout->setContentsMargins(28, 24, 28, 24);
    auto *header = new QHBoxLayout;
    auto *heading = new QLabel(tool->name);
    heading->setObjectName("heading");
    header->addWidget(heading);
    header->addStretch();
    auto *create = new QPushButton(QStringLiteral("新建会话"));
    header->addWidget(create);
    auto *detach = new QPushButton(QStringLiteral("独立窗口"));
    detach->setObjectName("locationAction");
    header->addWidget(detach);
    layout->addLayout(header);
    auto *description = new QLabel(tool->description);
    description->setWordWrap(true);
    layout->addWidget(description);
    auto *input = new QPlainTextEdit;
    input->setPlaceholderText(QStringLiteral("工具界面接入预览 · 业务功能在对应阶段提供"));
    layout->addWidget(input, 1);
    layout->addWidget(new QLabel(QStringLiteral("工具包 v%1 · 业务尚待接入").arg(tool->version)));
    sessions.insert(id, {tool->id, view, input, nullptr});
    workspace->addWidget(view);
    connect(create, &QPushButton::clicked, this, [this, toolId=tool->id] { currentTool = toolId; addSession(); });
    connect(detach, &QPushButton::clicked, this, [this,id] {
        if (isDetached(id)) attachSession(id); else detachSession(id);
    });
    activateSession(id);
    return id;
}
void Workbench::detachSession(int id) {
    auto it = sessions.find(id);
    if (it == sessions.end() || it->floating) return;
    auto *window = new ToolWindow;
    window->setWindowTitle(it->view->findChild<QLabel *>("heading")->text() + QStringLiteral(" · 会话 %1").arg(id));
    window->resize(760, 560);
    window->returnToHost = [this,id] { attachSession(id); };
    workspace->removeWidget(it->view);
    window->setCentralWidget(it->view);
    it->floating = window;
    it->view->findChild<QPushButton *>("locationAction")->setText(QStringLiteral("收回主窗口"));
    it->view->show();
    applyDarkFrame(window);
    window->show();
    workspace->setCurrentWidget(floatingNotice);
    updateSelector(id);
}
void Workbench::attachSession(int id) {
    auto it = sessions.find(id);
    if (it == sessions.end() || !it->floating) return;
    auto *window = it->floating;
    window->takeCentralWidget();
    it->view->findChild<QPushButton *>("locationAction")->setText(QStringLiteral("独立窗口"));
    it->floating = nullptr;
    workspace->addWidget(it->view);
    window->hide();
    window->deleteLater();
    activateSession(id);
}
