#pragma once
#include <QPoint>
#include <QString>
#include <functional>

class QWidget;

namespace kobalt::platform {
// A place in one of the host's own notebooks: the page (part) and the scroll
// offset within it, as the host saves them when the notebook closes.
struct StockNotebookSpot
{
    int part = -1;
    QPoint offset;

    bool valid() const { return part >= 0; }
};

// The Kobo firmware 4 adapter fills this in; elsewhere every member is null.
struct StockNotebooks
{
    // Opens the notebook with this content ID. A valid spot is restored first.
    // Opens the notebook over `reader`, which stays open but hidden while the
    // host shows the notebook. When the notebook closes, the reader is shown
    // again and `closed` runs. False when the notebook cannot be opened; the
    // reader is then left as it was.
    bool (*open)(const QString &contentId, const StockNotebookSpot &spot, QWidget *reader,
                 std::function<void()> closed) = nullptr;
    // The place the notebook was left at, or an invalid spot.
    StockNotebookSpot (*spot)(const QString &contentId) = nullptr;
    // Where the host keeps its notebooks.
    QString root;

    bool available() const { return open && spot; }
};

StockNotebooks &stockNotebooks();
} // namespace kobalt::platform
