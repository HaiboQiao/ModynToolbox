#include "workbench.h"
#include "components/icons.h"
#include "components/tool_panel.h"
#include "catalog/package_manager.h"
#include "jobs/scheduler.h"
#include "storage/state_repository.h"
#include "storage/data_transfer.h"
#include <QJsonArray>
#include <QMenu>
#include <QDialog>
#include <QMessageBox>
#include <QFileDialog>
#include <QInputDialog>
#include <QFormLayout>
#include <QCheckBox>
#include <QSpinBox>
#include <QDialogButtonBox>
#include <QDesktopServices>
#include "core/files.h"
#include <QProgressDialog>
#include <QFutureWatcher>
#include <QtConcurrent>
#include <QScreen>
#include <QTimer>
#include <QSplitter>
#include <QUuid>
#include "theme/theme.h"
#include "view_spec.h"
#include <QCloseEvent>
#include <QApplication>
#include <QDebug>
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
        auto *close = findChild<QToolButton *>("windowClose");
        close->setAccessibleName(QStringLiteral("关闭并收回主窗口"));
        close->setToolTip(QStringLiteral("关闭并收回主窗口"));
    }
    std::function<void()> returnToHost;

  protected:
    void closeEvent(QCloseEvent *event) override {
        event->ignore();
        if (returnToHost)
            returnToHost();
    }
};
UiIcon navigationIcon(const QString &icon) {
    if (icon == "image")
        return UiIcon::Image;
    if (icon == "code")
        return UiIcon::Code;
    return UiIcon::Files;
}
} // namespace
Workbench::Workbench(QVector<ToolDefinition> tools, QWidget *parent)
    : Workbench(std::move(tools), WorkbenchConfig{}, parent) {
}
Workbench::Workbench(QVector<ToolDefinition> tools, WorkbenchConfig options, QWidget *parent)
    : ChromeWindow(parent), definitions(std::move(tools)), config(std::move(options)) {
    Theme::apply();
    resize(1200, 780);
    setMinimumSize(760, 480);
    if (QGuiApplication::platformName() == "windows")
        resize(QSize(1200, 780).boundedTo(
            QGuiApplication::primaryScreen()->availableGeometry().size() - QSize(32, 32)));
    auto *root = new QWidget;
    root->setObjectName("workbenchRoot");
    auto *layout = new QHBoxLayout(root);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    auto *sidebar = new QWidget;
    sidebar->setObjectName("sidebar");
    sidebar->setMinimumWidth(170);
    sidebar->setMaximumWidth(280);
    auto *sideLayout = new QVBoxLayout(sidebar);
    sideLayout->setContentsMargins(14, 20, 14, 16);
    sideLayout->setSpacing(Theme::gap);
    auto *search = new QLineEdit;
    search->setObjectName("toolSearch");
    search->setAccessibleName(QStringLiteral("搜索工具"));
    search->setPlaceholderText(QStringLiteral("搜索工具"));
    search->addAction(uiIcon(UiIcon::Search, Theme::muted), QLineEdit::LeadingPosition);
    search->setClearButtonEnabled(true);
    sideLayout->addWidget(search);
    categories = new UiComboBox;
    categories->setObjectName("toolCategory");
    categories->setAccessibleName(QStringLiteral("筛选工具分类"));
    categories->hide();
    sideLayout->addWidget(categories);
    auto *section = new QLabel(QStringLiteral("工具"));
    section->setObjectName("secondary");
    sideLayout->addWidget(section);
    navigation = new QListWidget;
    navigation->setObjectName("toolNavigation");
    navigation->setAccessibleName(QStringLiteral("工具列表"));
    navigation->setIconSize(QSize(18, 18));
    navigation->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    sideLayout->addWidget(navigation, 1);
    auto *noMatches = new QLabel(QStringLiteral("没有匹配的工具"));
    noMatches->setObjectName("noMatchingTools");
    noMatches->setWordWrap(true);
    noMatches->hide();
    sideLayout->addWidget(noMatches);
    auto *version = new QLabel(QStringLiteral("%1 个本地工具").arg(definitions.size()));
    version->setObjectName("catalogStatus");
    sideLayout->addWidget(version);
    activity = new QPushButton;
    activity->setObjectName("activityButton");
    activity->hide();
    sideLayout->addWidget(activity);
    connect(activity, &QPushButton::clicked, this, &Workbench::showHistory);
    auto *manage = new QPushButton(QStringLiteral("管理"));
    manage->setObjectName("managementButton");
    auto *management = new QMenu(manage);
    management->addAction(QStringLiteral("工具包"), this, &Workbench::showManager);
    management->addAction(QStringLiteral("退出并排"), this, [this] { pairSession(0); });
    management->addAction(QStringLiteral("全部会话"), this, &Workbench::showSessions);
    management->addAction(QStringLiteral("运行记录"), this, &Workbench::showHistory);
    management->addAction(QStringLiteral("设置"), this, &Workbench::showPreferences);
    management->addAction(QStringLiteral("备份工作区"), this, [this] { transferData(false); });
    management->addAction(QStringLiteral("恢复工作区"), this, [this] { transferData(true); });
    manage->setMenu(management);
    sideLayout->addWidget(manage);
    auto *content = new QWidget;
    auto *contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(Theme::pageInset, Theme::pageInset, Theme::pageInset,
                                      Theme::pageInset);
    contentLayout->setSpacing(Theme::gap);
    selector = new UiComboBox;
    selector->setObjectName("sessionSelector");
    selector->setAccessibleName(QStringLiteral("当前工具会话"));
    selector->hide();
    auto *workspaceHeading = new QLabel(QStringLiteral("并排工作区"));
    workspaceHeading->setObjectName("pairedHeading");
    workspaceHeading->hide();
    auto *workspaceToolbar = new QHBoxLayout;
    workspaceToolbar->addWidget(workspaceHeading);
    workspaceToolbar->addStretch();
    workspaceToolbar->addWidget(selector);
    contentLayout->addLayout(workspaceToolbar);
    workspace = new QStackedWidget;
    workspace->setObjectName("primaryWorkspace");
    splitter = new QSplitter;
    splitter->setObjectName("workspaceSplitter");
    secondary = new QStackedWidget;
    splitter->addWidget(workspace);
    splitter->addWidget(secondary);
    secondary->hide();
    contentLayout->addWidget(splitter, 1);
    floatingNotice = new QWidget;
    floatingNotice->setObjectName("floatingNotice");
    auto *noticeLayout = new QVBoxLayout(floatingNotice);
    noticeLayout->addStretch();
    auto *notice = new QLabel(QStringLiteral("此会话正在独立窗口中使用"));
    notice->setAlignment(Qt::AlignCenter);
    notice->setObjectName("workspaceNoticeText");
    noticeLayout->addWidget(notice);
    auto *returnButton =
        new QPushButton(uiIcon(UiIcon::Return, Theme::accent), QStringLiteral("收回主窗口"));
    returnButton->setObjectName("returnFloatingButton");
    noticeLayout->addWidget(returnButton, 0, Qt::AlignHCenter);
    noticeLayout->addStretch();
    workspace->addWidget(floatingNotice);
    connect(returnButton, &QPushButton::clicked, this,
            [this] { attachSession(recentSessions.value(currentTool)); });
    navigationSplit = new QSplitter;
    navigationSplit->setObjectName("navigationSplitter");
    navigationSplit->setChildrenCollapsible(false);
    navigationSplit->setHandleWidth(3);
    navigationSplit->addWidget(sidebar);
    navigationSplit->addWidget(content);
    navigationSplit->setStretchFactor(0, 0);
    navigationSplit->setStretchFactor(1, 1);
    layout->addWidget(navigationSplit);
    connect(navigationSplit, &QSplitter::splitterMoved, this, [this] { updatePairVisibility(); });
    setContentWidget(root);
    connect(navigation, &QListWidget::currentRowChanged, this, [this](int row) {
        if (auto *item = navigation->item(row))
            openTool(item->data(Qt::UserRole).toString());
    });
    connect(search, &QLineEdit::textChanged, this, [this] { filterNavigation(); });
    connect(categories, &QComboBox::currentIndexChanged, this, [this] { filterNavigation(); });
    rebuildNavigation();
    connect(selector, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (index >= 0)
            activateSession(selector->itemData(index).toInt());
    });
    auto *find = new QShortcut(QKeySequence(QStringLiteral("Ctrl+K")), this);
    connect(find, &QShortcut::activated, search, [search] {
        search->setFocus();
        search->selectAll();
    });
    auto *next = new QShortcut(QKeySequence(QStringLiteral("Ctrl+Tab")), this);
    connect(next, &QShortcut::activated, this, [this] {
        QList<int> attached;
        for (auto it = sessions.cbegin(); it != sessions.cend(); ++it)
            if (!it->floating)
                attached.append(it.key());
        if (attached.isEmpty())
            return;
        const auto current = attached.indexOf(recentSessions.value(currentTool));
        activateSession(attached.at((current + 1) % attached.size()));
    });
    if (!definitions.isEmpty())
        navigation->setCurrentRow(0);
    else {
        auto *empty = new QLabel(QStringLiteral("工具目录为空\n通过管理菜单安装兼容工具包。"));
        empty->setAlignment(Qt::AlignCenter);
        empty->setObjectName("secondary");
        workspace->addWidget(empty);
        workspace->setCurrentWidget(empty);
    }
    initializeServices();
    QTimer::singleShot(0, this, [this] {
        navigationSplit->setSizes({navigationWidth, width() - navigationWidth});
        updatePairVisibility();
    });
}
Workbench::~Workbench() {
    closing = true;
    saveState();
    for (auto &session : sessions)
        if (session.floating)
            delete session.floating;
}
int Workbench::sessionCount() const {
    return sessions.size();
}
QWidget *Workbench::sessionView(int id) const {
    const_cast<Workbench *>(this)->ensureView(id);
    return sessions.value(id).view;
}
QPlainTextEdit *Workbench::editor(int id) const {
    return panel(id) ? panel(id)->editor() : nullptr;
}
ToolPanel *Workbench::panel(int id) const {
    const_cast<Workbench *>(this)->ensureView(id);
    return sessions.value(id).panel;
}
bool Workbench::isDetached(int id) const {
    return sessions.value(id).floating != nullptr;
}
int Workbench::open(const QString &toolId, bool fresh) {
    if (fresh) {
        currentTool = toolId;
        return addSession();
    }
    openTool(toolId);
    return recentSessions.value(toolId);
}
void Workbench::openTool(const QString &toolId) {
    currentTool = toolId;
    if (recentSessions.contains(toolId))
        activateSession(recentSessions.value(toolId));
    else
        addSession();
}
void Workbench::updateSelector(int selected) {
    const QSignalBlocker blocker(selector);
    selector->clear();
    for (auto it = sessions.cbegin(); it != sessions.cend(); ++it)
        if (it->toolId == currentTool)
            selector->addItem(
                it->title + (it->floating ? QStringLiteral(" · 独立窗口") : QString()), it.key());
    selector->setCurrentIndex(selector->findData(selected));
    selector->setVisible(selector->count() > 1);
}
void Workbench::activateSession(int id) {
    auto it = sessions.find(id);
    if (it == sessions.end())
        return;
    activeId = id;
    currentTool = it->toolId;
    recentSessions[currentTool] = id;
    const QSignalBlocker blocker(navigation);
    for (int row = 0; row < navigation->count(); ++row)
        if (navigation->item(row)->data(Qt::UserRole).toString() == currentTool)
            navigation->setCurrentRow(row);
    ensureView(id);
    updateSelector(id);
    if (pairedId == id)
        pairedId = 0;
    if (it->floating) {
        floatingNotice->findChild<QLabel *>("workspaceNoticeText")
            ->setText(QStringLiteral("此会话正在独立窗口中使用"));
        floatingNotice->findChild<QPushButton *>("returnFloatingButton")->show();
        workspace->setCurrentWidget(floatingNotice);
        if (it->floating->isMinimized())
            it->floating->showNormal();
        else
            it->floating->show();
        if (QGuiApplication::platformName() != "offscreen") {
            it->floating->raise();
            it->floating->activateWindow();
        }
    } else {
        if (secondary->indexOf(it->view) >= 0)
            secondary->removeWidget(it->view);
        if (workspace->indexOf(it->view) < 0)
            workspace->addWidget(it->view);
        workspace->setCurrentWidget(it->view);
    }
    updatePairVisibility();
    for (auto entry = sessions.begin(); entry != sessions.end(); ++entry)
        if (entry->view) {
            auto *heading = entry->view->findChild<QLabel *>("heading");
            if (heading)
                heading->setText(
                    entry->customTitle ||
                            (pairedId > 0 && (entry.key() == activeId || entry.key() == pairedId))
                        ? QStringLiteral("%1 · %2").arg(entry->tool.name).arg(entry->title)
                        : entry->tool.name);
        }
    if (pairedId > 0 && pairedId != id && !isDetached(pairedId)) {
        ensureView(pairedId);
        auto *other = sessions[pairedId].view;
        workspace->removeWidget(other);
        if (secondary->indexOf(other) < 0)
            secondary->addWidget(other);
        secondary->setCurrentWidget(other);
    }
    parkUnused();
}
int Workbench::addSession() {
    if (sessions.size() >= 100)
        return 0;
    const auto tool = std::find_if(definitions.cbegin(), definitions.cend(),
                                   [this](const auto &entry) { return entry.id == currentTool; });
    if (tool == definitions.cend())
        return 0;
    const int id = nextId++;
    Session session;
    session.toolId = tool->id;
    session.tool = *tool;
    session.uuid = QUuid::createUuid().toString(QUuid::WithoutBraces);
    int ordinal = 1;
    QSet<QString> titles;
    for (const auto &existing : sessions)
        if (existing.toolId == tool->id)
            titles.insert(existing.title);
    while (titles.contains(QStringLiteral("会话 %1").arg(ordinal)))
        ++ordinal;
    session.title = QStringLiteral("会话 %1").arg(ordinal);
    session.draft = preferences.value("default_draft").toBool();
    sessions.insert(id, session);
    ensureView(id);
    activateSession(id);
    return id;
}
void Workbench::ensureView(int id) {
    auto it = sessions.find(id);
    if (it == sessions.end() || it->view)
        return;
    auto *view = new QWidget;
    auto *layout = new QVBoxLayout(view);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);
    auto *header = new QHBoxLayout;
    auto *titles = new QVBoxLayout;
    auto *heading =
        new QLabel(it->customTitle || (pairedId > 0 && (id == activeId || id == pairedId))
                       ? QStringLiteral("%1 · %2").arg(it->tool.name).arg(it->title)
                       : it->tool.name);
    heading->setObjectName("heading");
    heading->setTextFormat(Qt::PlainText);
    heading->setWordWrap(true);
    heading->setMinimumWidth(0);
    heading->setMaximumHeight(68);
    heading->setToolTip(it->tool.name);
    titles->addWidget(heading);
    auto *description = new QLabel(it->tool.description);
    description->setObjectName("description");
    description->setTextFormat(Qt::PlainText);
    description->setWordWrap(true);
    description->setMaximumHeight(48);
    description->setToolTip(it->tool.description);
    titles->addWidget(description);
    header->addLayout(titles, 1);
    auto *menuButton = new QPushButton(QStringLiteral("会话"));
    menuButton->setObjectName("sessionMenu");
    auto *menu = new QMenu(menuButton);
    menu->addAction(QStringLiteral("新建会话"), this, [this, id] {
        currentTool = sessions[id].toolId;
        addSession();
    });
    menu->addAction(QStringLiteral("重命名会话"), this, [this, id] { renameSession(id); });
    auto *pairMenu = menu->addMenu(QStringLiteral("并排显示"));
    connect(pairMenu, &QMenu::aboutToShow, this, [this, id, pairMenu] {
        pairMenu->clear();
        for (auto other = sessions.cbegin(); other != sessions.cend(); ++other)
            if (other.key() != id) {
                const auto target = other.key();
                pairMenu->addAction(
                    QStringLiteral("%1 · %2").arg(other->tool.name).arg(other->title), this,
                    [this, id, target] {
                        activateSession(id);
                        pairSession(target);
                    });
            }
        if (pairMenu->isEmpty()) {
            auto *empty = pairMenu->addAction(QStringLiteral("请先打开另一会话"));
            empty->setEnabled(false);
        }
    });
    auto *draft = menu->addAction(QStringLiteral("保存本会话输入草稿"));
    draft->setCheckable(true);
    draft->setChecked(it->draft);
    connect(draft, &QAction::toggled, this, [this, id](bool enabled) { setDraft(id, enabled); });
    menu->addAction(QStringLiteral("关闭会话"), this, [this, id] { closeSession(id); });
    menuButton->setMenu(menu);
    header->addWidget(menuButton, 0, Qt::AlignTop);
    auto *detach =
        new QPushButton(uiIcon(UiIcon::External, Theme::muted), QStringLiteral("独立窗口"));
    detach->setObjectName("locationAction");
    header->addWidget(detach, 0, Qt::AlignTop);
    layout->addLayout(header);
    auto spec = loadViewSpec(it->tool.packageRoot, !it->tool.viewFile.isEmpty());
    if (it->missing)
        spec.error =
            QStringLiteral(
                "该会话固定的工具包 %1 已移除。重新安装相同版本并刷新后可继续。输入草稿已保留。")
                .arg(it->tool.version);
    auto *body = new ToolPanel(it->tool, spec);
    it->view = view;
    it->panel = body;
    layout->addWidget(body, 1);
    workspace->addWidget(view);
    if (!it->missing && !it->parked.isEmpty())
        it->preserveSnapshot = !body->restore(it->parked);
    connect(detach, &QPushButton::clicked, this, [this, id] {
        if (isDetached(id))
            attachSession(id);
        else
            detachSession(id);
    });
    connect(body, &ToolPanel::changed, this, [this, id] {
        if (!sessions.contains(id))
            return;
        sessions[id].preserveSnapshot = false;
        ++sessions[id].revision;
    });
    connect(body, &ToolPanel::exportRequested, this,
            [this, id](const QString &path, const QJsonObject &document, bool plainText) {
                if (!scheduler || !sessions.contains(id))
                    return;
                auto &session = sessions[id];
                const auto job = scheduler->submitExport(
                    session.tool, session.uuid, session.revision, path, document, plainText);
                if (job.isEmpty()) {
                    const auto record = scheduler->record(session.job);
                    if (session.panel)
                        session.panel->setJobState(
                            record.state,
                            QStringLiteral("导出未提交，请等待当前作业结束或队列空闲"),
                            record.completed, record.total, record.cancellable);
                    return;
                }
                session.job = job;
                const auto record = scheduler->record(job);
                if (session.panel)
                    session.panel->setJobState(record.state, QStringLiteral("准备导出结果"), 0, 1,
                                               true);
            });
    connect(body, &ToolPanel::actionRequested, this,
            [this, id](const QString &action, const QJsonObject &input) {
                runAction(id, action, input);
            });
    connect(body, &ToolPanel::ready, this, &Workbench::parkUnused, Qt::QueuedConnection);
    connect(body, &ToolPanel::cancelRequested, this, [this, id] {
        if (scheduler)
            scheduler->cancel(sessions[id].job);
    });
    if (scheduler && !it->job.isEmpty()) {
        const auto job = scheduler->record(it->job);
        body->setJobState(job.state, job.message, job.completed, job.total, job.cancellable);
    }
}
void Workbench::parkUnused() {
    if (config.dataRoot.isEmpty())
        return;
    for (auto it = sessions.begin(); it != sessions.end(); ++it) {
        if (it.key() == activeId || it.key() == pairedId || it->floating || !it->panel ||
            !it->panel->canPark())
            continue;
        if (!it->missing && !it->preserveSnapshot)
            it->parked = it->panel->snapshot();
        workspace->removeWidget(it->view);
        secondary->removeWidget(it->view);
        delete it->view;
        it->view = nullptr;
        it->panel = nullptr;
    }
}
void Workbench::detachSession(int id) {
    auto it = sessions.find(id);
    if (it == sessions.end() || it->floating)
        return;
    ensureView(id);
    if (pairedId == id) {
        pairedId = 0;
        secondary->hide();
    }
    auto *window = new ToolWindow;
    window->setWindowTitle(
        QStringLiteral("ModynToolbox · %1 · %2").arg(it->tool.name).arg(it->title));
    window->resize(840, 700);
    window->setMinimumSize(620, 460);
    if (QGuiApplication::platformName() == "windows")
        window->resize(
            QSize(840, 700).boundedTo(screen()->availableGeometry().size() - QSize(32, 32)));
    window->returnToHost = [this, id] { attachSession(id); };
    workspace->removeWidget(it->view);
    secondary->removeWidget(it->view);
    auto *frame = new QWidget;
    auto *layout = new QVBoxLayout(frame);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->addWidget(it->view);
    window->setContentWidget(frame);
    it->floating = window;
    if (!it->geometry.isEmpty())
        window->restoreGeometry(it->geometry);
    it->view->findChild<QPushButton *>("locationAction")->setText(QStringLiteral("收回主窗口"));
    it->view->show();
    window->show();
    activateSession(id);
}
void Workbench::attachSession(int id) {
    auto it = sessions.find(id);
    if (it == sessions.end() || !it->floating)
        return;
    auto *window = it->floating;
    it->geometry = window->saveGeometry();
    it->view->setParent(nullptr);
    it->view->findChild<QPushButton *>("locationAction")->setText(QStringLiteral("独立窗口"));
    it->floating = nullptr;
    workspace->addWidget(it->view);
    window->hide();
    window->deleteLater();
    if (isMinimized())
        showNormal();
    if (QGuiApplication::platformName() != "offscreen") {
        raise();
        activateWindow();
    }
    activateSession(id);
}
void Workbench::pairSession(int id) {
    if (id == 0) {
        pairedId = 0;
        secondary->hide();
        if (activeId)
            activateSession(activeId);
        return;
    }
    if (!sessions.contains(id))
        return;
    const int primary = activeId;
    if (isDetached(id))
        attachSession(id);
    if (primary && primary != id && sessions.contains(primary))
        activateSession(primary);
    if (id == activeId) {
        for (int other : sessions.keys())
            if (other != id && !isDetached(other)) {
                activateSession(other);
                break;
            }
    }
    if (id == activeId)
        return;
    pairedId = id;
    activateSession(activeId);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({splitter->width() / 2, splitter->width() / 2});
}
void Workbench::closeSession(int id, bool confirmed) {
    auto it = sessions.find(id);
    if (it == sessions.end())
        return;
    if (scheduler && !it->job.isEmpty() &&
        QStringList{"preparing", "queued", "running"}.contains(scheduler->record(it->job).state)) {
        const auto job = scheduler->record(it->job);
        QMessageBox prompt(this);
        prompt.setWindowTitle(QStringLiteral("会话仍在运行"));
        prompt.setText(
            QStringLiteral("可以等待作业结束后关闭此会话。关闭后输入和该会话草稿将一并移除。"));
        auto *wait = prompt.addButton(QStringLiteral("完成后关闭"), QMessageBox::AcceptRole);
        QAbstractButton *cancel = nullptr;
        if (job.cancellable)
            cancel =
                prompt.addButton(QStringLiteral("安全取消后关闭"), QMessageBox::DestructiveRole);
        prompt.addButton(QStringLiteral("继续使用"), QMessageBox::RejectRole);
        prompt.exec();
        if (prompt.clickedButton() == wait || (cancel && prompt.clickedButton() == cancel)) {
            pendingSessionClose.insert(id);
            if (cancel && prompt.clickedButton() == cancel)
                scheduler->cancel(it->job);
            if (!QStringList{"preparing", "queued", "running"}.contains(
                    scheduler->record(it->job).state))
                QTimer::singleShot(0, this, [this, id] {
                    pendingSessionClose.remove(id);
                    closeSession(id, true);
                });
        }
        return;
    }
    if (!restoring && !confirmed &&
        ((it->panel && it->panel->hasVolatileInput()) ||
         (!it->parked.value("text").toString().isEmpty()) ||
         (!it->parked.value("files").toArray().isEmpty())) &&
        QMessageBox::question(this, QStringLiteral("关闭会话"),
                              QStringLiteral("关闭会移除此会话和它的已存草稿，确认继续？")) !=
            QMessageBox::Yes)
        return;
    pendingSessionClose.remove(id);
    const auto toolId = it->toolId;
    if (it->view) {
        workspace->removeWidget(it->view);
        secondary->removeWidget(it->view);
        delete it->view;
    }
    if (it->floating)
        delete it->floating;
    sessions.erase(it);
    if (pairedId == id) {
        pairedId = 0;
        secondary->hide();
    }
    recentSessions.remove(toolId);
    for (auto other = sessions.cbegin(); other != sessions.cend(); ++other)
        if (other->toolId == toolId)
            recentSessions[toolId] = other.key();
    if (activeId == id) {
        activeId = 0;
        if (!sessions.isEmpty())
            activateSession(sessions.firstKey());
        else {
            floatingNotice->findChild<QLabel *>("workspaceNoticeText")
                ->setText(QStringLiteral("尚未打开会话，请从左侧选择工具。"));
            floatingNotice->findChild<QPushButton *>("returnFloatingButton")->hide();
            workspace->setCurrentWidget(floatingNotice);
        }
    }
    updateSelector(activeId);
    if (!restoring)
        saveState();
}
void Workbench::setDraft(int id, bool enabled) {
    if (sessions.contains(id))
        sessions[id].draft = enabled;
}
void Workbench::runAction(int id, const QString &actionId, const QJsonObject &input,
                          bool confirmed) {
    if (!scheduler || !sessions.contains(id) || sessions[id].missing)
        return;
    auto &session = sessions[id];
    if (!session.job.isEmpty() && QStringList{"preparing", "queued", "running"}.contains(
                                      scheduler->record(session.job).state))
        return;
    const auto action = std::find_if(session.tool.actions.cbegin(), session.tool.actions.cend(),
                                     [&](const auto &value) { return value.id == actionId; });
    if (action == session.tool.actions.cend())
        return;
    auto confirmation = action->confirmation;
    const int affected = input.value("context").toObject().value("affected_count").toInt(-1);
    if (affected >= 0)
        confirmation += QStringLiteral("\n受影响文件数 %1").arg(affected);
    if (!action->confirmation.isEmpty() && !confirmed &&
        QMessageBox::question(this, action->label, confirmation) != QMessageBox::Yes)
        return;
    session.job = scheduler->submit(session.tool, session.uuid, session.revision, actionId, input);
    if (session.job.isEmpty()) {
        if (session.panel)
            session.panel->setJobState("failed", QStringLiteral("作业未提交，队列或输入不符合限制"),
                                       0, 0, false);
    } else {
        const auto record = scheduler->record(session.job);
        if (session.panel)
            session.panel->setJobState(record.state, record.message, 0, 0, record.cancellable);
    }
}
void Workbench::initializeServices() {
    if (config.dataRoot.isEmpty())
        return;
    repository = std::make_unique<StateRepository>(config.dataRoot);
    if (!repository->open())
        return;
    scheduler = new Scheduler(config.dataRoot, config.bridge, this);
    connect(scheduler, &Scheduler::updated, this, [this](const QString &id) {
        const auto record = scheduler->record(id);
        if (record.state == "failed")
            ++unreadErrors;
        updateActivity();
        const auto now = QDateTime::currentMSecsSinceEpoch();
        if (!persistedAt.contains(id) || now - persistedAt.value(id) >= 1000 ||
            !QStringList{"preparing", "queued", "running"}.contains(record.state)) {
            QString title;
            for (const auto &session : sessions)
                if (session.uuid == record.request.sessionId)
                    title = session.title;
            repository->job(record, title);
            persistedAt[id] = now;
            if (persistedAt.size() > 500)
                persistedAt.remove(persistedAt.firstKey());
        }
        for (auto &session : sessions)
            if (session.uuid == record.request.sessionId && session.panel)
                session.panel->setJobState(record.state, record.message, record.completed,
                                           record.total, record.cancellable);
        if (waitingExit && !scheduler->busy())
            QTimer::singleShot(0, this, &QWidget::close);
    });
    connect(scheduler, &Scheduler::finished, this,
            [this](const QString &sessionId, const QString &job, int revision,
                   const QJsonObject &result, const QString &outcome) {
                for (auto &session : sessions)
                    if (session.uuid == sessionId && session.revision == revision &&
                        session.job == job) {
                        const bool exportOnly =
                            scheduler->record(job).request.kind == JobKind::FileExport;
                        if (!exportOnly) {
                            session.parked["result"] = result;
                            session.parked["context"] = result.value("context");
                        }
                        if (session.panel) {
                            if (!exportOnly)
                                session.panel->setResult(result);
                            session.panel->setJobState(outcome, result.value("message").toString(),
                                                       0, 0, false);
                        }
                    }
                saveState();
                const auto pending = pendingSessionClose.values();
                for (int id : pending)
                    if (sessions.contains(id) && sessions[id].uuid == sessionId) {
                        pendingSessionClose.remove(id);
                        QTimer::singleShot(0, this, [this, id] { closeSession(id, true); });
                    }
            });
    const auto state = repository->state();
    historyReviewedAt = state.value("history_reviewed_at").toInteger();
    for (const auto &value : repository->history()) {
        const auto record = value.toObject();
        if (QStringList{"failed", "interrupted"}.contains(record.value("state").toString()) &&
            record.value("updated").toInteger(record.value("created").toInteger()) >
                historyReviewedAt)
            ++unreadErrors;
    }
    updateActivity();
    preferences = state.value("preferences").toObject();
    navigationWidth = qBound(170, state.value("navigation_width").toInt(190), 280);
    applyPreferences();
    if (!state.isEmpty()) {
        restoring = true;
        int restoredActive = 0, restoredPair = 0;
        for (int id : sessions.keys())
            closeSession(id);
        const auto versions = PackageManager(config.packagesRoot).versions();
        for (const auto &value : state.value("sessions").toArray()) {
            if (sessions.size() >= 100)
                break;
            const auto stored = value.toObject();
            Session session;
            session.toolId = stored.value("tool").toString();
            session.uuid = stored.value("uuid").toString();
            if (session.uuid.isEmpty())
                session.uuid = QUuid::createUuid().toString(QUuid::WithoutBraces);
            session.draft = stored.value("draft").toBool();
            session.title =
                stored.value("title").toString(QStringLiteral("会话 %1").arg(nextId)).left(80);
            session.customTitle = stored.value("custom_title").toBool();
            session.parked = stored.value("snapshot").toObject();
            session.parked.remove("result");
            session.parked.remove("context");
            session.geometry =
                QByteArray::fromBase64(stored.value("geometry").toString().toLatin1());
            const auto version = stored.value("version").toString();
            const auto tool =
                std::find_if(versions.cbegin(), versions.cend(), [&](const auto &entry) {
                    return entry.id == session.toolId && entry.version == version;
                });
            if (tool != versions.cend())
                session.tool = *tool;
            else {
                session.missing = true;
                session.tool.id = session.toolId;
                session.tool.name = stored.value("name").toString(session.toolId);
                session.tool.version = version;
            }
            const int id = nextId++;
            if (session.uuid == state.value("active").toString())
                restoredActive = id;
            if (session.uuid == state.value("paired").toString())
                restoredPair = id;
            sessions[id] = session;
            recentSessions[session.toolId] = id;
            if (stored.value("floating").toBool())
                detachSession(id);
        }
        if (!state.value("geometry").toString().isEmpty())
            restoreGeometry(QByteArray::fromBase64(state.value("geometry").toString().toLatin1()));
        for (QScreen *screen : QGuiApplication::screens())
            if (frameGeometry().intersects(screen->availableGeometry())) {
                restoring = false;
                break;
            }
        if (restoring)
            move(QGuiApplication::primaryScreen()->availableGeometry().topLeft() + QPoint(40, 40));
        restoring = false;
        if (!sessions.isEmpty()) {
            pairedId = restoredPair;
            activateSession(restoredActive ? restoredActive : sessions.firstKey());
        }
    }
    auto *timer = new QTimer(this);
    timer->setInterval(10000);
    connect(timer, &QTimer::timeout, this, [this] { saveState(); });
    timer->start();
}
QJsonObject Workbench::savedState() const {
    QJsonArray records;
    for (auto it = sessions.cbegin(); it != sessions.cend(); ++it) {
        auto snapshot = it->panel && !it->missing && !it->preserveSnapshot
                            ? it->panel->snapshot(it->draft)
                            : it->parked;
        if (!it->draft) {
            const auto params = snapshot.value("params");
            snapshot = {{"params", params}};
        } else {
            snapshot.remove("context");
            snapshot.remove("result");
        }
        records.append(QJsonObject{
            {"tool", it->toolId},
            {"name", it->tool.name},
            {"title", it->title},
            {"custom_title", it->customTitle},
            {"version", it->tool.version},
            {"uuid", it->uuid},
            {"draft", it->draft},
            {"snapshot", snapshot},
            {"floating", it->floating != nullptr},
            {"geometry",
             QString::fromLatin1(
                 (it->floating ? it->floating->saveGeometry() : it->geometry).toBase64())}});
    }
    return {{"history_reviewed_at", historyReviewedAt},
            {"preferences", preferences},
            {"navigation_width", navigationSplit ? navigationSplit->sizes().value(0, 190) : 190},
            {"sessions", records},
            {"active", sessions.value(activeId).uuid},
            {"paired", sessions.value(pairedId).uuid},
            {"geometry", QString::fromLatin1(saveGeometry().toBase64())}};
}
bool Workbench::saveState() {
    return !repository || repository->save(savedState());
}
QString Workbench::storageError() const {
    return repository ? repository->error() : QString();
}
bool Workbench::refreshTools() {
    if (config.packagesRoot.isEmpty())
        return false;
    const auto catalog = readCatalog(config.packagesRoot);
    definitions = catalog.tools;
    if (auto *label = findChild<QLabel *>("catalogStatus"))
        label->setText(QStringLiteral("%1 个本地工具").arg(definitions.size()));
    const auto versions = PackageManager(config.packagesRoot).versions();
    for (auto &session : sessions) {
        const auto tool = std::find_if(versions.cbegin(), versions.cend(), [&](const auto &entry) {
            return entry.id == session.toolId && entry.version == session.tool.version;
        });
        const bool missing = tool == versions.cend();
        if (missing != session.missing) {
            if (session.panel && !session.missing && !session.preserveSnapshot)
                session.parked = session.panel->snapshot();
            if (session.floating) {
                auto *window = session.floating;
                session.floating = nullptr;
                if (session.view)
                    session.view->setParent(nullptr);
                window->deleteLater();
            }
            if (session.view) {
                workspace->removeWidget(session.view);
                secondary->removeWidget(session.view);
                delete session.view;
                session.view = nullptr;
                session.panel = nullptr;
            }
            session.missing = missing;
            if (!missing)
                session.tool = *tool;
        }
    }
    rebuildNavigation();
    if (sessions.contains(activeId))
        activateSession(activeId);
    else if (!definitions.isEmpty())
        openTool(definitions.first().id);
    return catalog.errors.isEmpty();
}
QString Workbench::installPackage(const QString &folder) {
    auto result = PackageManager(config.packagesRoot).install(folder);
    if (result.isEmpty())
        refreshTools();
    return result;
}
QString Workbench::removePackage(const QString &id, const QString &version) {
    const auto versions = PackageManager(config.packagesRoot).versions();
    for (const auto &tool : versions)
        if (tool.id == id && tool.version == version) {
            bool referenced = false;
            for (const auto &session : sessions)
                referenced |= session.toolId == id && session.tool.version == version;
            if (scheduler)
                for (const auto &job : scheduler->records())
                    referenced |= job.request.tool.id == id &&
                                  job.request.tool.version == version &&
                                  QStringList{"preparing", "queued", "running"}.contains(job.state);
            auto error = PackageManager(config.packagesRoot).remove(tool, referenced);
            if (error.isEmpty())
                refreshTools();
            return error;
        }
    return QStringLiteral("未找到该版本");
}
void Workbench::showManager() {
    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("工具管理"));
    dialog.resize(660, 450);
    auto *layout = new QVBoxLayout(&dialog);
    auto *list = new QListWidget;
    layout->addWidget(list, 1);
    auto populate = [&] {
        list->clear();
        for (const auto &tool : PackageManager(config.packagesRoot).versions()) {
            auto *item = new QListWidgetItem(tool.name + " · " + tool.version, list);
            item->setData(Qt::UserRole, tool.id);
            item->setData(Qt::UserRole + 1, tool.version);
        }
    };
    populate();
    auto *note =
        new QLabel(QStringLiteral("移除工具包保留工具数据。关闭引用该版本的会话后可以移除。"));
    note->setWordWrap(true);
    layout->addWidget(note);
    auto *row = new QHBoxLayout;
    auto *add = new QPushButton(QStringLiteral("安装文件夹"));
    auto *remove = new QPushButton(QStringLiteral("移除选中版本"));
    auto *refresh = new QPushButton(QStringLiteral("刷新目录"));
    row->addWidget(add);
    row->addWidget(remove);
    row->addWidget(refresh);
    layout->addLayout(row);
    connect(add, &QPushButton::clicked, &dialog, [&] {
        const auto source = QFileDialog::getExistingDirectory(
            &dialog, QStringLiteral("选择包含 manifest.json 的工具包"));
        if (source.isEmpty())
            return;
        add->setEnabled(false);
        remove->setEnabled(false);
        refresh->setEnabled(false);
        note->setText(QStringLiteral("正在后台校验并安装…"));
        auto *watcher = new QFutureWatcher<QString>(&dialog);
        connect(watcher, &QFutureWatcher<QString>::finished, &dialog, [&, watcher] {
            const auto error = watcher->result();
            watcher->deleteLater();
            refreshTools();
            populate();
            note->setText(error.isEmpty() ? QStringLiteral("安装完成，新会话使用最新版本") : error);
            add->setEnabled(true);
            remove->setEnabled(true);
            refresh->setEnabled(true);
        });
        watcher->setFuture(QtConcurrent::run(
            [root = config.packagesRoot, source] { return PackageManager(root).install(source); }));
    });
    connect(remove, &QPushButton::clicked, &dialog, [&] {
        if (auto *item = list->currentItem()) {
            if (QMessageBox::question(&dialog, QStringLiteral("移除工具"),
                                      QStringLiteral("确认移除该版本程序文件？工具数据保留。")) !=
                QMessageBox::Yes)
                return;
            const auto error = removePackage(item->data(Qt::UserRole).toString(),
                                             item->data(Qt::UserRole + 1).toString());
            note->setText(error.isEmpty() ? QStringLiteral("已移除，数据保留") : error);
            populate();
        }
    });
    connect(refresh, &QPushButton::clicked, &dialog, [&] {
        refreshTools();
        populate();
    });
    dialog.exec();
}
void Workbench::showSessions() {
    QMenu menu;
    for (auto it = sessions.cbegin(); it != sessions.cend(); ++it)
        menu.addAction(QStringLiteral("%1 · 会话 %2%3")
                           .arg(it->tool.name)
                           .arg(it->title)
                           .arg(it->floating ? QStringLiteral(" · 独立窗口") : QString()),
                       this, [this, id = it.key()] { activateSession(id); });
    menu.exec(QCursor::pos());
}
void Workbench::transferData(bool importing) {
    if (!repository)
        return;
    if (scheduler && scheduler->busy()) {
        QMessageBox::information(this, QStringLiteral("工作区正在运行"),
                                 QStringLiteral("请等待所有作业结束后再迁移数据。"));
        return;
    }
    QString path;
    if (importing)
        path = QFileDialog::getExistingDirectory(this, QStringLiteral("选择快照目录"));
    else {
        const auto parent =
            QFileDialog::getExistingDirectory(this, QStringLiteral("选择备份父目录"));
        if (!parent.isEmpty())
            path = parent + "/ModynToolbox-backup-" +
                   QDateTime::currentDateTime().toString("yyyyMMdd-hhmmss");
    }
    if (path.isEmpty())
        return;
    if (importing &&
        QMessageBox::question(
            this, QStringLiteral("恢复工作区"),
            QStringLiteral(
                "当前工作区将先保留到 "
                "host/backups，然后恢复快照。完成后程序退出，下次启动载入恢复状态。确认继续？")) !=
            QMessageBox::Yes)
        return;
    if (!saveState()) {
        QMessageBox::critical(this, QStringLiteral("保存工作区失败"), storageError());
        return;
    }
    repository->close();
    QProgressDialog progress(QStringLiteral("正在校验并处理数据…"), QString(), 0, 0, this);
    progress.setWindowModality(Qt::ApplicationModal);
    progress.setCancelButton(nullptr);
    QFutureWatcher<QString> watcher;
    connect(&watcher, &QFutureWatcher<QString>::finished, &progress, &QDialog::accept);
    watcher.setFuture(QtConcurrent::run([root = config.dataRoot, path, importing] {
        return importing ? DataTransfer::restoreSnapshot(root, path)
                         : DataTransfer::exportSnapshot(root, path);
    }));
    progress.setWindowFlag(Qt::WindowCloseButtonHint, false);
    progress.exec();
    watcher.waitForFinished();
    const auto error = watcher.result();
    if (importing && error.isEmpty()) {
        repository.reset();
        closing = true;
        for (auto &session : sessions)
            if (session.floating)
                session.floating->hide();
        QCoreApplication::quit();
        return;
    }
    if (!repository->open()) {
        QMessageBox::critical(this, QStringLiteral("数据目录"), repository->error());
        return;
    }
    QMessageBox::information(this, QStringLiteral("数据迁移"),
                             error.isEmpty() ? QStringLiteral("备份完成 · ") + path : error);
}
void Workbench::closeEvent(QCloseEvent *event) {
    if (closing) {
        event->accept();
        return;
    }
    if (scheduler && scheduler->busy()) {
        event->ignore();
        QMessageBox prompt(this);
        prompt.setWindowTitle(QStringLiteral("仍有作业运行"));
        prompt.setText(scheduler->critical() ? QStringLiteral("文件事务正在提交，请等待结束。")
                                             : QStringLiteral("等待全部完成，或安全取消后退出。"));
        auto *wait = prompt.addButton(QStringLiteral("完成后退出"), QMessageBox::AcceptRole);
        QAbstractButton *cancel = nullptr;
        if (!scheduler->critical())
            cancel =
                prompt.addButton(QStringLiteral("取消作业后退出"), QMessageBox::DestructiveRole);
        prompt.addButton(QStringLiteral("继续使用"), QMessageBox::RejectRole);
        prompt.setInformativeText(QStringLiteral("退出会保存已启用的草稿，其他临时输入将清除。"));
        prompt.exec();
        if (prompt.clickedButton() == wait || (cancel && prompt.clickedButton() == cancel)) {
            waitingExit = true;
            if (cancel && prompt.clickedButton() == cancel)
                for (const auto &job : scheduler->records())
                    scheduler->cancel(job.request.jobId);
        }
        return;
    }
    bool unsaved = false;
    for (const auto &session : sessions)
        if (!session.draft)
            unsaved |= session.panel ? session.panel->hasVolatileInput()
                                     : !session.parked.value("text").toString().isEmpty() ||
                                           !session.parked.value("files").toArray().isEmpty();
    if (unsaved && !waitingExit &&
        QMessageBox::question(
            this, QStringLiteral("退出 ModynToolbox"),
            QStringLiteral("部分输入未保存为会话草稿。参数与窗口布局会保存，确认退出？")) !=
            QMessageBox::Yes) {
        event->ignore();
        return;
    }
    if (!saveState()) {
        QMessageBox::critical(this, QStringLiteral("保存失败"), storageError());
        event->ignore();
        return;
    }
    closing = true;
    for (auto &session : sessions)
        if (session.floating)
            session.floating->hide();
    event->accept();
}
void Workbench::resizeEvent(QResizeEvent *event) {
    ChromeWindow::resizeEvent(event);
    updatePairVisibility();
}
void Workbench::rebuildNavigation() {
    const QSignalBlocker navigationBlocker(navigation), categoryBlocker(categories);
    const auto selected = categories->currentData().toString();
    navigation->clear();
    categories->clear();
    categories->addItem(QStringLiteral("全部分类"), QString());
    QSet<QString> names;
    for (const auto &tool : definitions) {
        auto *item = new QListWidgetItem(uiIcon(navigationIcon(tool.icon), Theme::accent),
                                         tool.name, navigation);
        item->setData(Qt::UserRole, tool.id);
        item->setData(Qt::UserRole + 1,
                      tool.description + " " + tool.category + " " + tool.keywords.join(' '));
        item->setData(Qt::UserRole + 2, tool.category);
        item->setToolTip(tool.name + "\n" + tool.description);
        names.insert(tool.category);
    }
    auto sorted = names.values();
    std::sort(sorted.begin(), sorted.end());
    for (const auto &name : sorted)
        categories->addItem(name, name);
    categories->setCurrentIndex(qMax(0, categories->findData(selected)));
    categories->setVisible(definitions.size() > 8 && names.size() > 1);
    navigation->setCurrentRow(-1);
    filterNavigation();
}
void Workbench::filterNavigation() {
    const auto query = findChild<QLineEdit *>("toolSearch")->text().trimmed();
    const auto category =
        categories->isVisible() ? categories->currentData().toString() : QString();
    int matches = 0;
    for (int row = 0; row < navigation->count(); ++row) {
        auto *item = navigation->item(row);
        const bool visible =
            (category.isEmpty() || item->data(Qt::UserRole + 2).toString() == category) &&
            (item->text() + " " + item->data(Qt::UserRole + 1).toString())
                .contains(query, Qt::CaseInsensitive);
        item->setHidden(!visible);
        if (visible)
            ++matches;
    }
    if (auto *empty = findChild<QLabel *>("noMatchingTools"))
        empty->setVisible(matches == 0);
}
void Workbench::updatePairVisibility() {
    if (!secondary || !navigationSplit)
        return;
    const int sidebar = navigationSplit->sizes().value(0, Theme::sidebarWidth);
    const bool visible = pairedId > 0 && width() - sidebar - 2 * Theme::pageInset >= 840;
    secondary->setVisible(visible);
    if (auto *heading = findChild<QLabel *>("pairedHeading"))
        heading->setVisible(visible);
}
void Workbench::renameSession(int id) {
    if (!sessions.contains(id))
        return;
    QInputDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("重命名会话"));
    dialog.setOkButtonText(QStringLiteral("保存"));
    dialog.setCancelButtonText(QStringLiteral("取消"));
    dialog.setLabelText(QStringLiteral("会话名称"));
    dialog.setTextValue(sessions[id].title);
    if (auto *input = dialog.findChild<QLineEdit *>())
        input->setMaxLength(80);
    if (dialog.exec() != QDialog::Accepted || dialog.textValue().trimmed().isEmpty())
        return;
    auto &session = sessions[id];
    session.title = dialog.textValue().trimmed().left(80);
    session.customTitle = true;
    if (session.floating)
        session.floating->setWindowTitle(
            QStringLiteral("ModynToolbox · %1 · %2").arg(session.tool.name, session.title));
    activateSession(id);
    saveState();
}
void Workbench::applyPreferences() {
    Theme::setPreferences(preferences.value("font_size").toInt(14),
                          preferences.value("compact").toBool());
    qApp->setProperty("modynRevealResults", preferences.value("reveal_results").toBool(true));
    if (scheduler)
        scheduler->setBatchLimit(preferences.value("batch_limit").toInt(1));
}
void Workbench::showPreferences() {
    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("设置"));
    dialog.resize(480, 350);
    auto *layout = new QVBoxLayout(&dialog);
    auto *form = new QFormLayout;
    auto *density = new UiComboBox;
    density->addItems({QStringLiteral("标准"), QStringLiteral("紧凑")});
    density->setCurrentIndex(preferences.value("compact").toBool() ? 1 : 0);
    auto *font = new QSpinBox;
    font->setObjectName("preferenceFont");
    font->setRange(12, 18);
    font->setValue(preferences.value("font_size").toInt(14));
    font->setSuffix(QStringLiteral(" px"));
    auto *batch = new QSpinBox;
    batch->setObjectName("preferenceBatch");
    batch->setRange(1, 2);
    batch->setValue(preferences.value("batch_limit").toInt(1));
    form->addRow(QStringLiteral("界面密度"), density);
    form->addRow(QStringLiteral("基础字号"), font);
    form->addRow(QStringLiteral("批处理并发"), batch);
    layout->addLayout(form);
    auto *draft = new QCheckBox(QStringLiteral("新会话默认保存输入草稿"));
    draft->setObjectName("preferenceDraft");
    draft->setChecked(preferences.value("default_draft").toBool());
    auto *reveal = new QCheckBox(QStringLiteral("处理完成后显示结果表"));
    reveal->setChecked(preferences.value("reveal_results").toBool(true));
    layout->addWidget(draft);
    layout->addWidget(reveal);
    auto *hint = new QLabel(QStringLiteral("系统 DPI "
                                           "缩放自动应用。默认一个批处理名额，调度设置影响后续作业"
                                           "。草稿包含文本和文件清单，保存在本工作区。"));
    hint->setWordWrap(true);
    layout->addWidget(hint);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    layout->addWidget(buttons);
    buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("应用"));
    buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted)
        return;
    preferences = {{"font_size", font->value()},
                   {"compact", density->currentIndex() == 1},
                   {"batch_limit", batch->value()},
                   {"default_draft", draft->isChecked()},
                   {"reveal_results", reveal->isChecked()}};
    applyPreferences();
    if (!saveState())
        QMessageBox::critical(this, QStringLiteral("保存设置失败"), storageError());
}
void Workbench::updateActivity() {
    if (!activity)
        return;
    const int count = scheduler ? scheduler->activeCount() : 0;
    activity->setVisible(count > 0 || unreadErrors > 0);
    activity->setText(count > 0 ? QStringLiteral("运行/排队 · %1").arg(count)
                                : QStringLiteral("失败 · %1").arg(unreadErrors));
}
void Workbench::showHistory() {
    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("运行详情"));
    dialog.resize(820, 620);
    auto *layout = new QVBoxLayout(&dialog);
    auto *list = new QListWidget;
    list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    layout->addWidget(list, 1);
    auto *details = new QPlainTextEdit;
    details->setReadOnly(true);
    details->setMaximumHeight(210);
    layout->addWidget(details);
    auto *row = new QHBoxLayout;
    auto *locate = new QPushButton(QStringLiteral("定位会话"));
    auto *cancel = new QPushButton(QStringLiteral("取消所选作业"));
    auto *output = new QPushButton(QStringLiteral("打开输出位置"));
    auto *jobDirectory = new QPushButton(QStringLiteral("打开作业目录"));
    row->addWidget(locate);
    row->addWidget(cancel);
    row->addWidget(output);
    row->addWidget(jobDirectory);
    layout->addLayout(row);
    const auto stored = repository ? repository->history() : QJsonArray{};
    auto selected = [&] {
        return list->currentItem() ? list->currentItem()->data(Qt::UserRole).toJsonObject()
                                   : QJsonObject{};
    };
    auto outputPath = [&](const QJsonObject &record) {
        auto path = record.value("first_output").toString();
        return path.replace("{data}", config.dataRoot);
    };
    auto sessionFor = [&](const QJsonObject &record) {
        for (auto it = sessions.cbegin(); it != sessions.cend(); ++it)
            if (it->uuid == record.value("session").toString())
                return it.key();
        return 0;
    };
    auto showDetails = [&] {
        const auto record = selected();
        const auto state = record.value("state").toString();
        const bool active = QStringList{"preparing", "queued", "running"}.contains(state);
        details->setPlainText(
            QStringLiteral(R"(工具  %1 · %2
会话  %3
动作  %4
状态  %5
进度  %6 / %7
取消能力  %8
输出项数  %9
首个输出  %10
作业 ID  %11

%12)")
                .arg(record.value("name").toString(), record.value("version").toString(),
                     record.value("session_name").toString(record.value("session").toString()),
                     record.value("action_label").toString(record.value("action").toString()),
                     state)
                .arg(record.value("completed").toInt())
                .arg(record.value("total").toInt())
                .arg(active && record.value("cancellable").toBool()
                         ? QStringLiteral("可请求取消")
                         : QStringLiteral("当前不可取消"))
                .arg(record.value("output_count").toInt())
                .arg(outputPath(record), record.value("id").toString(),
                     record.value("message").toString()));
        details->appendPlainText(QStringLiteral("失败/跳过项数  %1")
                                     .arg(record.value("failed").toInt(-1) < 0
                                              ? QStringLiteral("未报告")
                                              : QString::number(record.value("failed").toInt())));
        if (record.value("partial_effects").toBool())
            details->appendPlainText(
                QStringLiteral("进程异常退出，可能已产生输出，请核对工具数据与作业目录。"));
        locate->setEnabled(sessionFor(record) > 0);
        cancel->setEnabled(active && record.value("cancellable").toBool());
        const auto path = outputPath(record);
        output->setEnabled(QDir::isAbsolutePath(path) && QFileInfo::exists(path));
        jobDirectory->setEnabled(
            !Files::child(config.dataRoot, record.value("job_dir").toString(), true).isEmpty());
    };
    auto populate = [&] {
        const auto previous = selected().value("id").toString();
        QMap<QString, QJsonObject> all;
        for (const auto &value : stored) {
            const auto record = value.toObject();
            all[record.value("id").toString()] = record;
        }
        if (scheduler)
            for (const auto &job : scheduler->records()) {
                QString title;
                for (const auto &session : sessions)
                    if (session.uuid == job.request.sessionId)
                        title = session.title;
                all[job.request.jobId] = QJsonObject{
                    {"id", job.request.jobId},
                    {"created", job.created},
                    {"session", job.request.sessionId},
                    {"session_name", title},
                    {"name", job.request.tool.name},
                    {"version", job.request.tool.version},
                    {"action", job.request.action.id},
                    {"action_label", job.request.action.label},
                    {"state", job.state},
                    {"message", job.message},
                    {"completed", job.completed},
                    {"total", job.total},
                    {"cancellable", job.cancellable},
                    {"failed", job.result.value("failed")},
                    {"partial_effects", job.result.value("partial_effects")},
                    {"output_count", job.result.value("output_count")},
                    {"first_output", job.result.value("first_output")},
                    {"job_dir", QDir(config.dataRoot).relativeFilePath(job.request.jobRoot)}};
            }
        auto records = all.values();
        std::sort(records.begin(), records.end(), [](const auto &a, const auto &b) {
            return a.value("created").toInteger() > b.value("created").toInteger();
        });
        const QSignalBlocker blocker(list);
        list->clear();
        int selectedRow = 0;
        for (int index = 0; index < qMin(500, int(records.size())); ++index) {
            const auto &record = records[index];
            auto *item = new QListWidgetItem(
                QStringLiteral(R"(%1  %2 · %3 · %4
%5)")
                    .arg(QDateTime::fromMSecsSinceEpoch(record.value("created").toInteger())
                             .toString("MM-dd hh:mm"),
                         record.value("name").toString(),
                         record.value("action_label").toString(record.value("action").toString()),
                         record.value("state").toString(),
                         record.value("message").toString().left(160)),
                list);
            item->setData(Qt::UserRole, record);
            if (record.value("id").toString() == previous)
                selectedRow = index;
        }
        if (list->count())
            list->setCurrentRow(selectedRow);
        showDetails();
    };
    connect(list, &QListWidget::currentRowChanged, &dialog, [&] { showDetails(); });
    connect(locate, &QPushButton::clicked, &dialog, [&] {
        const int id = sessionFor(selected());
        if (id) {
            dialog.accept();
            activateSession(id);
        }
    });
    connect(cancel, &QPushButton::clicked, &dialog, [&] {
        if (scheduler)
            scheduler->cancel(selected().value("id").toString());
    });
    connect(output, &QPushButton::clicked, &dialog, [&] {
        const auto path = outputPath(selected());
        if (QFileInfo::exists(path))
            QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
    });
    connect(jobDirectory, &QPushButton::clicked, &dialog, [&] {
        const auto path =
            Files::child(config.dataRoot, selected().value("job_dir").toString(), true);
        if (!path.isEmpty())
            QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    });
    bool dirty = false;
    QTimer refresh;
    refresh.setInterval(500);
    if (scheduler)
        connect(scheduler, &Scheduler::updated, &dialog, [&] { dirty = true; });
    connect(&refresh, &QTimer::timeout, &dialog, [&] {
        if (dirty) {
            dirty = false;
            populate();
        }
    });
    refresh.start();
    populate();
    dialog.exec();
    historyReviewedAt = QDateTime::currentMSecsSinceEpoch();
    unreadErrors = 0;
    updateActivity();
    saveState();
}
