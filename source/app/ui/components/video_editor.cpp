#include "video_editor.h"
#include "combo_box.h"
#include "ui/theme/theme.h"
#include "video_surface.h"
#include <QAudioOutput>
#include <QCheckBox>
#include <QDateTime>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMediaPlayer>
#include <QMessageBox>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QRegularExpressionValidator>
#include <QSignalBlocker>
#include <QSlider>
#include <QStackedLayout>
#include <QVBoxLayout>
#include <QVideoFrame>
#include <QVideoSink>
#include <cmath>
RangeTimeline::RangeTimeline(QWidget *p) : QWidget(p) {
    setObjectName("videoTimeline");
    setMinimumHeight(48);
    setMinimumWidth(160);
    setFocusPolicy(Qt::StrongFocus);
    setAccessibleName(QStringLiteral("视频选段时间轴"));
    setToolTip(QStringLiteral(
        "拖动左右端点选段；点击轨道定位。方向键调整端点，Tab选择另一端点，Shift加大步长。"));
}
int RangeTimeline::x(qint64 t) const {
    return 12 + int(length ? double(t) * (width() - 24) / length : 0);
}
qint64 RangeTimeline::at(int px) const {
    return qBound(0LL, qRound64(double(px - 12) * length / qMax(1, width() - 24)), length);
}
void RangeTimeline::setValues(qint64 d, qint64 a, qint64 b, qint64 p) {
    length = d;
    first = a;
    last = b;
    playhead = p;
    update();
}
bool RangeTimeline::event(QEvent *event) {
    if (event->type() == QEvent::KeyPress) {
        auto *key = static_cast<QKeyEvent *>(event);
        if (key->key() == Qt::Key_Tab && !(key->modifiers() & Qt::ShiftModifier) && active == 0) {
            active = 1;
            update();
            return true;
        }
        if (key->key() == Qt::Key_Backtab && active == 1) {
            active = 0;
            update();
            return true;
        }
        if (key->key() == Qt::Key_Tab || key->key() == Qt::Key_Backtab)
            active = 0;
    }
    return QWidget::event(event);
}
void RangeTimeline::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(Theme::border);
    p.drawRoundedRect(QRectF(12, 15, width() - 24, 18), 5, 5);
    if (!length)
        return;
    p.setBrush(Theme::selection);
    p.drawRoundedRect(QRectF(x(first), 15, qMax(1, x(last) - x(first)), 18), 4, 4);
    p.setPen(QPen(Theme::accent, 2));
    p.drawLine(x(playhead), 8, x(playhead), 40);
    for (int i = 0; i < 2; ++i) {
        auto px = x(i ? last : first);
        p.setPen(QPen(Theme::accent, hasFocus() && active == i ? 3 : 1));
        p.setBrush(Theme::panel);
        p.drawRoundedRect(QRectF(px - 6, 10, 12, 28), 4, 4);
        p.setPen(QPen(Theme::accent, 2));
        p.drawLine(px, 18, px, 29);
    }
}
void RangeTimeline::moveHandle(qint64 t) {
    if (active == 0)
        first = qBound(0LL, t, qMax(0LL, last - 1));
    else
        last = qBound(first + 1, t, length);
    emit rangeEdited(first, last);
    update();
}
void RangeTimeline::mousePressEvent(QMouseEvent *e) {
    if (!length || e->button() != Qt::LeftButton)
        return;
    setFocus();
    const auto px = e->position().x();
    if (qAbs(px - x(first)) <= 12 || qAbs(px - x(last)) <= 12) {
        active = qAbs(px - x(first)) <= qAbs(px - x(last)) ? 0 : 1;
        dragging = true;
        moveHandle(at(int(px)));
    } else
        emit seekRequested(at(int(px)));
}
void RangeTimeline::mouseMoveEvent(QMouseEvent *e) {
    if (dragging)
        moveHandle(at(int(e->position().x())));
}
void RangeTimeline::mouseReleaseEvent(QMouseEvent *) { dragging = false; }
void RangeTimeline::keyPressEvent(QKeyEvent *e) {
    if (e->key() == Qt::Key_Tab && !(e->modifiers() & Qt::ShiftModifier)) {
        active = 1 - active;
        update();
        e->accept();
        return;
    }
    if (e->key() == Qt::Key_Left || e->key() == Qt::Key_Right) {
        auto step = e->modifiers() & Qt::ShiftModifier ? 1000 : 100;
        moveHandle((active ? last : first) + (e->key() == Qt::Key_Left ? -step : step));
        e->accept();
        return;
    }
    QWidget::keyPressEvent(e);
}
QString VideoEditor::timecode(qint64 ms) {
    ms = qMax(0LL, ms);
    return QString("%1:%2:%3.%4")
        .arg(ms / 3600000, 2, 10, QChar('0'))
        .arg(ms / 60000 % 60, 2, 10, QChar('0'))
        .arg(ms / 1000 % 60, 2, 10, QChar('0'))
        .arg(ms % 1000, 3, 10, QChar('0'));
}
VideoEditor::VideoEditor(const QStringList &extensions, QWidget *p) : QWidget(p) {
    setObjectName("videoEditor");
    setAcceptDrops(true);
    setFocusPolicy(Qt::StrongFocus);
    installEventFilter(this);
    QStringList globs;
    for (const auto &e : extensions)
        globs << "*." + e;
    filter = QStringLiteral("视频文件 (%1);;所有文件 (*)").arg(globs.join(' '));
    auto *box = new QVBoxLayout(this);
    box->setContentsMargins(0, 0, 0, 0);
    box->setSpacing(12);
    auto *header = new QHBoxLayout;
    fileName = new QLabel(QStringLiteral("拖入视频，选取需要的片段"));
    fileName->setObjectName("sectionTitle");
    fileName->setWordWrap(true);
    fileName->setMinimumWidth(0);
    chooseButton = new QPushButton(QStringLiteral("选择视频"));
    chooseButton->setObjectName("chooseVideo");
    reprobe = new QPushButton(QStringLiteral("重新分析"));
    reprobe->setEnabled(false);
    header->addWidget(fileName, 1);
    header->addWidget(reprobe);
    header->addWidget(chooseButton);
    box->addLayout(header);
    connect(reprobe, &QPushButton::clicked, this, &VideoEditor::probeRequested);
    details = new QLabel(QStringLiteral("本地处理 · 原视频只读 · 导出为新文件"));
    details->setObjectName("secondary");
    details->setWordWrap(true);
    box->addWidget(details);
    video = new VideoSurface;
    video->setObjectName("videoSurface");
    video->setMinimumSize(180, 200);
    video->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    auto videoPalette = video->palette();
    videoPalette.setColor(QPalette::Window, Theme::videoBackground);
    video->setPalette(videoPalette);
    video->setAutoFillBackground(true);

    video->installEventFilter(this);
    auto *previewFrame = new QWidget;
    previewFrame->setObjectName("videoPreviewFrame");
    auto *layers = new QStackedLayout(previewFrame);
    layers->setContentsMargins(0, 0, 0, 0);
    layers->setStackingMode(QStackedLayout::StackAll);
    layers->addWidget(video);
    placeholder = new QLabel(QStringLiteral("拖入或选择本地视频"));
    placeholder->setObjectName("videoPlaceholder");
    placeholder->setAlignment(Qt::AlignCenter);
    placeholder->setWordWrap(true);
    layers->addWidget(placeholder);
    layers->setCurrentWidget(placeholder);
    box->addWidget(previewFrame, 1);
    connect(video->videoSink(), &QVideoSink::videoFrameChanged, this,
            [this](const QVideoFrame &frame) {
                if (frame.isValid())
                    placeholder->hide();
            });
    auto *transport = new QHBoxLayout;
    play = new QPushButton(QStringLiteral("播放"));
    play->setEnabled(false);
    play->setObjectName("videoPlay");
    transport->addWidget(play);
    clock = new QLabel(timecode(0) + " / " + timecode(0));
    clock->setObjectName("secondary");
    transport->addWidget(clock, 1);
    auto *mute = new QCheckBox(QStringLiteral("静音"));
    transport->addWidget(mute);
    auto *volume = new QSlider(Qt::Horizontal);
    volume->setRange(0, 100);
    volume->setValue(70);
    volume->setMaximumWidth(85);
    volume->setAccessibleName(QStringLiteral("预览音量"));
    transport->addWidget(volume);
    box->addLayout(transport);
    timeline = new RangeTimeline;
    box->addWidget(timeline);
    auto *times = new QGridLayout;
    times->addWidget(new QLabel(QStringLiteral("起点")), 0, 0);
    times->addWidget(new QLabel(QStringLiteral("终点")), 0, 1);
    beginEdit = new QLineEdit(timecode(0));
    endEdit = new QLineEdit(timecode(0));
    beginEdit->setObjectName("videoStart");
    endEdit->setObjectName("videoEnd");
    for (auto *edit : {beginEdit, endEdit}) {
        edit->setMinimumWidth(0);
        edit->setPlaceholderText("00:00:00.000");
        edit->setValidator(new QRegularExpressionValidator(
            QRegularExpression("[0-9]{1,3}:[0-5][0-9]:[0-5][0-9]\\.[0-9]{3}"), edit));
    }
    beginEdit->installEventFilter(this);
    endEdit->installEventFilter(this);
    beginEdit->setAccessibleName(QStringLiteral("选段起点 时分秒毫秒"));
    endEdit->setAccessibleName(QStringLiteral("选段终点 时分秒毫秒"));
    times->addWidget(beginEdit, 1, 0);
    times->addWidget(endEdit, 1, 1);
    auto *first = new QPushButton(QStringLiteral("设为起点"));
    auto *last = new QPushButton(QStringLiteral("设为终点"));
    times->addWidget(first, 2, 0);
    times->addWidget(last, 2, 1);
    box->addLayout(times);
    auto *selectionRow = new QHBoxLayout;
    length = new QLabel;
    length->setObjectName("secondary");
    length->setWordWrap(true);
    selectionRow->addWidget(length, 1);
    auto *reset = new QPushButton(QStringLiteral("恢复整段"));
    selectionRow->addWidget(reset);
    box->addLayout(selectionRow);
    auto *audioRow = new QHBoxLayout;
    loop = new QCheckBox(QStringLiteral("循环预览选段"));
    loop->setObjectName("videoLoop");
    audioRow->addWidget(loop);
    tracks = new UiComboBox;
    tracks->setObjectName("videoAudioTrack");
    tracks->setAccessibleName(QStringLiteral("输出音轨"));
    audioRow->addWidget(tracks, 1);
    box->addLayout(audioRow);
    message = new QLabel;
    message->setWordWrap(true);
    message->setObjectName("secondary");
    box->addWidget(message);
    connect(chooseButton, &QPushButton::clicked, this, [this] {
        auto n = QFileDialog::getOpenFileName(this, QStringLiteral("选择视频"), {}, filter);
        if (!n.isEmpty())
            choose(n);
    });
    connect(play, &QPushButton::clicked, this, [this] {
        playerReady();
        if (!player || !loaded)
            return;
        if (player->isPlaying())
            player->pause();
        else {
            if (position < start || position >= end)
                player->setPosition(start);
            player->play();
        }
    });
    connect(mute, &QCheckBox::toggled, this, [this](bool on) {
        playerReady();
        if (audio)
            audio->setMuted(on);
    });
    connect(volume, &QSlider::valueChanged, this, [this](int n) {
        playerReady();
        if (audio)
            audio->setVolume(n / 100.0);
    });
    connect(timeline, &RangeTimeline::rangeEdited, this, [this](qint64 a, qint64 b) {
        if (!locked)
            range(a, b);
    });
    connect(timeline, &RangeTimeline::seekRequested, this, [this](qint64 t) {
        if (player)
            player->setPosition(t);
    });
    connect(first, &QPushButton::clicked, this, [this] { range(position, end); });
    connect(last, &QPushButton::clicked, this, [this] { range(start, position); });
    connect(reset, &QPushButton::clicked, this, [this] { range(0, duration); });
    auto readTime = [](QLineEdit *e) -> qint64 {
        auto n = e->text().split(QRegularExpression("[:.]")).toVector();
        if (n.size() != 4 || !e->hasAcceptableInput())
            return -1;
        return n[0].toLongLong() * 3600000 + n[1].toLongLong() * 60000 + n[2].toLongLong() * 1000 +
               n[3].toLongLong();
    };
    connect(beginEdit, &QLineEdit::editingFinished, this,
            [this, readTime] { range(readTime(beginEdit), end); });
    connect(endEdit, &QLineEdit::editingFinished, this,
            [this, readTime] { range(start, readTime(endEdit)); });
    connect(tracks, &QComboBox::currentIndexChanged, this, [this] {
        audioIndex = tracks->currentData().toInt();
        emit selectionChanged();
    });
    rangeControls = {first, last, reset};
    setBusy(false);
    updateTimes();
}
VideoEditor::~VideoEditor() {
    // Stop the decoder while its QWidget video output and placeholder still exist.
    // QObject child deletion order otherwise removes the view before the late-created player.
    disconnect(video->videoSink(), nullptr, this, nullptr);
    if (player) {
        disconnect(player, nullptr, this, nullptr);
        player->stop();
        player->setVideoOutput(nullptr);
        player->setAudioOutput(nullptr);
        player->setSource({});
        delete player;
        player = nullptr;
    }
}
void VideoEditor::playerReady() {
    if (player || path.isEmpty() || !loaded)
        return;
    audio = new QAudioOutput(this);
    audio->setVolume(.7);
    player = new QMediaPlayer(this);
    player->setAudioOutput(audio);
    player->setVideoOutput(video->videoOutput());
    connect(player, &QMediaPlayer::positionChanged, this, [this](qint64 p) {
        position = p;
        updateTimes();
        if (player->isPlaying() && p >= end) {
            if (loop->isChecked())
                player->setPosition(start);
            else
                player->pause();
        }
    });
    connect(player, &QMediaPlayer::playbackStateChanged, this, [this] {
        play->setText(player->isPlaying() ? QStringLiteral("暂停") : QStringLiteral("播放"));
    });
    connect(player, &QMediaPlayer::errorOccurred, this,
            [this](QMediaPlayer::Error, const QString &e) {
                message->setText(QStringLiteral("无法预览，可使用时间输入导出 · ") + e);
            });
    connect(player, &QMediaPlayer::mediaStatusChanged, this,
            [this](QMediaPlayer::MediaStatus state) {
                if (state == QMediaPlayer::EndOfMedia && loop->isChecked() && isVisible()) {
                    player->setPosition(start);
                    player->play();
                }
            });
    player->setSource(QUrl::fromLocalFile(path));
}
void VideoEditor::choose(const QString &name) {
    if (locked)
        return;
    if (!path.isEmpty() && QFileInfo(name).absoluteFilePath() != path &&
        QMessageBox::question(this, QStringLiteral("替换视频"),
                              QStringLiteral("替换当前视频并重置选段？已导出的文件会保留。")) !=
            QMessageBox::Yes)
        return;
    setSource(name);
}
void VideoEditor::setSource(const QString &value) {
    auto name = value.isEmpty() ? QString() : QFileInfo(value).absoluteFilePath();
    if (name.isEmpty() && path.isEmpty())
        return;
    pause();
    if (player)
        player->setSource({});
    placeholder->setText(QStringLiteral("点击下方播放，预览视频"));
    placeholder->show();
    placeholder->raise();
    path = name;
    duration = start = end = position = 0;
    loaded = false;
    mediaIdentity.clear();
    fileName->setText(path.isEmpty() ? QStringLiteral("拖入视频，选取需要的片段")
                                     : QFileInfo(path).fileName());
    fileName->setToolTip(path);
    details->setText(QStringLiteral("正在等待媒体分析"));
    message->clear();
    play->setEnabled(false);
    reprobe->setEnabled(!locked && !path.isEmpty());
    tracks->clear();
    setBusy(locked);
    updateTimes();
    emit sourceChanged();
}
bool VideoEditor::setMedia(const QJsonObject &m) {
    const auto d = m.value("duration_ms").toDouble();
    const QFileInfo sourceInfo(path);
    if (QFileInfo(m.value("source").toString()).absoluteFilePath() != path ||
        !sourceInfo.isFile() || m.value("size_bytes").toDouble() != double(sourceInfo.size()) ||
        m.value("modified_ms").toDouble() !=
            double(sourceInfo.lastModified().toMSecsSinceEpoch()) ||
        !std::isfinite(d) || d <= 0 || d > 604800000)
        return false;
    bool fresh = !loaded || mediaIdentity != m.value("identity").toString();
    mediaIdentity = m.value("identity").toString();
    duration = qint64(d);
    loaded = true;
    {
        QSignalBlocker b(tracks);
        tracks->clear();
        for (const auto &t : m.value("audio_tracks").toArray()) {
            auto o = t.toObject();
            tracks->addItem(QStringLiteral("音轨 %1 · %2")
                                .arg(o.value("index").toInt())
                                .arg(o.value("codec").toString()),
                            o.value("index").toInt());
        }
        if (!tracks->count())
            tracks->addItem(QStringLiteral("无音轨"), -1);
        auto i = tracks->findData(audioIndex);
        tracks->setCurrentIndex(i < 0 ? 0 : i);
        audioIndex = tracks->currentData().toInt();
    }
    details->setText(QStringLiteral("%1 · %2 × %3 · %4 MiB · %5")
                         .arg(timecode(duration))
                         .arg(m.value("width").toInt())
                         .arg(m.value("height").toInt())
                         .arg(m.value("size_bytes").toDouble() / 1048576, 0, 'f', 1)
                         .arg(m.value("codec").toString()));
    message->setText(m.value("warning").toString());
    if (fresh || end > duration || end <= start) {
        start = 0;
        end = duration;
    }
    range(start, end, false);
    play->setEnabled(true);
    setBusy(locked);
    playerReady();
    if (player && player->source().isEmpty())
        player->setSource(QUrl::fromLocalFile(path));
    return true;
}
void VideoEditor::range(qint64 a, qint64 b, bool notify) {
    if ((locked && notify) || !loaded || a < 0 || a >= b || b > duration) {
        updateTimes();
        if (notify)
            emit notice(QStringLiteral("起点应早于终点，时间不能超过视频时长"));
        return;
    }
    if (notify)
        pause();
    bool changed = a != start || b != end;
    start = a;
    end = b;
    updateTimes();
    if (notify && changed)
        emit selectionChanged();
}
void VideoEditor::updateTimes() {
    beginEdit->setText(timecode(start));
    endEdit->setText(timecode(end));
    clock->setText(timecode(position) + " / " + timecode(duration));
    length->setText(QStringLiteral("选段 %1").arg(timecode(end - start)));
    timeline->setValues(duration, start, end, position);
}
QJsonObject VideoEditor::selection() const {
    return {{"start_ms", double(start)}, {"end_ms", double(end)}, {"audio_index", audioIndex}};
}
QJsonObject VideoEditor::snapshot() const {
    auto s = selection();
    s["identity"] = mediaIdentity;
    s["source"] = path;
    s["position_ms"] = double(position);
    return s;
}
void VideoEditor::restoreSelection(const QJsonObject &s) {
    if (!loaded)
        return;
    audioIndex = s.value("audio_index").toInt(audioIndex);
    int i = tracks->findData(audioIndex);
    if (i >= 0) {
        QSignalBlocker b(tracks);
        tracks->setCurrentIndex(i);
    }
    range(qint64(s.value("start_ms").toDouble()), qint64(s.value("end_ms").toDouble(duration)),
          false);
    position = qBound(0LL, qint64(s.value("position_ms").toDouble()), duration);
    if (player)
        player->setPosition(position);
    updateTimes();
}
void VideoEditor::setBusy(bool b) {
    locked = b;
    chooseButton->setEnabled(!b);
    reprobe->setEnabled(!b && !path.isEmpty());
    timeline->setEnabled(!b);
    beginEdit->setEnabled(!b && loaded);
    endEdit->setEnabled(!b && loaded);
    tracks->setEnabled(!b && loaded);
    for (auto *control : rangeControls)
        control->setEnabled(!b && loaded);
}
void VideoEditor::pause() {
    if (player)
        player->pause();
}
void VideoEditor::hideEvent(QHideEvent *e) {
    pause();
    QWidget::hideEvent(e);
}
void VideoEditor::showPlan(const QJsonObject &p) {
    if (p.isEmpty())
        return;
    message->setText(p.value("message").toString() +
                     QStringLiteral(" · 实际起点 %1，终点 %2")
                         .arg(timecode(qint64(p.value("start_ms").toDouble())),
                              timecode(qint64(p.value("end_ms").toDouble()))));
}
bool VideoEditor::eventFilter(QObject *o, QEvent *e) {
    if (o == beginEdit || o == endEdit) {
        if (e->type() == QEvent::FocusIn)
            pause();
        return QWidget::eventFilter(o, e);
    }
    if (e->type() == QEvent::KeyPress) {
        auto *k = static_cast<QKeyEvent *>(e);
        if (k->key() == Qt::Key_Space) {
            play->click();
            return true;
        }
        if (!locked && k->key() == Qt::Key_I) {
            range(position, end);
            return true;
        }
        if (!locked && k->key() == Qt::Key_O) {
            range(start, position);
            return true;
        }
    }
    return QWidget::eventFilter(o, e);
}
void VideoEditor::dragEnterEvent(QDragEnterEvent *e) {
    if (!locked && e->mimeData()->hasUrls())
        e->acceptProposedAction();
}
void VideoEditor::dropEvent(QDropEvent *e) {
    auto u = e->mimeData()->urls();
    if (!locked && u.size() == 1 && u.first().isLocalFile())
        choose(u.first().toLocalFile());
    else
        emit notice(QStringLiteral("每个会话请选择一个本地视频"));
}
