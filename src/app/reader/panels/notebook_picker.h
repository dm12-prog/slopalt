#pragma once
#include <QStringList>
#include <QWidget>
#include <functional>

namespace kobalt {
// A full-window list of the Kobo's own notebooks (.nebo files), to choose the
// one that a book's highlights open. Rows show the path below the notebook
// folder without the extension. Tap a row to choose it, or Cancel.
class NotebookPicker final : public QWidget
{
public:
    NotebookPicker(QWidget *parent, const QString &root, const QStringList &files,
                   const QString &current);
    // The chosen file path; empty when cancelled. The owner destroys the picker.
    std::function<void(const QString &)> finished;

protected:
    bool eventFilter(QObject *object, QEvent *event) override;
    bool event(QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    void tap(QPoint point);
    int rowHeight() const;
    int rowsPerPage() const;
    QRect headerRect() const;
    QRect cancelRect() const;
    QRect previousRect() const;
    QRect nextRect() const;
    QString root_, current_;
    QStringList files_;
    int page_ = 0;
};
} // namespace kobalt
