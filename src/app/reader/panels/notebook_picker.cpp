#include "notebook_picker.h"
#include "support/strings.h"
#include "ui/input/input.h"
#include "ui/style/fonts.h"
#include "ui/style/scale.h"
#include <QDir>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QTouchEvent>

namespace kobalt {
NotebookPicker::NotebookPicker(QWidget *parent, const QString &root, const QStringList &files,
                               const QString &current)
    : QWidget(parent), root_(QDir(root).absolutePath()), current_(current), files_(files)
{
    setObjectName("rrNotebookPicker");
    setAttribute(Qt::WA_AcceptTouchEvents);
    setAttribute(Qt::WA_OpaquePaintEvent);
    parent->installEventFilter(this);
    setGeometry(parent->rect());
    show();
    raise();
}

bool NotebookPicker::eventFilter(QObject *object, QEvent *event)
{
    if (object == parentWidget() && event->type() == QEvent::Resize)
        setGeometry(parentWidget()->rect());
    return QWidget::eventFilter(object, event);
}

int NotebookPicker::rowHeight() const
{
    return ui::px(120);
}

QRect NotebookPicker::headerRect() const
{
    return QRect(0, 0, width(), ui::px(150));
}

QRect NotebookPicker::cancelRect() const
{
    return QRect(width() - ui::px(220), 0, ui::px(220), headerRect().height());
}

QRect NotebookPicker::previousRect() const
{
    return QRect(0, height() - rowHeight(), width() / 2, rowHeight());
}

QRect NotebookPicker::nextRect() const
{
    return QRect(width() / 2, height() - rowHeight(), width() - width() / 2, rowHeight());
}

int NotebookPicker::rowsPerPage() const
{
    return qMax(1, (height() - headerRect().height() - rowHeight()) / rowHeight());
}

bool NotebookPicker::event(QEvent *event)
{
    if (event->type() == QEvent::TouchBegin || event->type() == QEvent::TouchUpdate
        || event->type() == QEvent::TouchCancel) {
        event->accept();
        return true;
    }
    if (event->type() == QEvent::TouchEnd) {
        const auto contacts = ui::touchContacts(static_cast<QTouchEvent *>(event));
        if (!contacts.isEmpty())
            tap(contacts.first().position.toPoint());
        event->accept();
        return true;
    }
    return QWidget::event(event);
}

void NotebookPicker::mouseReleaseEvent(QMouseEvent *event)
{
    tap(ui::mousePosition(event).toPoint());
}

void NotebookPicker::tap(QPoint point)
{
    const int pages = qMax(1, (files_.size() + rowsPerPage() - 1) / rowsPerPage());
    if (cancelRect().contains(point)) {
        if (finished)
            finished({});
        return;
    }
    if (pages > 1 && previousRect().contains(point)) {
        page_ = (page_ + pages - 1) % pages;
        update();
        return;
    }
    if (pages > 1 && nextRect().contains(point)) {
        page_ = (page_ + 1) % pages;
        update();
        return;
    }
    const int row = (point.y() - headerRect().height()) / rowHeight();
    if (point.y() < headerRect().height() || row >= rowsPerPage())
        return;
    const int index = page_ * rowsPerPage() + row;
    if (index >= 0 && index < files_.size() && finished)
        finished(files_[index]);
}

void NotebookPicker::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.fillRect(rect(), Qt::white);
    const int margin = ui::px(40);
    const QRect header = headerRect();
    painter.setPen(Qt::black);
    painter.setFont(ui::interfaceFont(ui::px(36)));
    painter.drawText(header.adjusted(margin, 0, -cancelRect().width(), 0),
                     Qt::AlignLeft | Qt::AlignVCenter | Qt::TextWordWrap,
                     strings::get("notebook.choose_stock"));
    painter.setFont(ui::interfaceFont(ui::px(30)));
    painter.drawText(cancelRect(), Qt::AlignCenter, strings::get("common.cancel"));
    painter.setPen(QPen(Qt::black, qMax(1, ui::px(2))));
    painter.drawLine(header.bottomLeft(), header.bottomRight());
    if (files_.isEmpty()) {
        painter.setFont(ui::interfaceFont(ui::px(30)));
        painter.drawText(rect().adjusted(margin, header.height(), -margin, 0),
                         Qt::AlignCenter | Qt::TextWordWrap, strings::get("notebook.none_stock"));
        return;
    }
    const int perPage = rowsPerPage();
    painter.setFont(ui::interfaceFont(ui::px(32)));
    for (int row = 0; row < perPage; ++row) {
        const int index = page_ * perPage + row;
        if (index >= files_.size())
            break;
        const QRect line(0, header.height() + row * rowHeight(), width(), rowHeight());
        QString name = files_[index].mid(root_.size() + 1);
        if (name.endsWith(".nebo", Qt::CaseInsensitive))
            name.chop(5);
        if (files_[index] == current_)
            name = QStringLiteral("✓ ") + name;
        painter.setPen(Qt::black);
        painter.drawText(
                line.adjusted(margin, 0, -margin, 0), Qt::AlignLeft | Qt::AlignVCenter,
                painter.fontMetrics().elidedText(name, Qt::ElideMiddle, width() - 2 * margin));
        painter.setPen(QPen(QColor(180, 180, 180), 1));
        painter.drawLine(line.bottomLeft(), line.bottomRight());
    }
    const int pages = qMax(1, (files_.size() + perPage - 1) / perPage);
    if (pages > 1) {
        painter.setPen(Qt::black);
        painter.drawText(previousRect(), Qt::AlignCenter, QStringLiteral("‹"));
        painter.drawText(nextRect(), Qt::AlignCenter, QStringLiteral("›"));
        painter.drawText(QRect(0, previousRect().top(), width(), rowHeight()), Qt::AlignCenter,
                         strings::get("notebook.page").arg(page_ + 1).arg(pages));
    }
}
} // namespace kobalt
