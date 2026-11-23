#include "notebook.h"
#include "storage/notebook_store.h"
#include "storage/record_writer.h"
#include "support/strings.h"
#include "ui/input/input.h"
#include "ui/style/fonts.h"
#include "ui/style/scale.h"
#include <QFileInfo>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QTouchEvent>
#include <cmath>

namespace kobalt {
namespace {
// Pen tip widths in pixels at 300 ppi: about 0.3 mm, and 3 mm for the eraser.
constexpr int penWidth = 4, eraserWidth = 36;
// How far each new point pulls the smoothed line, from 0 (never) to 1 (at once).
// Lower is smoother but trails the pen tip more.
constexpr qreal smoothing = 0.55;
// The fast pen waveform stays on this long after the pen lifts, so the next
// stroke of a word does not wait for a cleanup refresh.
constexpr int fastLingerMs = 1500;
// A page is saved this long after the last stroke.
constexpr int autosaveMs = 4000;
constexpr int maxPages = storage::notebook::maxPages;
using storage::notebook::pageFile;

storage::RecordWriter &pageWriter()
{
    return storage::notebook::writer();
}

struct Contact
{
    int id;
    QPointF position;
    bool pressed, released;
    qreal pressure;
};

// The Qt 5 and Qt 6 touch APIs differ; see also ui/input/input.h.
QVector<Contact> contacts(const QTouchEvent *event)
{
    QVector<Contact> result;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    for (const auto &point : event->points())
        result.append({ point.id(), point.position(), point.state() == QEventPoint::State::Pressed,
                        point.state() == QEventPoint::State::Released, point.pressure() });
#else
    for (const auto &point : event->touchPoints())
        result.append({ point.id(), point.pos(), point.state() == Qt::TouchPointPressed,
                        point.state() == Qt::TouchPointReleased, point.pressure() });
#endif
    return result;
}

} // namespace

QHash<QString, int> Notebook::readLinks(const QString &folder)
{
    return storage::notebook::readLinks(folder);
}

Notebook::Notebook(QWidget *parent, const QString &folder, const QString &highlight,
                   const QString &excerpt, std::function<void(bool)> fastRefresh)
    : QWidget(parent),
      folder_(folder),
      highlight_(highlight),
      excerpt_(excerpt.simplified()),
      fastRefresh_(std::move(fastRefresh))
{
    setObjectName("rrNotebook");
    setAttribute(Qt::WA_AcceptTouchEvents);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setFocusPolicy(Qt::StrongFocus);
    links_ = readLinks(folder_);
    pages_ = storage::notebook::lastPage(folder_);
    for (const int page : links_)
        pages_ = qMax(pages_, page);
    int first = 1;
    if (!highlight_.isEmpty()) {
        // A highlight without a page gets a new page after the last one. It
        // is linked when something is drawn on it.
        const int linked = links_.value(highlight_, -1);
        first = linked >= 1 ? linked : qMin(pages_ + 1, maxPages);
    }
    fastEnd_.setSingleShot(true);
    fastEnd_.setInterval(fastLingerMs);
    QObject::connect(&fastEnd_, &QTimer::timeout, this, [this] {
        if (drawing_)
            return;
        fast_ = false;
        if (fastRefresh_)
            fastRefresh_(false);
        update(canvasRect());
    });
    autosave_.setSingleShot(true);
    autosave_.setInterval(autosaveMs);
    QObject::connect(&autosave_, &QTimer::timeout, this, [this] {
        if (!drawing_)
            savePage();
    });
    excerptPage_ = first;
    // Follow the reader window through rotations.
    parent->installEventFilter(this);
    setGeometry(parent->rect());
    page_ = first;
    show();
    raise();
    setFocus(Qt::OtherFocusReason);
    loadPage(first);
}

Notebook::~Notebook()
{
    // Destroyed without Done, as when the book closes: keep the drawing.
    cancelStroke();
    savePage();
    if (fast_ && fastRefresh_)
        fastRefresh_(false);
}

QRect Notebook::toolbarRect() const
{
    return QRect(0, 0, width(), ui::px(104));
}

QRect Notebook::excerptRect() const
{
    if (excerpt_.isEmpty() || page_ != excerptPage_)
        return {};
    return QRect(ui::px(40), toolbarRect().bottom() + ui::px(16), width() - 2 * ui::px(40),
                 ui::px(96));
}

QRect Notebook::canvasRect() const
{
    return QRect(0, toolbarRect().bottom() + 1, width(), height() - toolbarRect().height());
}

QRect Notebook::buttonRect(Button button) const
{
    // Done | ‹ | Page | › | Pen | Undo | Clear, in seven equal cells.
    const int cells = 7;
    const int cell = width() / cells;
    int index = 0;
    switch (button) {
    case Button::Done:
        index = 0;
        break;
    case Button::Previous:
        index = 1;
        break;
    case Button::Next:
        index = 3;
        break;
    case Button::Tool:
        index = 4;
        break;
    case Button::Undo:
        index = 5;
        break;
    case Button::Clear:
        index = 6;
        break;
    case Button::None:
        return {};
    }
    return QRect(index * cell, 0, cell, toolbarRect().height());
}

QString Notebook::buttonLabel(Button button) const
{
    switch (button) {
    case Button::Done:
        return strings::get("notebook.done");
    case Button::Previous:
        return QStringLiteral("‹");
    case Button::Next:
        return QStringLiteral("›");
    case Button::Tool:
        return strings::get(tool_ == Tool::Pen ? "notebook.pen" : "notebook.eraser");
    case Button::Undo:
        return strings::get("notebook.undo");
    case Button::Clear:
        return strings::get("notebook.clear");
    case Button::None:
        break;
    }
    return {};
}

Notebook::Button Notebook::buttonAt(QPoint point) const
{
    for (const auto button : { Button::Done, Button::Previous, Button::Next, Button::Tool,
                               Button::Undo, Button::Clear })
        if (buttonRect(button).contains(point))
            return button;
    return Button::None;
}

qreal Notebook::widthFor(qreal pressure) const
{
    if (tool_ == Tool::Eraser)
        return ui::px(eraserWidth);
    // Touch drivers without pressure report 0 or 1. Real pressure thickens the
    // line from 70% to 150% of the normal tip.
    const qreal base = qMax(2, ui::px(penWidth));
    if (pressure <= 0.0 || pressure >= 1.0)
        return base;
    return base * (0.7 + 0.8 * pressure);
}

void Notebook::loadPage(int page)
{
    page_ = qBound(1, page, maxPages);
    strokes_.clear();
    current_ = Stroke();
    drawing_ = false;
    dirty_ = false;
    const QSize size = canvasRect().size().expandedTo(QSize(1, 1));
    base_ = QImage(size, QImage::Format_ARGB32_Premultiplied);
    base_.fill(Qt::transparent);
    QImage saved;
    if (QFileInfo(pageFile(folder_, page_)).isFile() && saved.load(pageFile(folder_, page_))) {
        QPainter painter(&base_);
        // Pages keep their pixels; a page drawn in another orientation is
        // shown at the top left, scaled down only when it does not fit.
        if (saved.width() > size.width() || saved.height() > size.height())
            saved = saved.scaled(size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        painter.drawImage(0, 0, saved);
    }
    ink_ = base_;
    update();
}

void Notebook::savePage()
{
    autosave_.stop();
    if (!dirty_)
        return;
    dirty_ = false;
    bool empty = true;
    for (int y = 0; y < ink_.height() && empty; ++y) {
        const auto *line = reinterpret_cast<const QRgb *>(ink_.constScanLine(y));
        for (int x = 0; x < ink_.width(); ++x)
            if (qAlpha(line[x])) {
                empty = false;
                break;
            }
    }
    const QString file = pageFile(folder_, page_);
    const QString folder = folder_;
    if (empty && !QFileInfo(file).exists())
        return;
    if (!highlight_.isEmpty() && !links_.contains(highlight_) && !empty) {
        links_.insert(highlight_, page_);
        saveLinks();
    }
    pages_ = qMax(pages_, page_);
    // An empty page stays as a blank file, so later page numbers keep their place.
    const QImage image = ink_;
    const int page = page_;
    pageWriter().post(
            [image, folder, page] { return storage::notebook::writePage(folder, page, image); },
            {});
}

void Notebook::saveLinks()
{
    const auto links = links_;
    const QString folder = folder_;
    pageWriter().post([links, folder] { return storage::notebook::writeLinks(folder, links); }, {});
}

void Notebook::goToPage(int page)
{
    if (page < 1 || page > qMin(pages_ + 1, maxPages) || page == page_)
        return;
    cancelStroke();
    savePage();
    loadPage(page);
}

void Notebook::finish()
{
    cancelStroke();
    savePage();
    if (closed)
        closed();
}

void Notebook::run(Button button)
{
    switch (button) {
    case Button::Done:
        finish();
        return;
    case Button::Previous:
        goToPage(page_ - 1);
        return;
    case Button::Next:
        goToPage(page_ + 1);
        return;
    case Button::Tool:
        tool_ = tool_ == Tool::Pen ? Tool::Eraser : Tool::Pen;
        update(toolbarRect());
        return;
    case Button::Undo:
        if (!strokes_.isEmpty()) {
            strokes_.removeLast();
            dirty_ = true;
            replay();
            update(toolbarRect());
            autosave_.start();
        }
        return;
    case Button::Clear: {
        Stroke clear;
        clear.clear = true;
        strokes_.append(clear);
        dirty_ = true;
        replay();
        update(toolbarRect());
        autosave_.start();
        return;
    }
    case Button::None:
        return;
    }
}

QRect Notebook::drawSegment(QImage &ink, const Stroke &stroke, int index, QPointF *lastMid,
                            QPointF *smoothed) const
{
    const QPointF point = stroke.points[index];
    const qreal width = stroke.widths[index];
    QPainter painter(&ink);
    painter.setRenderHint(QPainter::Antialiasing, true);
    if (stroke.eraser)
        painter.setCompositionMode(QPainter::CompositionMode_Clear);
    QPen pen(stroke.eraser ? QColor(Qt::transparent) : QColor(Qt::black), width, Qt::SolidLine,
             Qt::RoundCap, Qt::RoundJoin);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    QRectF touched;
    if (index == 0) {
        *smoothed = point;
        *lastMid = point;
        // A tap leaves a dot.
        painter.setPen(Qt::NoPen);
        painter.setBrush(stroke.eraser ? QColor(Qt::transparent) : QColor(Qt::black));
        painter.drawEllipse(point, width / 2, width / 2);
        touched = QRectF(point, point);
    } else {
        // Pull the smoothed point toward the pen, then draw a curve from the
        // last midpoint through the previous smoothed point to the new
        // midpoint. Corners become round and the jitter of the digitiser fades.
        const QPointF previous = *smoothed;
        *smoothed = previous + (point - previous) * smoothing;
        const QPointF mid = (previous + *smoothed) / 2;
        QPainterPath path(*lastMid);
        path.quadTo(previous, mid);
        painter.drawPath(path);
        touched = path.boundingRect();
        *lastMid = mid;
    }
    const int margin = int(std::ceil(width)) + 2;
    return touched.toAlignedRect().adjusted(-margin, -margin, margin, margin);
}

void Notebook::replay()
{
    ink_ = base_;
    for (const auto &stroke : strokes_) {
        if (stroke.clear) {
            ink_.fill(Qt::transparent);
            continue;
        }
        QPointF mid, smoothed;
        for (int i = 0; i < stroke.points.size(); ++i)
            drawSegment(ink_, stroke, i, &mid, &smoothed);
    }
    update(canvasRect());
}

void Notebook::press(QPointF point, qreal pressure)
{
    const QPoint at = point.toPoint();
    if (toolbarRect().contains(at)) {
        pressed_ = buttonAt(at);
        return;
    }
    pressed_ = Button::None;
    autosave_.stop();
    fastEnd_.stop();
    if (!fast_) {
        fast_ = true;
        if (fastRefresh_)
            fastRefresh_(true);
    }
    drawing_ = true;
    current_ = Stroke();
    current_.eraser = tool_ == Tool::Eraser;
    const QPointF local = point - canvasRect().topLeft();
    current_.points.append(local);
    current_.widths.append(widthFor(pressure));
    const QRect touched = drawSegment(ink_, current_, 0, &currentMid_, &currentSmoothed_);
    update(touched.translated(canvasRect().topLeft()));
}

void Notebook::move(QPointF point, qreal pressure)
{
    if (!drawing_)
        return;
    const QPointF local = point - canvasRect().topLeft();
    // Points closer than half a pixel add nothing but work.
    const QPointF last = current_.points.last();
    if (qAbs(local.x() - last.x()) + qAbs(local.y() - last.y()) < 0.5)
        return;
    current_.points.append(local);
    current_.widths.append(widthFor(pressure));
    const QRect touched = drawSegment(ink_, current_, current_.points.size() - 1, &currentMid_,
                                      &currentSmoothed_);
    update(touched.translated(canvasRect().topLeft()));
}

void Notebook::release(QPointF point)
{
    if (!drawing_) {
        const auto button = buttonAt(point.toPoint());
        if (button != Button::None && button == pressed_)
            run(button);
        pressed_ = Button::None;
        return;
    }
    move(point, -1);
    // Finish the line at the pen's last position, not the smoothed one.
    if (!current_.points.isEmpty()) {
        QPainter painter(&ink_);
        painter.setRenderHint(QPainter::Antialiasing, true);
        if (current_.eraser)
            painter.setCompositionMode(QPainter::CompositionMode_Clear);
        const qreal width = current_.widths.last();
        painter.setPen(QPen(current_.eraser ? QColor(Qt::transparent) : QColor(Qt::black), width,
                            Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawLine(currentMid_, current_.points.last());
        const int margin = int(std::ceil(width)) + 2;
        update(QRectF(currentMid_, current_.points.last())
                       .normalized()
                       .toAlignedRect()
                       .adjusted(-margin, -margin, margin, margin)
                       .translated(canvasRect().topLeft()));
        // Keep the replayed stroke identical to what is on screen.
        current_.points.append(current_.points.last());
        current_.widths.append(width);
    }
    const bool firstStroke = strokes_.isEmpty();
    strokes_.append(current_);
    current_ = Stroke();
    drawing_ = false;
    dirty_ = true;
    autosave_.start();
    endFastRefreshLater();
    if (firstStroke)
        update(toolbarRect());
}

void Notebook::cancelStroke()
{
    if (!drawing_)
        return;
    strokes_.append(current_);
    current_ = Stroke();
    drawing_ = false;
    dirty_ = true;
    endFastRefreshLater();
}

void Notebook::endFastRefreshLater()
{
    fastEnd_.start();
}

bool Notebook::eventFilter(QObject *object, QEvent *event)
{
    if (object == parentWidget() && event->type() == QEvent::Resize)
        setGeometry(parentWidget()->rect());
    return QWidget::eventFilter(object, event);
}

bool Notebook::event(QEvent *event)
{
    switch (event->type()) {
    case QEvent::TouchBegin:
    case QEvent::TouchUpdate:
    case QEvent::TouchEnd:
    case QEvent::TouchCancel: {
        const auto *touch = static_cast<QTouchEvent *>(event);
        if (event->type() == QEvent::TouchCancel) {
            cancelStroke();
            touchId_ = -1;
            event->accept();
            return true;
        }
        // Follow the first contact only. A palm or a second finger that lands
        // later is ignored until the first one lifts.
        for (const auto &point : contacts(touch)) {
            if (touchId_ < 0 && point.pressed) {
                touchId_ = point.id;
                press(point.position, point.pressure);
                continue;
            }
            if (point.id != touchId_)
                continue;
            if (point.released) {
                release(point.position);
                touchId_ = -1;
            } else {
                move(point.position, point.pressure);
            }
        }
        if (event->type() == QEvent::TouchEnd && touchId_ >= 0) {
            cancelStroke();
            touchId_ = -1;
        }
        event->accept();
        return true;
    }
    default:
        return QWidget::event(event);
    }
}

void Notebook::mousePressEvent(QMouseEvent *event)
{
    if (touchId_ >= 0)
        return;
    press(ui::mousePosition(event), -1);
}

void Notebook::mouseMoveEvent(QMouseEvent *event)
{
    if (touchId_ >= 0)
        return;
    move(ui::mousePosition(event), -1);
}

void Notebook::mouseReleaseEvent(QMouseEvent *event)
{
    if (touchId_ >= 0)
        return;
    release(ui::mousePosition(event));
}

void Notebook::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_PageDown:
    case Qt::Key_Right:
    case Qt::Key_Down:
        goToPage(page_ + 1);
        break;
    case Qt::Key_PageUp:
    case Qt::Key_Left:
    case Qt::Key_Up:
        goToPage(page_ - 1);
        break;
    case Qt::Key_Escape:
    case Qt::Key_Back:
        finish();
        break;
    default:
        QWidget::keyPressEvent(event);
        return;
    }
    event->accept();
}

void Notebook::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (ink_.size() == canvasRect().size())
        return;
    // A rotation keeps the drawing: save it, then reload at the new size.
    cancelStroke();
    savePage();
    QImage keep = ink_;
    loadPage(page_);
    if (!keep.isNull()) {
        QPainter painter(&base_);
        painter.drawImage(0, 0, keep);
        ink_ = base_;
    }
}

void Notebook::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    const QRect dirty = event->rect();
    const QRect canvas = canvasRect();
    if (dirty.intersects(canvas)) {
        const QRect area = dirty.intersected(canvas);
        painter.fillRect(area, Qt::white);
        // A 5 mm dot grid on screen only; saved pages hold just the ink.
        const int step = ui::px(59), dot = qMax(2, ui::px(3));
        const QPoint origin = canvas.topLeft() + QPoint(step / 2, step / 2);
        const int firstX = origin.x() + qMax(0, (area.left() - origin.x()) / step) * step;
        const int firstY = origin.y() + qMax(0, (area.top() - origin.y()) / step) * step;
        for (int y = firstY; y <= area.bottom() + dot; y += step)
            for (int x = firstX; x <= area.right() + dot; x += step)
                painter.fillRect(x - dot / 2, y - dot / 2, dot, dot, QColor(150, 150, 150));
        const QRect excerpt = excerptRect();
        if (!excerpt.isEmpty() && area.intersects(excerpt)) {
            QFont font = ui::interfaceFont(ui::px(26));
            font.setItalic(true);
            painter.setFont(font);
            painter.setPen(QColor(90, 90, 90));
            painter.drawText(excerpt, Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
                             QStringLiteral("“%1”").arg(excerpt_));
        }
        painter.drawImage(area.topLeft(), ink_, area.translated(-canvas.topLeft()));
    }
    const QRect bar = toolbarRect();
    if (dirty.intersects(bar)) {
        painter.fillRect(bar, Qt::white);
        painter.setPen(QPen(Qt::black, qMax(1, ui::px(2))));
        painter.drawLine(bar.bottomLeft(), bar.bottomRight());
        painter.setFont(ui::interfaceFont(ui::px(30)));
        for (const auto button : { Button::Done, Button::Previous, Button::Next, Button::Tool,
                                   Button::Undo, Button::Clear }) {
            const QRect rect = buttonRect(button);
            const bool disabled = (button == Button::Previous && page_ <= 1)
                    || (button == Button::Next && page_ > pages_)
                    || (button == Button::Undo && strokes_.isEmpty());
            painter.setPen(disabled ? QColor(170, 170, 170) : QColor(Qt::black));
            if (button == Button::Previous || button == Button::Next) {
                QFont arrow = ui::interfaceFont(ui::px(48));
                painter.setFont(arrow);
                painter.drawText(rect, Qt::AlignCenter, buttonLabel(button));
                painter.setFont(ui::interfaceFont(ui::px(30)));
            } else {
                painter.drawText(rect, Qt::AlignCenter, buttonLabel(button));
            }
            if (button == Button::Tool && tool_ == Tool::Eraser) {
                painter.setPen(QPen(Qt::black, qMax(1, ui::px(2))));
                painter.drawRect(rect.adjusted(ui::px(10), ui::px(18), -ui::px(10), -ui::px(18)));
            }
        }
        // Between ‹ and ›: the page number.
        const QRect pageCell(buttonRect(Button::Previous).right() + 1, 0,
                             buttonRect(Button::Next).left() - buttonRect(Button::Previous).right()
                                     - 1,
                             bar.height());
        painter.setPen(Qt::black);
        painter.setFont(ui::interfaceFont(ui::px(28)));
        painter.drawText(pageCell, Qt::AlignCenter,
                         strings::get("notebook.page").arg(page_).arg(qMax(pages_, page_)));
    }
}
} // namespace kobalt
