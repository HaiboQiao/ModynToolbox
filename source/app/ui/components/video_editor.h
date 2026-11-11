#pragma once
#include <QJsonObject>
#include <QWidget>
class QAudioOutput;
class QMediaPlayer;
class VideoSurface;
class QLabel;
class QLineEdit;
class QPushButton;
class QSlider;
class QComboBox;
class QCheckBox;
class RangeTimeline : public QWidget {
    Q_OBJECT
  public:
    explicit RangeTimeline(QWidget *parent = nullptr);
    void setValues(qint64 duration, qint64 start, qint64 end, qint64 position);
  signals:
    void rangeEdited(qint64 start, qint64 end);
    void seekRequested(qint64 position);

  protected:
    bool event(QEvent *) override;
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void keyPressEvent(QKeyEvent *) override;

  private:
    qint64 at(int x) const;
    int x(qint64 t) const;
    void moveHandle(qint64 t);
    qint64 length = 0, first = 0, last = 0, playhead = 0;
    int active = 0;
    bool dragging = false;
};
class VideoEditor : public QWidget {
    Q_OBJECT
  public:
    explicit VideoEditor(const QStringList &extensions, QWidget *parent = nullptr);
    ~VideoEditor() override;
    QString source() const { return path; }
    void setSource(const QString &value);
    bool setMedia(const QJsonObject &media);
    QJsonObject selection() const;
    QJsonObject snapshot() const;
    void restoreSelection(const QJsonObject &state);
    void setBusy(bool busy);
    void pause();
    void showPlan(const QJsonObject &plan);
    static QString timecode(qint64 ms);
  signals:
    void sourceChanged();
    void probeRequested();
    void selectionChanged();
    void notice(const QString &message);

  protected:
    void hideEvent(QHideEvent *) override;
    void dragEnterEvent(QDragEnterEvent *) override;
    void dropEvent(QDropEvent *) override;

  private:
    void choose(const QString &name);
    void playerReady();
    void range(qint64 start, qint64 end, bool notify = true);
    void updateTimes();
    bool eventFilter(QObject *, QEvent *) override;
    QString path, filter, mediaIdentity;
    qint64 duration = 0, start = 0, end = 0, position = 0;
    int audioIndex = -1;
    bool locked = false, loaded = false;
    QList<QWidget *> rangeControls;
    QMediaPlayer *player = nullptr;
    QAudioOutput *audio = nullptr;
    VideoSurface *video;
    RangeTimeline *timeline;
    QLabel *fileName, *details, *clock, *length, *message, *placeholder;
    QLineEdit *beginEdit, *endEdit;
    QPushButton *chooseButton, *play, *reprobe;
    QComboBox *tracks;
    QCheckBox *loop;
};
