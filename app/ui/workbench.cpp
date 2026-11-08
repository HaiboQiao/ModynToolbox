#include "workbench.h"
#include "components/icons.h"
#include "components/tool_view.h"
#include "theme/theme.h"
#include "view_spec.h"
#include <QCloseEvent>
#include <QComboBox>
#include "ui/components/combo_box.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QShortcut>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QToolButton>
#include <algorithm>
#include <functional>
namespace {
class ToolWindow : public ChromeWindow {
public:
    ToolWindow() {
        auto *close=findChild<QToolButton *>("windowClose");
        close->setAccessibleName(QStringLiteral("关闭并收回主窗口"));
        close->setToolTip(QStringLiteral("关闭并收回主窗口"));
    }
    std::function<void()> returnToHost;
protected:
    void closeEvent(QCloseEvent *event) override {
        event->ignore();
        if (returnToHost) returnToHost();
    }
};
UiIcon navigationIcon(const QString &icon) {
    if (icon == "image") return UiIcon::Image;
    if (icon == "code") return UiIcon::Code;
    return UiIcon::Files;
}
}
Workbench::Workbench(QVector<ToolDefinition> tools,QWidget *parent)
    : ChromeWindow(parent),definitions(std::move(tools)) {
    Theme::apply();
    resize(1200,780);
    setMinimumSize(920,600);
    auto *root = new QWidget;
    root->setObjectName("workbenchRoot");
    auto *layout = new QHBoxLayout(root);
    layout->setContentsMargins(0,0,0,0);
    layout->setSpacing(0);
    auto *sidebar = new QWidget;
    sidebar->setObjectName("sidebar");
    sidebar->setFixedWidth(Theme::sidebarWidth);
    auto *sideLayout = new QVBoxLayout(sidebar);
    sideLayout->setContentsMargins(14,20,14,16);
    sideLayout->setSpacing(Theme::gap);
    auto *search = new QLineEdit;
    search->setObjectName("toolSearch");
    search->setAccessibleName(QStringLiteral("搜索工具"));
    search->setPlaceholderText(QStringLiteral("搜索工具"));
    search->addAction(uiIcon(UiIcon::Search,Theme::muted),QLineEdit::LeadingPosition);
    search->setClearButtonEnabled(true);
    sideLayout->addWidget(search);
    auto *section = new QLabel(QStringLiteral("工具"));
    section->setObjectName("secondary");
    sideLayout->addWidget(section);
    navigation = new QListWidget;
    navigation->setObjectName("toolNavigation");
    navigation->setAccessibleName(QStringLiteral("工具列表"));
    navigation->setIconSize(QSize(18,18));
    navigation->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    sideLayout->addWidget(navigation,1);
    auto *noMatches = new QLabel(QStringLiteral("没有匹配的工具"));
    noMatches->setObjectName("secondary");
    noMatches->setWordWrap(true);
    noMatches->hide();
    sideLayout->addWidget(noMatches);
    auto *version = new QLabel(QStringLiteral("%1 个本地工具").arg(definitions.size()));
    version->setObjectName("secondary");
    sideLayout->addWidget(version);
    auto *content = new QWidget;
    auto *contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(Theme::pageInset,Theme::pageInset,Theme::pageInset,Theme::pageInset);
    contentLayout->setSpacing(Theme::gap);
    selector = new UiComboBox;
    selector->setObjectName("sessionSelector");
    selector->setAccessibleName(QStringLiteral("当前工具会话"));
    selector->hide();
    contentLayout->addWidget(selector,0,Qt::AlignRight);
    workspace = new QStackedWidget;
    contentLayout->addWidget(workspace,1);
    floatingNotice = new QWidget;
    floatingNotice->setObjectName("floatingNotice");
    auto *noticeLayout = new QVBoxLayout(floatingNotice);
    noticeLayout->addStretch();
    auto *notice = new QLabel(QStringLiteral("此会话正在独立窗口中使用"));
    notice->setAlignment(Qt::AlignCenter);
    notice->setObjectName("secondary");
    noticeLayout->addWidget(notice);
    auto *returnButton = new QPushButton(uiIcon(UiIcon::Return,Theme::accent),QStringLiteral("收回主窗口"));
    noticeLayout->addWidget(returnButton,0,Qt::AlignHCenter);
    noticeLayout->addStretch();
    workspace->addWidget(floatingNotice);
    connect(returnButton,&QPushButton::clicked,this,[this] { attachSession(recentSessions.value(currentTool)); });
    layout->addWidget(sidebar);
    layout->addWidget(content,1);
    setContentWidget(root);
    for (const auto &tool : definitions) {
        auto *item = new QListWidgetItem(uiIcon(navigationIcon(tool.icon),Theme::accent),tool.name,navigation);
        item->setData(Qt::UserRole,tool.id);
        item->setData(Qt::UserRole+1,tool.description);
        item->setToolTip(tool.name);
    }
    connect(navigation,&QListWidget::currentRowChanged,this,[this](int row) {
        if (auto *item = navigation->item(row)) openTool(item->data(Qt::UserRole).toString());
    });
    connect(search,&QLineEdit::textChanged,this,[this,noMatches](const QString &query) {
        int matches = 0;
        for (int row=0;row<navigation->count();++row) {
            auto *item = navigation->item(row);
            const bool visible = (item->text()+" "+item->data(Qt::UserRole+1).toString()).contains(query.trimmed(),Qt::CaseInsensitive);
            item->setHidden(!visible);
            if (visible) ++matches;
        }
        noMatches->setVisible(matches==0);
    });
    connect(selector,&QComboBox::currentIndexChanged,this,[this](int index) {
        if (index>=0) activateSession(selector->itemData(index).toInt());
    });
    auto *find = new QShortcut(QKeySequence(QStringLiteral("Ctrl+K")),this);
    connect(find,&QShortcut::activated,search,[search] { search->setFocus(); search->selectAll(); });
    auto *next = new QShortcut(QKeySequence(QStringLiteral("Ctrl+Tab")),this);
    connect(next,&QShortcut::activated,this,[this] {
        QList<int> attached;
        for (auto it=sessions.cbegin();it!=sessions.cend();++it) if (!it->floating) attached.append(it.key());
        if (attached.isEmpty()) return;
        const auto current = attached.indexOf(recentSessions.value(currentTool));
        activateSession(attached.at((current+1)%attached.size()));
    });
    if (!definitions.isEmpty()) navigation->setCurrentRow(0);
    else {
        auto *empty = new QLabel(QStringLiteral("工具目录为空\n添加兼容工具包后重新打开。"));
        empty->setAlignment(Qt::AlignCenter);
        empty->setObjectName("secondary");
        workspace->addWidget(empty);
        workspace->setCurrentWidget(empty);
    }
}
Workbench::~Workbench() {
    for (auto &session : sessions) if (session.floating) delete session.floating;
}
int Workbench::sessionCount() const { return sessions.size(); }
QWidget *Workbench::sessionView(int id) const { return sessions.value(id).view; }
QPlainTextEdit *Workbench::editor(int id) const { return sessions.value(id).editor; }
bool Workbench::isDetached(int id) const { return sessions.value(id).floating!=nullptr; }
void Workbench::openTool(const QString &toolId) {
    currentTool = toolId;
    if (recentSessions.contains(toolId)) activateSession(recentSessions.value(toolId)); else addSession();
}
void Workbench::updateSelector(int activeId) {
    const QSignalBlocker blocker(selector);
    selector->clear();
    int ordinal=0;
    for (auto it=sessions.cbegin();it!=sessions.cend();++it) if (it->toolId==currentTool) {
        ++ordinal;
        selector->addItem(QStringLiteral("会话 %1%2").arg(ordinal).arg(it->floating ? QStringLiteral(" · 独立窗口") : QString()),it.key());
    }
    selector->setCurrentIndex(selector->findData(activeId));
    selector->setVisible(selector->count()>1);
}
void Workbench::activateSession(int id) {
    auto it=sessions.find(id);
    if (it==sessions.end()) return;
    currentTool=it->toolId;
    recentSessions[currentTool]=id;
    const QSignalBlocker navigationBlocker(navigation);
    for (int row=0;row<navigation->count();++row) if (navigation->item(row)->data(Qt::UserRole).toString()==currentTool) {
        navigation->setCurrentRow(row);
        break;
    }
    updateSelector(id);
    if (it->floating) {
        workspace->setCurrentWidget(floatingNotice);
        if (it->floating->isMinimized()) it->floating->showNormal();
        else it->floating->show();
        it->floating->raise();
        it->floating->activateWindow();
    } else workspace->setCurrentWidget(it->view);
}
int Workbench::addSession() {
    const auto tool=std::find_if(definitions.cbegin(),definitions.cend(),[this](const auto &entry) { return entry.id==currentTool; });
    if (tool==definitions.cend()) return 0;
    const int id=nextId++;
    const auto spec=loadViewSpec(tool->packageRoot,!tool->viewFile.isEmpty());
    auto *view=new QWidget;
    auto *layout=new QVBoxLayout(view);
    layout->setContentsMargins(0,0,0,0);
    layout->setSpacing(20);
    auto *header=new QHBoxLayout;
    auto *titleBlock=new QVBoxLayout;
    auto *heading=new QLabel(tool->name);
    heading->setObjectName("heading");
    heading->setWordWrap(true);
    titleBlock->addWidget(heading);
    auto *description=new QLabel(tool->description);
    description->setObjectName("description");
    description->setWordWrap(true);
    titleBlock->addWidget(description);
    header->addLayout(titleBlock,1);
    auto *create=new QPushButton(uiIcon(UiIcon::Add,Theme::muted),QStringLiteral("新建会话"));
    header->addWidget(create,0,Qt::AlignTop);
    auto *detach=new QPushButton(uiIcon(UiIcon::External,Theme::muted),QStringLiteral("独立窗口"));
    detach->setObjectName("locationAction");
    header->addWidget(detach,0,Qt::AlignTop);
    layout->addLayout(header);
    const auto rendered=renderToolView(spec);
    auto *scroll=new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setWidget(rendered.widget);
    layout->addWidget(scroll,1);
    auto *status=new QLabel(QStringLiteral("工具包 v%1 · 界面预览，处理后端待接入").arg(tool->version));
    status->setObjectName("secondary");
    status->setWordWrap(true);
    layout->addWidget(status);
    sessions.insert(id,{tool->id,view,rendered.editor,nullptr});
    workspace->addWidget(view);
    connect(create,&QPushButton::clicked,this,[this,toolId=tool->id] { currentTool=toolId; addSession(); });
    connect(detach,&QPushButton::clicked,this,[this,id] {
        if (isDetached(id)) attachSession(id); else detachSession(id);
    });
    activateSession(id);
    return id;
}
void Workbench::detachSession(int id) {
    auto it=sessions.find(id);
    if (it==sessions.end() || it->floating) return;
    auto *window=new ToolWindow;
    window->setWindowTitle(QStringLiteral("ModynToolbox · %1 · 会话 %2").arg(it->view->findChild<QLabel *>("heading")->text()).arg(id));
    window->resize(820,660);
    window->setMinimumSize(620,460);
    window->returnToHost=[this,id] { attachSession(id); };
    workspace->removeWidget(it->view);
    auto *frame=new QWidget;
    auto *layout=new QVBoxLayout(frame);
    layout->setContentsMargins(20,20,20,20);
    layout->addWidget(it->view);
    window->setContentWidget(frame);
    it->floating=window;
    it->view->findChild<QPushButton *>("locationAction")->setText(QStringLiteral("收回主窗口"));
    it->view->show();
    window->show();
    activateSession(id);
}
void Workbench::attachSession(int id) {
    auto it=sessions.find(id);
    if (it==sessions.end() || !it->floating) return;
    auto *window=it->floating;
    it->view->setParent(nullptr);
    it->view->findChild<QPushButton *>("locationAction")->setText(QStringLiteral("独立窗口"));
    it->floating=nullptr;
    workspace->addWidget(it->view);
    window->hide();
    window->deleteLater();
    if (isMinimized()) showNormal();
    raise();
    activateWindow();
    activateSession(id);
}
