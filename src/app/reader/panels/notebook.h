#pragma once
#include "storage/notebook_store.h"
#include <QHash>
#include <QImage>
#include <QPointF>
#include <QPointer>
#include <QRect>
#include <QString>
#include <QTimer>
#include <QVector>
#include <QWidget>
#include <functional>
#include <memory>

namespace kobalt {
namespace storage {
class RecordWriter;
}

// A handwritten notebook for one book, drawn with the stylus or a finger.
// Each page is a PNG in the book's notebook folder, page-001.png and on.
// links.json maps a highlight ID to the page that belongs to it, so a tap on
// that highlight opens its page again.
//
// The notebook covers the whole reader window. Strokes are painted into an
// image as the pen moves, and only the changed rectangle is updated, so the
// e-ink refresh stays small. Saving runs on the shared writer thread.
//
// Where the host has its own pen layer (platform::nativeInk), that layer
// draws and erases the strokes instead, over the paper this widget paints.
// The whole notebook is then one file, `folder`.notebook, with a page's
// strokes as SVG; see storage::notebook::Book.
class Notebook final : public QWidget
{
public:
    // `folder` is the book's notebook folder. `highlight` is the highlight
    // whose page opens; empty opens the first page. `excerpt` is shown above
    // that page. `fastRefresh` switches the reader's fast pen waveform on and off.
    Notebook(QWidget *parent, const QString &folder, const QString &highlight,
             const QString &excerpt, std::function<void(bool)> fastRefresh);
    ~Notebook() override;

    // Highlight ID to page number, read from `folder`/links.json. Empty when
    // the file is missing or unreadable.
    static QHash<QString, int> readLinks(const QString &folder);

    // The links as they are now, including a page linked in this session.
    const QHash<QString, int> &links() const { return links_; }

    // A page button: 1 for the next page, -1 for the previous one.
    void turnPage(int direction) { goToPage(page_ + (direction > 0 ? 1 : -1)); }

    // Saves the open page and runs `closed`. The owner destroys the notebook.
    void finish();
    std::function<void()> closed;

protected:
    bool eventFilter(QObject *object, QEvent *event) override;
    bool event(QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    enum class Tool { Pen, Eraser };
    enum class Button { Done, Previous, Next, Tool, Undo, Clear, None };

    struct Stroke
    {
        QVector<QPointF> points;
        QVector<qreal> widths;
        bool eraser = false;
        // Clear empties the page; it is a stroke so that Undo brings it back.
        bool clear = false;
    };

    void press(QPointF point, qreal pressure);
    void move(QPointF point, qreal pressure);
    void release(QPointF point);
    void cancelStroke();
    // Draws one smoothed piece of the current stroke into `ink` and returns
    // the canvas rectangle it touched.
    QRect drawSegment(QImage &ink, const Stroke &stroke, int index, QPointF *lastMid,
                      QPointF *smoothed) const;
    void replay();
    void loadPage(int page);
    void savePage();
    void saveLinks();
    void goToPage(int page);
    void run(Button button);
    Button buttonAt(QPoint point) const;
    QRect buttonRect(Button button) const;
    QString buttonLabel(Button button) const;
    QRect toolbarRect() const;
    QRect excerptRect() const;
    QRect canvasRect() const;
    qreal widthFor(qreal pressure) const;
    void endFastRefreshLater();
    void placeInkLayer();

    QString folder_, highlight_, excerpt_;
    QHash<QString, int> links_;
    int page_ = 1, pages_ = 0, excerptPage_ = 0;
    // The saved page, and the page with every stroke of this visit on it.
    QImage base_, ink_;
    QVector<Stroke> strokes_;
    Stroke current_;
    QPointF currentMid_, currentSmoothed_;
    bool drawing_ = false, dirty_ = false, fast_ = false;
    int touchId_ = -1;
    Button pressed_ = Button::None;
    Tool tool_ = Tool::Pen;
    std::function<void(bool)> fastRefresh_;
    QTimer fastEnd_, autosave_;
    // The host's pen layer, and the widget under it that takes the touches
    // the layer passes on: its one child swallows them, so a finger or a palm
    // on the paper neither draws nor reaches this widget.
    QPointer<QWidget> inkLayer_;
    QWidget *pad_ = nullptr, *padInner_ = nullptr;
    storage::notebook::Book book_;
    bool native_ = false;
};
} // namespace kobalt
