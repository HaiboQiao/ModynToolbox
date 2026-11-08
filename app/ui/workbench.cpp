#include "workbench.h"
#include <QCloseEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QTimer>
#include <functional>
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
Workbench::Workbench(QWidget *parent) : QMainWindow(parent) {
    setWindowTitle(QStringLiteral("石墨工具箱 · 原生架构试点"));
    resize(1060, 700);
    auto *root = new QWidget;
    auto *layout = new QHBoxLayout(root);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    auto *sidebar = new QWidget;
    sidebar->setObjectName("sidebar");
    sidebar->setFixedWidth(180);
    auto *sideLayout = new QVBoxLayout(sidebar);
    sideLayout->setContentsMargins(12, 20, 12, 16);
    auto *brand = new QLabel(QStringLiteral("石 墨"));
    sideLayout->addWidget(brand);
    auto *search = new QLineEdit;
    search->setPlaceholderText(QStringLiteral("搜索任务"));
    sideLayout->addWidget(search);
    navigation = new QListWidget;
    sideLayout->addWidget(navigation);
    auto *create = new QPushButton(QStringLiteral("新建试点会话"));
    sideLayout->addWidget(create);
    workspace = new QStackedWidget;
    layout->addWidget(sidebar);
    layout->addWidget(workspace, 1);
    setCentralWidget(root);
    setStyleSheet(QStringLiteral(
        "QWidget{background:#25282e;color:#eff1f5;font-family:'Segoe UI','Microsoft YaHei UI';font-size:14px;}"
        "QWidget#sidebar,QListWidget{background:#1e2126;}"
        "QLineEdit,QPlainTextEdit{background:#2c3037;border:1px solid #748094;border-radius:5px;padding:8px;}"
        "QPushButton{background:#363e4a;border:1px solid #748094;border-radius:5px;padding:8px 12px;}"
        "QPushButton:hover{background:#454f5d;}QPushButton:focus{border:2px solid #b6c6e5;}"
        "QListWidget{border:0;}QListWidget::item{padding:10px 6px;}QListWidget::item:selected{background:#363e4a;}"
        "QLabel#heading{font-size:20px;font-weight:600;}"
    ));
    connect(create, &QPushButton::clicked, this, [this]{ addSession(); });
    connect(navigation, &QListWidget::currentRowChanged, this, [this](int row){
        auto *item = navigation->item(row);
        if (!item) return;
        const int id = item->data(Qt::UserRole).toInt();
        auto &session = sessions[id];
        if (session.floating) {
            session.floating->showNormal();
            session.floating->raise();
            session.floating->activateWindow();
        } else {
            workspace->setCurrentWidget(session.view);
        }
    });
    connect(search, &QLineEdit::textChanged, this, [this](const QString &query){
        for(int row = 0; row < navigation->count(); ++row) {
            auto *item = navigation->item(row);
            item->setHidden(!item->text().contains(query, Qt::CaseInsensitive));
        }
    });
    addSession();
    addSession();
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
int Workbench::addSession() {
    const int id = nextId++;
    auto *view = new QWidget;
    auto *layout = new QVBoxLayout(view);
    layout->setContentsMargins(28, 24, 28, 24);
    auto *header = new QHBoxLayout;
    auto *heading = new QLabel(QStringLiteral("独立任务 %1").arg(id));
    heading->setObjectName("heading");
    header->addWidget(heading);
    header->addStretch();
    auto *detach = new QPushButton(QStringLiteral("独立窗口"));
    header->addWidget(detach);
    layout->addLayout(header);
    auto *description = new QLabel(QStringLiteral("原生窗口与状态保留试点。此处输入仅用于验证会话，业务工具将在后续阶段接入。"));
    description->setWordWrap(true);
    layout->addWidget(description);
    auto *input = new QPlainTextEdit;
    input->setPlaceholderText(QStringLiteral("输入当前任务的内容，分离和收回后应完整保留。"));
    layout->addWidget(input, 1);
    auto *status = new QLabel(QStringLiteral("本会话独立保存输入 · 数据处理待接入"));
    layout->addWidget(status);
    sessions.insert(id, {view, input, nullptr});
    workspace->addWidget(view);
    auto *item = new QListWidgetItem(QStringLiteral("任务 %1").arg(id), navigation);
    item->setData(Qt::UserRole, id);
    navigation->setCurrentItem(item);
    connect(detach, &QPushButton::clicked, this, [this,id]{
        if (isDetached(id)) attachSession(id); else detachSession(id);
    });
    return id;
}
void Workbench::detachSession(int id) {
    auto it = sessions.find(id);
    if (it == sessions.end() || it->floating) return;
    auto *window = new ToolWindow;
    window->setWindowTitle(QStringLiteral("石墨 · 任务 %1").arg(id));
    window->resize(760, 560);
    window->returnToHost = [this,id]{ attachSession(id); };
    workspace->removeWidget(it->view);
    window->setCentralWidget(it->view);
    it->floating = window;
    window->show();
}
void Workbench::attachSession(int id) {
    auto it = sessions.find(id);
    if (it == sessions.end() || !it->floating) return;
    auto *window = it->floating;
    window->takeCentralWidget();
    it->floating = nullptr;
    workspace->addWidget(it->view);
    workspace->setCurrentWidget(it->view);
    window->hide();
    window->deleteLater();
}
