#include "tool_panel.h"
#include "combo_box.h"
#include "video_editor.h"
#include <QMessageBox>
#include <QCompleter>
#include <QGroupBox>
#include <QToolButton>
#include "icons.h"
#include "core/files.h"
#include "ui/theme/theme.h"
#include <QApplication>
#include <QDesktopServices>
#include <QGridLayout>
#include <QJsonDocument>
#include <QCheckBox>
#include <algorithm>
#include <QClipboard>
#include <QDragEnterEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QDir>
#include <QFutureWatcher>
#include <QHeaderView>
#include <QImageReader>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QMimeData>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QSaveFile>
#include <QScrollArea>
#include <QScrollBar>
#include <QSet>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include "ui/components/spin_box.h"
#include <QStyle>
#include <QStringConverter>
#include <QTableWidget>
#include <QTextDocument>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QtConcurrent>
#include <functional>
namespace {
constexpr int PreviewCharacters = 32768;
}
class FullTextEdit : public QPlainTextEdit {
  public:
    explicit FullTextEdit(int limit) : maximumBytes(limit) {
    }
    QString content() const {
        return preview ? full : toPlainText();
    }
    bool isPreview() const {
        return preview;
    }
    std::function<void(const QString &)> rejected;
    void setContent(const QString &value) {
        if (value.toUtf8().size() > maximumBytes) {
            if (rejected)
                rejected(QStringLiteral("输入超过工具容量限制，未替换已有内容"));
            return;
        }
        full = value;
        preview = value.size() > PreviewCharacters;
        auto visible = preview ? value.left(PreviewCharacters) : value;
        if (!visible.isEmpty() && visible.back().isHighSurrogate())
            visible.chop(1);
        QSignalBlocker blocker(this);
        QPlainTextEdit::setPlainText(visible);
        setReadOnly(blocked || preview);
        blocker.unblock();
        emit textChanged();
    }
    void setBusy(bool value) {
        blocked = value;
        setReadOnly(blocked || preview);
    }

  protected:
    void insertFromMimeData(const QMimeData *source) override {
        if (isReadOnly() || !source->hasText())
            return;
        auto value = toPlainText();
        const auto cursor = textCursor();
        value.replace(cursor.selectionStart(), cursor.selectionEnd() - cursor.selectionStart(),
                      source->text());
        if (value.toUtf8().size() > maximumBytes) {
            if (rejected)
                rejected(QStringLiteral("输入超过工具容量限制，未接收本次粘贴"));
            return;
        }
        if (value.size() > PreviewCharacters)
            setContent(value);
        else
            QPlainTextEdit::insertFromMimeData(source);
    }

  private:
    QString full;
    int maximumBytes;
    bool preview = false, blocked = false;
};
namespace {
QLabel *label(const QString &text, const QString &name = {}) {
    auto *result = new QLabel(text);
    result->setTextFormat(Qt::PlainText);
    result->setWordWrap(true);
    result->setObjectName(name);
    return result;
}
class DropArea : public QFrame {
  public:
    std::function<void(const QStringList &)> receive;
    DropArea() {
        setAcceptDrops(true);
        setObjectName("fileDropArea");
    }

  protected:
    void dragEnterEvent(QDragEnterEvent *event) override {
        if (event->mimeData()->hasUrls())
            event->acceptProposedAction();
    }
    void dropEvent(QDropEvent *event) override {
        QStringList files;
        for (const auto &url : event->mimeData()->urls())
            if (url.isLocalFile())
                files.append(url.toLocalFile());
        if (receive)
            receive(files);
        event->acceptProposedAction();
    }
};
class ActionRow : public QWidget {
  public:
    ActionRow() {
        grid = new QGridLayout(this);
        grid->setContentsMargins(0, 0, 0, 0);
    }
    void append(QWidget *control) {
        controls.append(control);
        reflow();
    }

  protected:
    void resizeEvent(QResizeEvent *event) override {
        reflow();
        QWidget::resizeEvent(event);
    }

  private:
    void reflow() {
        int columns = width() >= 720 ? 4 : width() >= 420 ? 2 : 1;
        for (int i = 0; i < controls.size(); ++i)
            grid->addWidget(controls[i], i / columns, i % columns);
    }
    QGridLayout *grid;
    QVector<QWidget *> controls;
};
class AdaptiveRow : public QWidget {
  public:
    AdaptiveRow() {
        box = new QBoxLayout(QBoxLayout::LeftToRight, this);
        box->setContentsMargins(0, 0, 0, 0);
        box->setSpacing(20);
    }
    void append(QWidget *widget, int stretch = 1) {
        box->addWidget(widget, qBound(1, stretch, 4));
    }

  protected:
    void resizeEvent(QResizeEvent *event) override {
        box->setDirection(width() < 650 ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight);
        QWidget::resizeEvent(event);
    }

  private:
    QBoxLayout *box;
};
QPushButton *button(const QString &text) {
    return new QPushButton(text);
}
} // namespace
ToolPanel::ToolPanel(ToolDefinition definition, ViewSpec view, QWidget *parent)
    : QWidget(parent), tool(std::move(definition)), spec(std::move(view)) {
    setObjectName("toolPanel");
    setAttribute(Qt::WA_StyledBackground, true);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);
    if (!spec.mediaProbe.isEmpty()) {
        QSet<QString> ids;
        for (const auto &a : tool.actions) ids.insert(a.id);
        for (const auto &id : {spec.mediaProbe,spec.mediaPrepare,spec.mediaExport})
            if (!ids.contains(id)) spec.error = QStringLiteral("媒体动作未在工具清单声明");
    }
    if (!spec.error.isEmpty()) {
        layout->addWidget(label(spec.error, "viewError"));
        layout->addStretch();
        return;
    }
    auto tree = spec.layout;
    if (tree.isEmpty()) {
        QJsonArray children;
        if (spec.kind == "files")
            children.append(QJsonObject{{"type", "row"},
                                        {"children", QJsonArray{QJsonObject{{"type", "files"}},
                                                                QJsonObject{{"type", "fields"}}}}});
        else {
            if (!spec.fields.isEmpty())
                children.append(QJsonObject{{"type", "fields"}});
            children.append(QJsonObject{{"type", "text"}});
        }
        children.append(QJsonObject{{"type", "output_text"}});
        children.append(QJsonObject{{"type", "output_table"}});
        tree = {{"type", "column"}, {"children", children}};
    }
    auto *body = node(tree);
    auto *scroll = new QScrollArea;
    bodyScroll = scroll;
    scroll->setWidgetResizable(true);
    scroll->setWidget(body);
    body->setAutoFillBackground(false);
    scroll->viewport()->setAutoFillBackground(false);
    scroll->setMinimumHeight(120);
    layout->addWidget(scroll, 1);
    layout->addWidget(makeActions());
    auto *footer = new QHBoxLayout;
    status = label(tool.executable ? QStringLiteral("就绪")
                                   : QStringLiteral("兼容预览包，未提供处理入口"),
                   "secondary");
    status->setObjectName("jobStatus");
    status->setMaximumHeight(42);
    status->setMinimumWidth(0);
    progress = new QProgressBar;
    progress->setFixedWidth(110);
    progress->hide();
    cancelButton = button(QStringLiteral("取消"));
    cancelButton->hide();
    elapsedLabel = label(QString(), "secondary");
    auto *elapsedTimer = new QTimer(this);
    elapsedTimer->setInterval(500);
    connect(elapsedTimer, &QTimer::timeout, this, [this] {
        if (busy && jobClock.isValid()) elapsedLabel->setText(QStringLiteral("已用 %1 s").arg(jobClock.elapsed()/1000));
    });
    elapsedTimer->start();
    footer->addWidget(status, 1);
    footer->addWidget(elapsedLabel);
    footer->addWidget(progress);
    footer->addWidget(cancelButton);
    layout->addLayout(footer);
    connect(cancelButton, &QPushButton::clicked, this, &ToolPanel::cancelRequested);
    if (textInput)
        textInput->rejected = [this](const QString &error) { setStatusText(error); };
    conditions();
    updateActions();
}
QWidget *ToolPanel::node(const QJsonObject &description) {
    const auto type = description.value("type").toString();
    if (type == "actions" || type == "status")
        return nullptr;
    if (type == "row") {
        auto *row = new AdaptiveRow;
        for (const auto &child : description.value("children").toArray())
            if (auto *widget = node(child.toObject()))
                row->append(widget, child.toObject().value("stretch").toInt(1));
        return row;
    }
    if (type == "column") {
        auto *column = new QWidget;
        auto *layout = new QVBoxLayout(column);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(12);
        for (const auto &child : description.value("children").toArray())
            if (auto *widget = node(child.toObject()))
                layout->addWidget(widget);
        return column;
    }
    if (type == "video_editor") {
        videoEditor = new VideoEditor(spec.extensions);
        connect(videoEditor, &VideoEditor::sourceChanged, this, [this] {
            if (restoring) return;
            pendingMediaSelection = {}; context = {}; inputChanged();
            QTimer::singleShot(0, this, [this] { if (!busy && videoEditor && !videoEditor->source().isEmpty()) trigger(spec.mediaProbe); });
        });
        connect(videoEditor, &VideoEditor::probeRequested, this, [this] { trigger(spec.mediaProbe); });
        connect(videoEditor, &VideoEditor::selectionChanged, this, [this] { inputChanged(); });
        connect(videoEditor, &VideoEditor::notice, this, &ToolPanel::setStatusText);
        return videoEditor;
    }
    if (type == "fields")
        return makeFields();
    if (type == "files")
        return makeFiles();
    if (type == "text")
        return makeText();
    if (type == "output_text") {
        auto *frame = new QWidget;
        auto *layout = new QVBoxLayout(frame);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->addWidget(label(QStringLiteral("结果"), "resultCaption"));
        textResult = new QPlainTextEdit;
        textResult->setReadOnly(true);
        textResult->setLineWrapMode(QPlainTextEdit::NoWrap);
        textResult->setMinimumHeight(160);
        textResult->setObjectName("textResult");
        layout->addWidget(textResult);
        auto *row = new QHBoxLayout;
        row->addStretch();
        auto *copy = button(QStringLiteral("复制完整结果"));
        auto *save = button(QStringLiteral("导出结果"));
        row->addWidget(copy);
        row->addWidget(save);
        layout->addLayout(row);
        connect(copy, &QPushButton::clicked, this,
                [this] { QApplication::clipboard()->setText(resultText()); });
        frozenInputs.append(save);
        connect(save, &QPushButton::clicked, this, &ToolPanel::exportText);
        frame->hide();
        frame->setObjectName("textResultFrame");
        return frame;
    }
    if (type == "output_table") {
        resultTable = new QTableWidget;
        resultTable->setObjectName("resultTable");
        resultTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
        resultTable->setTextElideMode(Qt::ElideMiddle);
        resultTable->verticalHeader()->hide();
        resultTable->horizontalHeader()->setStretchLastSection(true);
        resultTable->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        resultTable->setMinimumHeight(180);
        auto *frame = new QWidget;
        auto *layout = new QVBoxLayout(frame);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->addWidget(resultTable);
        auto *row = new QHBoxLayout;
        row->addStretch();
        auto *openFile = button(QStringLiteral("打开首个输出文件"));
        row->addWidget(openFile);
        connect(openFile, &QPushButton::clicked, this, [this] {
            const auto paths=lastResult.value("artifacts").toArray();
            if (!paths.isEmpty() && QDir::isAbsolutePath(paths.first().toString()) && QFileInfo(paths.first().toString()).isFile())
                QDesktopServices::openUrl(QUrl::fromLocalFile(paths.first().toString()));
        });
        auto *open = button(QStringLiteral("打开首个输出所在目录"));
        auto *save = button(QStringLiteral("导出结果表 JSON"));
        row->addWidget(open);
        row->addWidget(save);
        layout->addLayout(row);
        connect(open, &QPushButton::clicked, this, [this] {
            const auto paths = lastResult.value("artifacts").toArray();
            if (!paths.isEmpty()) {
                const auto path = paths.first().toString();
                if (QDir::isAbsolutePath(path) && QFileInfo::exists(path))
                    QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
            }
        });
        frozenInputs.append(save);
        connect(save, &QPushButton::clicked, this, [this] {
            const auto path =
                QFileDialog::getSaveFileName(this, QStringLiteral("导出结果表"), "results.json");
            if (path.isEmpty())
                return;
            writeExport(
                path,
                {{"columns", lastResult.value("columns")}, {"rows", lastResult.value("rows")}},
                false);
        });
        frame->hide();
        return frame;
    }
    if (type == "image_preview") {
        preview = label(QString());
        preview->setAlignment(Qt::AlignCenter);
        preview->hide();
        return preview;
    }
    return nullptr;
}
QWidget *ToolPanel::makeFields() {
    auto *frame = new QWidget;
    frozenInputs.append(frame);
    auto *layout = new QVBoxLayout(frame);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);
    if (!spec.fields.isEmpty())
        layout->addWidget(label(QStringLiteral("参数设置"), "sectionTitle"));
    frame->setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Maximum);
    frame->setObjectName("parameterFields"); frame->installEventFilter(this);
    auto *normalGrid = new QGridLayout;
    normalGrid->setObjectName("normalFieldGrid"); normalGrid->setAlignment(Qt::AlignTop);
    normalGrid->setColumnStretch(0, 1); normalGrid->setColumnStretch(1, 1);
    layout->addLayout(normalGrid);
    auto *advanced = new QWidget;
    advanced->setObjectName("advancedFields");
    auto *advancedGrid = new QGridLayout(advanced);
    advancedGrid->setObjectName("advancedFieldGrid"); advancedGrid->setAlignment(Qt::AlignTop);
    advancedGrid->setContentsMargins(0, 0, 0, 0);
    advancedGrid->setColumnStretch(0, 1); advancedGrid->setColumnStretch(1, 1);
    auto *toggle = new QPushButton(QStringLiteral("展开高级设置"));
    toggle->setIcon(uiIcon(UiIcon::Add,Theme::accent));
    toggle->setObjectName("advancedToggle"); toggle->setCheckable(true);
    toggle->setAccessibleName(QStringLiteral("展开或收起高级设置"));
    layout->addWidget(toggle,0,Qt::AlignLeft); layout->addWidget(advanced); advanced->hide();
    connect(toggle, &QPushButton::toggled, advanced, [advanced, toggle](bool on) {
        advanced->setVisible(on); toggle->setText(on ? QStringLiteral("收起高级设置") : QStringLiteral("展开高级设置"));
        toggle->setIcon(uiIcon(on ? UiIcon::Minimize : UiIcon::Add,Theme::accent));
    });
    bool hasAdvanced = false;
    int normalCell = 0, advancedCell = 0;
    for (const auto &field : spec.fields) {
        auto *container = new QWidget;
        auto *group = new QVBoxLayout(container);
        group->setContentsMargins(0, 0, 0, 0);
        group->setSpacing(5);
        auto *caption = label(field.label, "secondary");
        group->addWidget(caption);
        QWidget *input = nullptr;
        if (field.type == "choice") {
            auto *choice = new UiComboBox;
            choice->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
            choice->setMinimumContentsLength(8);
            for (int i=0; i<field.options.size(); ++i)
                choice->addItem(field.options[i], field.optionValues.value(i, field.options[i]));
            choice->setCurrentIndex(choice->findData(field.value));
            choice->setMaxVisibleItems(12);
            if (field.searchable) {
                choice->setEditable(true); choice->setInsertPolicy(QComboBox::NoInsert);
                choice->completer()->setCaseSensitivity(Qt::CaseInsensitive);
                choice->completer()->setFilterMode(Qt::MatchContains);
                choice->completer()->setCompletionMode(QCompleter::PopupCompletion);
                connect(choice->lineEdit(), &QLineEdit::textChanged, this, [this] { updateActions(); });
                choice->lineEdit()->setPlaceholderText(field.placeholder.isEmpty() ? QStringLiteral("输入关键字筛选，选择匹配项") : field.placeholder);
            }
            input = choice;
            connect(choice, &QComboBox::currentIndexChanged, this, [this] { inputChanged(true); });
        } else if (field.type == "range") {
            auto *row = new QHBoxLayout;
            auto *slider = new QSlider(Qt::Horizontal);
            slider->setRange(field.minimum, field.maximum);
            slider->setValue(field.number);
            input = slider;
            auto *value = label(QString::number(field.number), "secondary");
            value->setMinimumWidth(28);
            row->addWidget(slider, 1);
            row->addWidget(value);
            group->addLayout(row);
            connect(slider, &QSlider::valueChanged, this, [this, value](int number) {
                value->setText(QString::number(number));
                inputChanged(true);
            });
        } else if (field.type == "number") {
            auto *number = new UiSpinBox;
            number->setRange(field.minimum, field.maximum);
            number->setValue(field.number);
            input = number;
            connect(number, &QSpinBox::valueChanged, this, [this] { inputChanged(true); });
        } else if (field.type == "boolean") {
            auto *check = new QCheckBox(field.label);
            check->setChecked(field.boolean);
            caption->hide();
            input = check;
            connect(check, &QCheckBox::toggled, this, [this] { inputChanged(true); });
        } else {
            auto *text = new QLineEdit(field.value);
            text->setMaxLength(512);
            text->setPlaceholderText(field.placeholder);
            input = text;
            connect(text, &QLineEdit::textChanged, this, [this] { inputChanged(true); });
            if (field.type == "directory") {
                auto *row = new QHBoxLayout;
                auto *choose = button(QStringLiteral("选择"));
                row->addWidget(text, 1);
                row->addWidget(choose);
                group->addLayout(row);
                connect(choose, &QPushButton::clicked, this, [this, text] {
                    const auto path =
                        QFileDialog::getExistingDirectory(this, QStringLiteral("选择输出目录"));
                    if (!path.isEmpty())
                        text->setText(path);
                });
            }
        }
        input->setObjectName(field.id);
        input->setAccessibleName(field.label);
        caption->setBuddy(input);
        fields[field.id] = input;
        fieldContainers[field.id] = container;
        if (field.type != "range" && field.type != "directory")
            group->addWidget(input);
        if (!field.help.isEmpty()) {
            auto *help = label(field.help, "secondary"); help->setWordWrap(true); group->addWidget(help);
            input->setToolTip(field.help); input->setAccessibleDescription(field.help);
        }
        auto *grid = field.advanced ? advancedGrid : normalGrid;
        int &cell = field.advanced ? advancedCell : normalCell;
        hasAdvanced |= field.advanced;
        if (!field.compact && cell % 2) ++cell;
        grid->addWidget(container, cell/2, cell%2, 1, field.compact ? 1 : 2);
        cell += field.compact ? 1 : 2;
    }
    toggle->setVisible(hasAdvanced);
    layout->addStretch();
    return frame;
}
bool ToolPanel::eventFilter(QObject *object, QEvent *event) {
    if (object->objectName() == "parameterFields" && event->type() == QEvent::Resize) {
        auto *frame = qobject_cast<QWidget *>(object);
        const bool narrow = frame->width() < 460;
        if (!frame->property("narrowFields").isValid() || frame->property("narrowFields").toBool() != narrow) {
            frame->setProperty("narrowFields", narrow);
            for (const auto &name : {"normalFieldGrid", "advancedFieldGrid"}) {
                auto *grid=frame->findChild<QGridLayout *>(name);
                if (!grid) continue;
                while (auto *item=grid->takeAt(0)) delete item;
                int cell=0;
                for (const auto &field : spec.fields) {
                    if (field.advanced != (QString(name)=="advancedFieldGrid")) continue;
                    if (!field.compact && cell%2 && !narrow) ++cell;
                    grid->addWidget(fieldContainers.value(field.id), narrow ? cell : cell/2, narrow ? 0 : cell%2,
                                    1, narrow || field.compact ? 1 : 2);
                    cell += narrow || field.compact ? 1 : 2;
                }
                grid->setColumnStretch(1, narrow ? 0 : 1);
            }
        }
    }
    return QWidget::eventFilter(object, event);
}
QWidget *ToolPanel::makeText() {
    auto *frame = new QWidget;
    auto *layout = new QVBoxLayout(frame);
    layout->setContentsMargins(0, 0, 0, 0);
    auto *row = new QHBoxLayout;
    row->addWidget(label(spec.inputTitle));
    row->addStretch();
    auto *load = button(QStringLiteral("载入 UTF-8 文件"));
    row->addWidget(load);
    frozenInputs.append(load);
    layout->addLayout(row);
    textInput = new FullTextEdit(tool.limits.value("max_input_bytes").toInt(5 * 1024 * 1024));
    textInput->setObjectName("toolEditor");
    textInput->setAccessibleName(spec.inputTitle);
    textInput->setPlaceholderText(spec.placeholder);
    textInput->setLineWrapMode(QPlainTextEdit::NoWrap);
    textInput->setMinimumHeight(180);
    textInput->setContent(spec.initialText);
    layout->addWidget(textInput, 1);
    auto *count = label(QStringLiteral("0 字符"), "secondary");
    auto *bottom = new QHBoxLayout;
    bottom->addWidget(count);
    bottom->addStretch();
    auto *copy = button(QStringLiteral("复制"));
    auto *clear = button(QStringLiteral("清空"));
    bottom->addWidget(copy);
    bottom->addWidget(clear);
    frozenInputs.append(clear);
    layout->addLayout(bottom);
    connect(textInput, &QPlainTextEdit::textChanged, this,
            [this, count, last = textInput->content()]() mutable {
                if (!textInput->isPreview() &&
                    textInput->document()->characterCount() - 1 > PreviewCharacters) {
                    textInput->setContent(textInput->content());
                    return;
                }
                const auto current = textInput->content();
                if (current == last)
                    return;
                last = current;
                count->setText(
                    QStringLiteral("%1 字符%2")
                        .arg(textInput->content().size())
                        .arg(textInput->isPreview()
                                 ? QStringLiteral(" · 大文本只读预览，处理和复制使用全部内容")
                                 : QString()));
                inputChanged();
            });
    connect(copy, &QPushButton::clicked, this,
            [this] { QApplication::clipboard()->setText(textInput->content()); });
    connect(clear, &QPushButton::clicked, this, [this] {
        textInput->setContent({});
        inputChanged();
    });
    connect(load, &QPushButton::clicked, this, &ToolPanel::loadText);
    return frame;
}
QWidget *ToolPanel::makeFiles() {
    auto *frame = new QWidget;
    frozenInputs.append(frame);
    auto *layout = new QVBoxLayout(frame);
    layout->setContentsMargins(0, 0, 0, 0);
    auto *drop = new DropArea;
    drop->setMinimumHeight(140);
    drop->receive = [this](const QStringList &paths) { addPaths(paths); };
    auto *dropLayout = new QVBoxLayout(drop);
    dropLayout->setContentsMargins(16, 16, 16, 16);
    auto *symbol = label(QString());
    symbol->setPixmap(
        uiIcon(spec.extensions.contains("jpg") ? UiIcon::Image : UiIcon::Files, Theme::accent)
            .pixmap(30, 30));
    symbol->setAlignment(Qt::AlignCenter);
    auto *title = label(spec.inputTitle);
    title->setAlignment(Qt::AlignCenter);
    auto *hint = label(spec.hint, "secondary");
    hint->setAlignment(Qt::AlignCenter);
    auto *add = button(QStringLiteral("添加文件"));
    add->setObjectName("addFiles");
    add->setProperty("primary", true);
    dropLayout->addWidget(symbol);
    dropLayout->addWidget(title);
    dropLayout->addWidget(hint);
    dropLayout->addWidget(add, 0, Qt::AlignHCenter);
    connect(add, &QPushButton::clicked, this, [this] {
        addPaths(QFileDialog::getOpenFileNames(this, QStringLiteral("选择输入文件")));
    });
    layout->addWidget(drop);
    auto *toolbar = new QHBoxLayout;
    for (const auto &entry : QStringList{QStringLiteral("上移"), QStringLiteral("下移"),
                                         QStringLiteral("移除选中行"), QStringLiteral("清空")}) {
        auto *action = button(entry);
        toolbar->addWidget(action);
        connect(action, &QPushButton::clicked, this, [this, entry] {
            QSignalBlocker blocker(fileTable);
            const int row = fileTable->currentRow();
            if (entry == QStringLiteral("清空"))
                fileTable->setRowCount(0);
            else if (entry == QStringLiteral("移除选中行")) {
                QSet<int> rows;
                for (auto *item : fileTable->selectedItems())
                    rows.insert(item->row());
                auto sorted = rows.values();
                std::sort(sorted.begin(), sorted.end(), std::greater<int>());
                for (int row : sorted)
                    fileTable->removeRow(row);
            } else {
                const int other = row + (entry == QStringLiteral("上移") ? -1 : 1);
                if (row < 0 || other < 0 || other >= fileTable->rowCount())
                    return;
                for (int column = 0; column < 2; ++column) {
                    auto *a = fileTable->takeItem(row, column);
                    auto *b = fileTable->takeItem(other, column);
                    fileTable->setItem(row, column, b);
                    fileTable->setItem(other, column, a);
                }
                fileTable->setCurrentCell(other, 0);
            }
            inputChanged();
        });
    }
    layout->addLayout(toolbar);
    fileTable = new QTableWidget(0, 2);
    fileTable->setObjectName("inputFiles");
    fileTable->setHorizontalHeaderLabels({QStringLiteral("文件"), QStringLiteral("大小")});
    fileTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    fileTable->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    fileTable->setColumnWidth(1, 80);
    fileTable->verticalHeader()->hide();
    fileTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    fileTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    fileTable->setMinimumHeight(150);
    layout->addWidget(fileTable);
    connect(fileTable, &QTableWidget::itemChanged, this, [this] { inputChanged(); });
    fileNotice = label(QStringLiteral("尚未添加文件"), "secondary");
    fileNotice->setObjectName("inputFileNotice");
    layout->addWidget(fileNotice);
    return frame;
}
QWidget *ToolPanel::makeActions() {
    auto *frame = new ActionRow;
    for (int index = 0; index < tool.actions.size(); ++index) {
        const auto &action = tool.actions[index];
        auto *control = button(action.label);
        control->setObjectName("action_" + action.id);
        if (index == 0)
            control->setProperty("primary", true);
        actions[action.id] = control;
        if (action.id == spec.mediaProbe) control->setVisible(false);
        frame->append(control);
        connect(control, &QPushButton::clicked, this, [this, id = action.id] { trigger(id); });
    }
    return frame;
}
QJsonObject ToolPanel::parameters() const {
    QJsonObject values;
    for (auto it = fields.cbegin(); it != fields.cend(); ++it) {
        if (auto *choice = qobject_cast<QComboBox *>(it.value()))
            values[it.key()] = choice->currentIndex() >= 0 && choice->currentText() == choice->itemText(choice->currentIndex()) ? choice->currentData().toString() : QString();
        else if (auto *slider = qobject_cast<QSlider *>(it.value()))
            values[it.key()] = slider->value();
        else if (auto *number = qobject_cast<QSpinBox *>(it.value()))
            values[it.key()] = number->value();
        else if (auto *check = qobject_cast<QCheckBox *>(it.value()))
            values[it.key()] = check->isChecked();
        else if (auto *text = qobject_cast<QLineEdit *>(it.value()))
            values[it.key()] = text->text();
    }
    if (videoEditor) {
        const auto selection = videoEditor->selection();
        for (auto i=selection.begin(); i!=selection.end(); ++i) values[i.key()] = i.value();
    }
    return values;
}
QJsonObject ToolPanel::input() const {
    QJsonArray files;
    if (fileTable)
        for (int row = 0; row < fileTable->rowCount(); ++row)
            if (fileTable->item(row, 0)->checkState() == Qt::Checked)
                files.append(fileTable->item(row, 0)->data(Qt::UserRole).toString());
    if (videoEditor && !videoEditor->source().isEmpty()) files.append(videoEditor->source());
    return {{"params", parameters()},
            {"files", files},
            {"text", textInput ? textInput->content() : QString()},
            {"context", context}};
}
QJsonObject ToolPanel::snapshot(bool includeInput) const {
    QJsonObject state{{"params", parameters()},
                      {"body_scroll", bodyScroll ? bodyScroll->verticalScrollBar()->value() : 0}};
    if (includeInput) {
        if (textInput) {
            state["text"] = textInput->content();
            const auto cursor = textInput->textCursor();
            state["cursor"] = cursor.position();
            state["anchor"] = cursor.anchor();
            state["scroll"] = textInput->verticalScrollBar()->value();
        }
        QJsonArray records;
        if (fileTable)
            for (int row = 0; row < fileTable->rowCount(); ++row)
                records.append(
                    QJsonObject{{"path", fileTable->item(row, 0)->data(Qt::UserRole).toString()},
                                {"size", fileTable->item(row, 1)->data(Qt::UserRole).toDouble()},
                                {"checked", fileTable->item(row, 0)->checkState() == Qt::Checked}});
        if (videoEditor) state["video"] = videoEditor->snapshot();
        state["files"] = records;
        state["context"] = context;
        state["result"] = lastResult;
    }
    return state;
}
void ToolPanel::appendRecords(const QJsonArray &records, bool replace) {
    if (!fileTable)
        return;
    QSignalBlocker blocker(fileTable);
    fileTable->setUpdatesEnabled(false);
    if (replace)
        fileTable->setRowCount(0);
    QSet<QString> existing;
    for (int row = 0; row < fileTable->rowCount(); ++row)
        existing.insert(fileTable->item(row, 0)->data(Qt::UserRole).toString().toCaseFolded());
    for (const auto &value : records) {
        const auto record =
            value.isString() ? QJsonObject{{"path", value}, {"checked", true}} : value.toObject();
        const auto path = record.value("path").toString();
        if (path.isEmpty() || existing.contains(path.toCaseFolded()))
            continue;
        existing.insert(path.toCaseFolded());
        const int row = fileTable->rowCount();
        fileTable->insertRow(row);
        auto *name = new QTableWidgetItem(QFileInfo(path).fileName());
        name->setData(Qt::UserRole, path);
        name->setToolTip(path);
        name->setFlags(name->flags() | Qt::ItemIsUserCheckable);
        name->setCheckState(record.value("checked").toBool(true) ? Qt::Checked : Qt::Unchecked);
        auto *size = new QTableWidgetItem(
            record.contains("size")
                ? QStringLiteral("%1 KB").arg(record.value("size").toDouble() / 1024.0, 0, 'f', 1)
                : QStringLiteral("待核验"));
        size->setData(Qt::UserRole, record.value("size").toDouble());
        fileTable->setItem(row, 0, name);
        fileTable->setItem(row, 1, size);
    }
    fileTable->setUpdatesEnabled(true);
    if (fileNotice)
        fileNotice->setText(QStringLiteral("%1 个文件").arg(fileTable->rowCount()));
}
bool ToolPanel::restore(const QJsonObject &state) {
    if (!spec.error.isEmpty())
        return false;
    if (state.value("files").toArray().size() > tool.limits.value("max_files").toInt(2000) ||
        state.value("text").toString().toUtf8().size() > 5 * 1024 * 1024) {
        if (status)
            setStatusText(QStringLiteral("保存的输入超限，未恢复"));
        return false;
    }
    restoring = true;
    if (spec.version >= 3) { context = state.value("context").toObject(); refreshOptions(); }
    const auto values = state.value("params").toObject();
    for (auto it = fields.begin(); it != fields.end(); ++it)
        if (values.contains(it.key())) {
            const auto value = values.value(it.key());
            if (auto *choice = qobject_cast<QComboBox *>(it.value()))
                { choice->setCurrentIndex(choice->findData(value.toString())); if (spec.version >= 3) refreshOptions(); }
            else if (auto *slider = qobject_cast<QSlider *>(it.value()))
                slider->setValue(value.toInt());
            else if (auto *number = qobject_cast<QSpinBox *>(it.value()))
                number->setValue(value.toInt());
            else if (auto *check = qobject_cast<QCheckBox *>(it.value()))
                check->setChecked(value.toBool());
            else if (auto *text = qobject_cast<QLineEdit *>(it.value()))
                text->setText(value.toString());
        }
    if (spec.version >= 3) for (const auto &field : spec.fields) {
        if (field.type != "choice" || !values.contains(field.id)) continue;
        refreshOptions();
        auto *choice=qobject_cast<QComboBox *>(fields.value(field.id));
        const QSignalBlocker block(choice);
        choice->setCurrentIndex(choice->findData(values.value(field.id).toString()));
    }
    if (textInput)
        textInput->setContent(state.value("text").toString());
    if (videoEditor) {
        auto mediaState = state.value("video").toObject();
        QString source = mediaState.value("source").toString();
        const auto records = state.value("files").toArray();
        if (source.isEmpty() && !records.isEmpty()) source = records.first().isString() ? records.first().toString() : records.first().toObject().value("path").toString();
        videoEditor->setSource(source);
        const auto media = state.value("context").toObject().value("media").toObject();
        if (!media.isEmpty()) videoEditor->setMedia(media);
        if (mediaState.isEmpty()) mediaState = values;
        pendingMediaSelection = mediaState;
        videoEditor->restoreSelection(mediaState);
    }
    appendRecords(state.value("files").toArray(), true);
    if (state.contains("result"))
        setResult(state.value("result").toObject());
    context = state.value("context").toObject();
    if (videoEditor && context.contains("media") && !videoEditor->setMedia(context.value("media").toObject())) {
        context = {};
        setStatusText(QStringLiteral("视频已变化或缺失，请重新选择并分析视频"));
    }
    if (videoEditor && !videoEditor->source().isEmpty() && !context.contains("media"))
        QTimer::singleShot(0, this, [this] { if (!busy) trigger(spec.mediaProbe); });
    if (textInput) {
        auto cursor = textInput->textCursor();
        cursor.setPosition(
            qMin(state.value("anchor").toInt(), textInput->document()->characterCount() - 1));
        cursor.setPosition(
            qMin(state.value("cursor").toInt(), textInput->document()->characterCount() - 1),
            QTextCursor::KeepAnchor);
        textInput->setTextCursor(cursor);
        QTimer::singleShot(0, this, [this, state] {
            textInput->verticalScrollBar()->setValue(state.value("scroll").toInt());
        });
    }
    if (bodyScroll)
        QTimer::singleShot(0, this, [this, state] {
            bodyScroll->verticalScrollBar()->setValue(state.value("body_scroll").toInt());
        });
    if (spec.version >= 3) refreshOptions();
    restoring = false;
    verifyRestoredFiles();
    conditions();
    updateActions();
    return true;
}
void ToolPanel::inputChanged(bool parameterOnly) {
    if (restoring)
        return;
    if (spec.version >= 3 && parameterOnly) { refreshOptions(); }
    else if (videoEditor && context.contains("media"))
        context = {{"media", context.value("media")}};
    else { context = {}; if (spec.version >= 3) refreshOptions(); }
    conditions();
    updateActions();
    emit changed();
}
void ToolPanel::refreshOptions() {
    for (const auto &field : spec.fields) {
        if (field.optionsSource.isEmpty()) continue;
        auto *choice = qobject_cast<QComboBox *>(fields.value(field.id));
        if (!choice) continue;
        QJsonValue source(context);
        for (const auto &part : field.optionsSource.split('.')) source = source.toObject().value(part);
        if (!field.optionsDependsOn.isEmpty()) source = source.toObject().value(parameters().value(field.optionsDependsOn).toString());
        QSet<QString> seen;
        QStringList titles, values;
        const auto options = source.toArray();
        if (options.size() <= 2000) for (const auto &option : options) {
            const auto title = option.isString() ? option.toString() : option.toObject().value("label").toString();
            const auto value = option.isString() ? option.toString() : option.toObject().value("value").toString();
            if (title.isEmpty() || title.size() > 160 || value.size() > 512 || seen.contains(value) || (!option.isString() && !option.toObject().value("value").isString())) continue;
            seen.insert(value); titles.append(title); values.append(value);
        }
        bool same=choice->count()==titles.size();
        for (int i=0; same && i<titles.size(); ++i)
            same=choice->itemText(i)==titles[i] && choice->itemData(i).toString()==values[i];
        choice->setPlaceholderText(titles.isEmpty() ? QStringLiteral("请先分析输入") : QStringLiteral("请选择"));
        // Do not reset a combo's model from inside its own selection signal.
        if (same) continue;
        const QSignalBlocker block(choice);
        const auto selected = choice->currentData().toString();
        choice->clear();
        for (int i=0; i<titles.size(); ++i) choice->addItem(titles[i],values[i]);
        int index = choice->findData(selected);
        if (index < 0) index = choice->findData(field.value);
        choice->setCurrentIndex(index);
        choice->setPlaceholderText(choice->count() ? QStringLiteral("请选择") : QStringLiteral("请先分析输入"));
    }
}
void ToolPanel::conditions() {
    const auto values = parameters();
    for (const auto &field : spec.fields)
        if (!field.condition.isEmpty()) {
            const auto value = values.value(field.condition.value("field").toString());
            const bool enabled = field.condition.contains("equals")
                                     ? value == field.condition.value("equals")
                                     : value != field.condition.value("not");
            fieldContainers[field.id]->setEnabled(enabled);
        }
}
void ToolPanel::updateActions() {
    bool validChoices=true;
    for (const auto &field : spec.fields) if (field.searchable) {
        auto *choice=qobject_cast<QComboBox *>(fields.value(field.id));
        if (choice && choice->currentText() != choice->itemText(choice->currentIndex()) && !choice->currentText().isEmpty()) validChoices=false;
    }
    for (const auto &action : tool.actions)
        if (actions.contains(action.id))
            actions[action.id]->setEnabled(
                tool.executable && validChoices && !busy && !scanning &&
                (action.requiredContext.isEmpty() || context.contains(action.requiredContext)));
    if (videoEditor) {
        const bool ready = context.contains("media");
        if (actions.contains(spec.mediaProbe)) actions[spec.mediaProbe]->setVisible(false);
        const bool fast = parameters().value(spec.mediaPrepareWhen.value("field").toString()) == spec.mediaPrepareWhen.value("equals");
        if (actions.contains(spec.mediaPrepare)) {
            actions[spec.mediaPrepare]->setVisible(fast);
            actions[spec.mediaPrepare]->setEnabled(!busy && ready && fast);
        }
        if (actions.contains(spec.mediaExport)) actions[spec.mediaExport]->setEnabled(!busy && ready && (!fast || context.contains("plan")));
    }
}
void ToolPanel::trigger(const QString &action) {
    if (!actions.contains(action) || !actions[action]->isEnabled()) return;
    if (videoEditor && action == spec.mediaExport && !context.value("plan").toObject().isEmpty() &&
        context.value("plan").toObject().value("fast").toBool()) {
        const auto plan=context.value("plan").toObject();
        if (QMessageBox::question(this, QStringLiteral("确认快速截取"),
            QStringLiteral("将截取 %1 至 %2。起点按关键帧调整，末尾可能有编码包边界误差。继续导出？")
            .arg(VideoEditor::timecode(qint64(plan.value("start_ms").toDouble())), VideoEditor::timecode(qint64(plan.value("end_ms").toDouble())))) != QMessageBox::Yes) return;
    }
    emit actionRequested(action, input());
}
void ToolPanel::addPaths(const QStringList &paths) {
    if (videoEditor) {
        if (!busy && paths.size()==1) videoEditor->setSource(paths.first());
        else setStatusText(QStringLiteral("每个视频会话接收一个文件"));
        return;
    }
    if (!fileTable || busy || scanning || paths.isEmpty())
        return;
    const int maximum = tool.limits.value("max_files").toInt(1000);
    if (fileTable->rowCount() + paths.size() > maximum) {
        setStatusText(QStringLiteral("文件超过 %1 个，未接收本次添加").arg(maximum));
        return;
    }
    scanning = true;
    updateActions();
    auto *watcher = new QFutureWatcher<QJsonArray>(this);
    connect(watcher, &QFutureWatcher<QJsonArray>::finished, this, [this, watcher] {
        const auto records = watcher->result();
        watcher->deleteLater();
        appendRecords(records);
        scanning = false;
        inputChanged();
        setStatusText(QStringLiteral("已接收 %1 个普通文件").arg(records.size()));
        emit ready();
    });
    watcher->setFuture(QtConcurrent::run([paths, extensions = spec.extensions] {
        QJsonArray records;
        QSet<QString> seen;
        for (const auto &path : paths) {
            const QFileInfo file(path);
            const auto canonical = file.canonicalFilePath().toCaseFolded();
            if (!file.isFile() ||
                (!extensions.isEmpty() && !extensions.contains(file.suffix().toLower())) ||
                seen.contains(canonical))
                continue;
            seen.insert(canonical);
            records.append(QJsonObject{{"path", file.absoluteFilePath()},
                                       {"size", static_cast<double>(file.size())},
                                       {"checked", true}});
        }
        return records;
    }));
}
void ToolPanel::setResult(const QJsonObject &value) {
    lastResult = value;
    if (spec.version < 3 || value.contains("context")) context = value.value("context").toObject();
    if (spec.version >= 3) refreshOptions();
    if (videoEditor) {
        if (context.contains("media")) {
            const auto metadata=context.value("media").toObject();
            if (!videoEditor->setMedia(metadata)) context = {};
            else if (!pendingMediaSelection.isEmpty()) {
                if (pendingMediaSelection.value("identity").toString().isEmpty() ||
                    pendingMediaSelection.value("identity")==metadata.value("identity"))
                    videoEditor->restoreSelection(pendingMediaSelection);
                pendingMediaSelection = {};
            }
        }
        if (context.contains("plan")) videoEditor->showPlan(context.value("plan").toObject());
    }
    conditions();
    updateActions();
    if (textResult) {
        const auto text = value.value("text").toString();
        textResult->setPlainText(text.left(PreviewCharacters));
        textResult->parentWidget()->setVisible(value.contains("text"));
        if (auto *caption = findChild<QLabel *>("resultCaption"))
            caption->setText(text.size() > PreviewCharacters
                                 ? QStringLiteral("结果 · 前 32K 字符预览，复制与导出包含全部内容")
                                 : QStringLiteral("结果"));
    }
    if (resultTable) {
        const auto columns = value.value("columns").toArray(), rows = value.value("rows").toArray();
        resultTable->setUpdatesEnabled(false);
        resultTable->clear();
        resultTable->setRowCount(rows.size());
        resultTable->setColumnCount(columns.size());
        for (int column = 0; column < columns.size(); ++column)
            resultTable->setHorizontalHeaderItem(
                column, new QTableWidgetItem(columns[column].toObject().value("label").toString()));
        for (int row = 0; row < rows.size(); ++row)
            for (int column = 0; column < columns.size(); ++column) {
                const auto cell = rows[row]
                                      .toObject()
                                      .value(columns[column].toObject().value("key").toString())
                                      .toVariant()
                                      .toString();
                auto *item = new QTableWidgetItem(cell);
                item->setToolTip(cell);
                resultTable->setItem(row, column, item);
            }
        resultTable->setUpdatesEnabled(true);
        resultTable->parentWidget()->setVisible(!columns.isEmpty());
    }
    if (preview) {
        const auto path = value.value("preview_path").toString();
        QImageReader reader(path, "png");
        const auto size = reader.size();
        const bool bounded = size.isValid() && size.width() <= 512 && size.height() <= 512 &&
                             QFileInfo(path).size() <= 1024 * 1024;
        if (bounded)
            preview->setPixmap(QPixmap::fromImage(reader.read()));
        preview->setVisible(bounded);
    }
    if (!restoring && bodyScroll &&
        (!qApp->property("modynRevealResults").isValid() ||
         qApp->property("modynRevealResults").toBool())) {
        QWidget *resultFrame = resultTable && !value.value("columns").toArray().isEmpty()
                                   ? resultTable->parentWidget()
                               : textResult && value.contains("text") ? textResult->parentWidget()
                                                                      : nullptr;
        if (resultFrame)
            QTimer::singleShot(0, this, [this, resultFrame] {
                bodyScroll->ensureWidgetVisible(resultFrame, 0, 10);
            });
    }
    if (status)
        setStatusText(value.value("message").toString());
    if (textInput && value.contains("line")) {
        auto cursor = textInput->textCursor();
        cursor.movePosition(QTextCursor::Start);
        cursor.movePosition(QTextCursor::Down, QTextCursor::MoveAnchor,
                            qMax(0, value.value("line").toInt() - 1));
        textInput->setTextCursor(cursor);
    }
}
QString ToolPanel::resultText() const {
    return lastResult.value("text").toString();
}
void ToolPanel::setJobState(const QString &state, const QString &message, int done, int total,
                            bool cancellable) {
    const bool wasBusy = busy;
    busy = QStringList{"preparing", "queued", "running"}.contains(state);
    if (busy && !wasBusy) { jobClock.start(); if (elapsedLabel) elapsedLabel->clear(); }
    for (auto *input : frozenInputs)
        input->setEnabled(!busy);
    if (textInput)
        textInput->setBusy(busy);
    if (videoEditor) videoEditor->setBusy(busy);
    if (status) {
        static const QMap<QString, QString> labels{
            {"preparing", QStringLiteral("准备中")},
            {"queued", QStringLiteral("等待执行")},
            {"running", QStringLiteral("正在执行")},
            {"succeeded", QStringLiteral("处理完成")},
            {"failed", QStringLiteral("处理失败，请查看运行详情")},
            {"cancelled", QStringLiteral("已取消")},
            {"interrupted", QStringLiteral("运行中断")}};
        setStatusText(message.isEmpty() ? labels.value(state, state) : message);
    }
    if (progress) {
        progress->setVisible(busy);
        progress->setRange(0, total > 0 ? total : 0);
        progress->setValue(done);
    }
    if (cancelButton) {
        cancelButton->setVisible(busy);
        cancelButton->setEnabled(cancellable);
    }
    updateActions();
}
bool ToolPanel::hasVolatileInput() const {
    return (videoEditor && !videoEditor->source().isEmpty()) ||
           (textInput && !textInput->content().isEmpty()) ||
           (fileTable && fileTable->rowCount() > 0);
}
void ToolPanel::loadText() {
    const auto path = QFileDialog::getOpenFileName(this, QStringLiteral("载入 UTF-8 文件"));
    if (path.isEmpty())
        return;
    scanning = true;
    updateActions();
    textInput->setBusy(true);
    for (auto *control : frozenInputs)
        control->setEnabled(false);
    auto *watcher = new QFutureWatcher<QPair<QString, QString>>(this);
    connect(watcher, &QFutureWatcher<QPair<QString, QString>>::finished, this, [this, watcher] {
        const auto result = watcher->result();
        watcher->deleteLater();
        if (result.second.isEmpty())
            textInput->setContent(result.first);
        else
            setStatusText(result.second);
        scanning = false;
        textInput->setBusy(false);
        for (auto *control : frozenInputs)
            control->setEnabled(true);
        conditions();
        updateActions();
        emit ready();
    });
    watcher->setFuture(QtConcurrent::run(
        [path, limit = tool.limits.value("max_input_bytes").toInt(5 * 1024 * 1024)] {
            QFile file(path);
            if (!file.open(QIODevice::ReadOnly))
                return qMakePair(QString(), file.errorString());
            const auto bytes = file.read(limit + 1);
            if (bytes.size() > limit)
                return qMakePair(QString(), QStringLiteral("文件超过输入容量限制"));
            if (file.error() != QFile::NoError)
                return qMakePair(QString(), file.errorString());
            QStringDecoder decoder(QStringDecoder::Utf8);
            const QString text = decoder(bytes);
            return qMakePair(text,
                             decoder.hasError() ? QStringLiteral("文件不是有效 UTF-8") : QString());
        }));
}
void ToolPanel::exportText() {
    const auto path = QFileDialog::getSaveFileName(this, QStringLiteral("导出结果"),
                                                   QStringLiteral("result.txt"));
    if (path.isEmpty())
        return;
    writeExport(path, {{"text", resultText()}}, true);
}

QPlainTextEdit *ToolPanel::editor() const {
    return textInput;
}

void ToolPanel::writeExport(const QString &path, const QJsonObject &document, bool plainText) {
    emit exportRequested(path, document, plainText);
}

void ToolPanel::setStatusText(const QString &text) {
    if (status) {
        status->setText(text);
        status->setToolTip(text);
    }
}

void ToolPanel::verifyRestoredFiles() {
    if (!fileTable || fileTable->rowCount() == 0)
        return;
    QStringList paths;
    for (int row = 0; row < fileTable->rowCount(); ++row)
        paths.append(fileTable->item(row, 0)->data(Qt::UserRole).toString());
    scanning = true;
    updateActions();
    auto *watcher = new QFutureWatcher<QMap<QString, qint64>>(this);
    connect(watcher, &QFutureWatcher<QMap<QString, qint64>>::finished, this, [this, watcher] {
        const auto metadata = watcher->result();
        watcher->deleteLater();
        int count = 0;
        const QSignalBlocker blocker(fileTable);
        const auto warning = style()->standardIcon(QStyle::SP_MessageBoxWarning);
        fileTable->setUpdatesEnabled(false);
        for (int row = 0; row < fileTable->rowCount(); ++row) {
            auto *item = fileTable->item(row, 0);
            const auto path = item->data(Qt::UserRole).toString();
            const auto size = metadata.value(path, -1);
            if (size < 0) {
                ++count;
                item->setIcon(warning);
                item->setToolTip(path + QStringLiteral("\n文件缺失，请移除后重新添加新位置"));
            } else {
                item->setIcon({});
                item->setToolTip(path);
                fileTable->item(row, 1)->setText(
                    QStringLiteral("%1 KB").arg(size / 1024.0, 0, 'f', 1));
                fileTable->item(row, 1)->setData(Qt::UserRole, size);
            }
        }
        fileTable->setUpdatesEnabled(true);
        if (fileNotice)
            fileNotice->setText(
                QStringLiteral("%1 个文件%2")
                    .arg(fileTable->rowCount())
                    .arg(count ? QStringLiteral(" · %1 个缺失，请移除后重新添加").arg(count)
                               : QString()));
        if (count)
            context = {};
        scanning = false;
        updateActions();
        emit ready();
    });
    watcher->setFuture(QtConcurrent::run([paths] {
        QMap<QString, qint64> metadata;
        for (const auto &path : paths) {
            const QFileInfo file(path);
            metadata[path] = file.isFile() ? file.size() : -1;
        }
        return metadata;
    }));
}
