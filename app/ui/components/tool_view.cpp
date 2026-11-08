#include "tool_view.h"
#include "icons.h"
#include "ui/theme/theme.h"
#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include "ui/components/combo_box.h"
#include <QDragEnterEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QGridLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMimeData>
#include <QPlainTextEdit>
#include <QTextDocument>
#include <QPushButton>
#include <QSet>
#include <QSlider>
#include <QTableWidget>
#include <QUrl>
#include <QVBoxLayout>
#include <functional>
#include <memory>
#include <QHBoxLayout>
namespace {
constexpr int maximumFiles = 1000;
class FileDropArea : public QFrame {
public:
    std::function<void(const QStringList &)> acceptFiles;
    explicit FileDropArea(QWidget *parent = nullptr) : QFrame(parent) { setAcceptDrops(true); }
protected:
    void dragEnterEvent(QDragEnterEvent *event) override {
        if (event->mimeData()->hasUrls()) event->acceptProposedAction();
    }
    void dropEvent(QDropEvent *event) override {
        QStringList paths;
        for (const auto &url : event->mimeData()->urls()) if (url.isLocalFile()) paths.append(url.toLocalFile());
        if (acceptFiles) acceptFiles(paths);
        event->acceptProposedAction();
    }
};
class ResponsiveInput : public QWidget {
public:
    explicit ResponsiveInput(QWidget *drop, QWidget *settings) : drop(drop), settings(settings) {
        grid = new QGridLayout(this);
        grid->setContentsMargins(0,0,0,0);
        grid->setSpacing(Theme::panelInset);
        grid->addWidget(drop,0,0);
        grid->addWidget(settings,0,1);
        grid->setColumnStretch(0,1);
        grid->setColumnStretch(1,1);
    }
protected:
    void resizeEvent(QResizeEvent *event) override {
        const bool narrow = width() < 570;
        if (narrow != stacked) {
            stacked = narrow;
            grid->removeWidget(settings);
            grid->addWidget(settings,stacked ? 1 : 0,stacked ? 0 : 1);
            grid->setColumnStretch(1,stacked ? 0 : 1);
        }
        QWidget::resizeEvent(event);
    }
private:
    QWidget *drop;
    QWidget *settings;
    QGridLayout *grid;
    bool stacked = false;
};
QPushButton *pendingProcessingAction() {
    auto *button = new QPushButton(QStringLiteral("开始处理"));
    button->setProperty("primary",true);
    button->setEnabled(false);
    button->setToolTip(QStringLiteral("处理后端将在对应工具阶段接入"));
    return button;
}
QWidget *parameters(const ViewSpec &spec) {
    auto *widget = new QWidget;
    auto *layout = new QVBoxLayout(widget);
    layout->setContentsMargins(0,0,0,0);
    layout->setSpacing(Theme::gap);
    auto *heading = new QLabel(QStringLiteral("参数设置"));
    layout->addWidget(heading);
    for (const auto &field : spec.fields) {
        auto *label = new QLabel(field.label);
        label->setObjectName("secondary");
        label->setWordWrap(true);
        layout->addWidget(label);
        if (field.type == "choice") {
            auto *choice = new UiComboBox;
            choice->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
            choice->setMinimumContentsLength(10);
            choice->addItems(field.options);
            choice->setCurrentText(field.value);
            choice->setObjectName(field.id);
            choice->setAccessibleName(field.label);
            label->setBuddy(choice);
            layout->addWidget(choice);
        } else if (field.type == "range") {
            auto *row = new QHBoxLayout;
            auto *slider = new QSlider(Qt::Horizontal);
            slider->setRange(field.minimum,field.maximum);
            slider->setValue(field.number);
            slider->setObjectName(field.id);
            slider->setAccessibleName(field.label);
            label->setBuddy(slider);
            auto *value = new QLabel(QString::number(field.number));
            value->setMinimumWidth(28);
            QObject::connect(slider,&QSlider::valueChanged,value,[value](int number) { value->setText(QString::number(number)); });
            row->addWidget(slider,1);
            row->addWidget(value);
            layout->addLayout(row);
        } else {
            auto *input = new QLineEdit(field.value);
            input->setObjectName(field.id);
            input->setAccessibleName(field.label);
            input->setMaxLength(512);
            label->setBuddy(input);
            layout->addWidget(input);
        }
    }
    layout->addStretch();
    return widget;
}
}
RenderedView renderToolView(const ViewSpec &spec) {
    auto *panel = new QWidget;
    panel->setObjectName("toolPanel");
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(Theme::panelInset,Theme::panelInset,Theme::panelInset,Theme::panelInset);
    layout->setSpacing(Theme::gap);
    if (!spec.error.isEmpty()) {
        auto *error = new QLabel(spec.error);
        error->setObjectName("viewError");
        error->setWordWrap(true);
        layout->addWidget(error);
        layout->addStretch();
        return {panel,nullptr};
    }
    if (spec.kind == "text") {
        if (!spec.fields.isEmpty()) layout->addWidget(parameters(spec));
        auto *label = new QLabel(spec.inputTitle);
        layout->addWidget(label);
        auto *editor = new QPlainTextEdit;
        editor->setObjectName("toolEditor");
        editor->setAccessibleName(spec.inputTitle);
        editor->setPlaceholderText(spec.placeholder);
        editor->setPlainText(spec.initialText);
        editor->setMinimumHeight(240);
        editor->setLineWrapMode(QPlainTextEdit::NoWrap);
        layout->addWidget(editor,1);
        auto *footer = new QHBoxLayout;
        auto *count = new QLabel;
        count->setObjectName("secondary");
        auto updateCount = [editor,count] { count->setText(QStringLiteral("%1 字符").arg(editor->document()->characterCount()-1)); };
        QObject::connect(editor,&QPlainTextEdit::textChanged,count,updateCount);
        updateCount();
        footer->addWidget(count);
        footer->addStretch();
        auto *copy = new QPushButton(QStringLiteral("复制"));
        QObject::connect(copy,&QPushButton::clicked,editor,[editor] { QApplication::clipboard()->setText(editor->toPlainText()); });
        footer->addWidget(copy);
        auto *clear = new QPushButton(QStringLiteral("清空"));
        QObject::connect(clear,&QPushButton::clicked,editor,&QPlainTextEdit::clear);
        footer->addWidget(clear);
        footer->addWidget(pendingProcessingAction());
        layout->addLayout(footer);
        return {panel,editor};
    }
    auto *drop = new FileDropArea;
    drop->setObjectName("fileDropArea");
    drop->setMinimumHeight(210);
    auto *dropLayout = new QVBoxLayout(drop);
    dropLayout->setContentsMargins(20,20,20,20);
    dropLayout->addStretch();
    auto *symbol = new QLabel;
    symbol->setPixmap(uiIcon(spec.extensions.contains("jpg") ? UiIcon::Image : UiIcon::Files,Theme::accent).pixmap(36,36));
    symbol->setAlignment(Qt::AlignCenter);
    dropLayout->addWidget(symbol);
    auto *title = new QLabel(spec.inputTitle);
    title->setAlignment(Qt::AlignCenter);
    title->setWordWrap(true);
    dropLayout->addWidget(title);
    auto *hint = new QLabel(spec.hint);
    hint->setObjectName("secondary");
    hint->setAlignment(Qt::AlignCenter);
    hint->setWordWrap(true);
    dropLayout->addWidget(hint);
    auto *add = new QPushButton(uiIcon(UiIcon::Add,Qt::white),QStringLiteral("添加文件"));
    add->setProperty("primary",true);
    add->setObjectName("addFiles");
    dropLayout->addWidget(add,0,Qt::AlignHCenter);
    dropLayout->addStretch();
    layout->addWidget(new ResponsiveInput(drop,parameters(spec)));
    auto *heading = new QLabel(QStringLiteral("文件列表"));
    layout->addWidget(heading);
    auto *table = new QTableWidget(0,3);
    table->setObjectName("inputFiles");
    table->setHorizontalHeaderLabels({QStringLiteral("文件名"),QStringLiteral("大小"),QStringLiteral("状态")});
    table->horizontalHeader()->setSectionResizeMode(0,QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(1,QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(2,QHeaderView::ResizeToContents);
    table->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    table->verticalHeader()->hide();
    table->verticalHeader()->setDefaultSectionSize(42);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setShowGrid(false);
    table->setMinimumHeight(180);
    layout->addWidget(table,1);
    auto *notice = new QLabel(QStringLiteral("尚未添加文件 · 最多 1000 个"));
    notice->setObjectName("secondary");
    notice->setWordWrap(true);
    auto *footer = new QHBoxLayout;
    footer->addWidget(notice,1);
    footer->addWidget(pendingProcessingAction());
    layout->addLayout(footer);
    auto paths = std::make_shared<QSet<QString>>();
    const auto accept = [table,notice,paths,extensions=spec.extensions](const QStringList &files) {
        int rejected = 0;
        for (const auto &path : files) {
            QFileInfo info(path);
            const auto identity = info.canonicalFilePath().toCaseFolded();
            if (!info.isFile() || (!extensions.isEmpty() && !extensions.contains(info.suffix().toLower()))) { ++rejected; continue; }
            if (paths->contains(identity)) continue;
            if (paths->size() >= maximumFiles) { ++rejected; continue; }
            paths->insert(identity);
            const int row = table->rowCount();
            table->insertRow(row);
            auto *name = new QTableWidgetItem(info.fileName());
            name->setToolTip(info.absoluteFilePath());
            table->setItem(row,0,name);
            table->setItem(row,1,new QTableWidgetItem(QStringLiteral("%1 KB").arg(info.size()/1024.0,0,'f',1)));
            table->setItem(row,2,new QTableWidgetItem(QStringLiteral("已添加")));
        }
        notice->setText(QStringLiteral("%1 个文件 · %2 个未接收").arg(table->rowCount()).arg(rejected));
    };
    drop->acceptFiles = accept;
    QObject::connect(add,&QPushButton::clicked,panel,[panel,accept] {
        accept(QFileDialog::getOpenFileNames(panel,QStringLiteral("选择输入文件")));
    });
    return {panel,nullptr};
}
