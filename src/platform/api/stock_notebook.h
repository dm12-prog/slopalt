#pragma once
#include <QPoint>
#include <QString>

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
    bool (*open)(const QString &contentId, const StockNotebookSpot &spot) = nullptr;
    // The place the notebook was left at, or an invalid spot.
    StockNotebookSpot (*spot)(const QString &contentId) = nullptr;
    // Where the host keeps its notebooks.
    QString root;

    bool available() const { return open && spot; }
};

StockNotebooks &stockNotebooks();
} // namespace kobalt::platform
