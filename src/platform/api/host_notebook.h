#pragma once
#include <QString>
#include <QStringList>
#include <QVector>
#include <functional>

class QPainter;
class QRect;
class QWidget;

namespace kobalt::platform {
// One opening of a book's own notebook in the host's notebook engine. The
// notebook is one file with every page; the host draws and saves it with its
// own pen tools.
struct HostNotebookRequest
{
    // The notebook file. The host creates it the first time it opens.
    QString file;
    // The name the host shows for the notebook.
    QString title;
    // The page to open, counted from 0. -1 asks for an empty page at the end.
    int page = -1;
    // The text shown above that page when `page` is -1.
    QString excerpt;
    // The text shown above each page the notebook already has, in page order.
    // A page with an empty text shows none. There is one for each page the
    // notebook had when it last closed; a notebook with fewer pages was
    // changed elsewhere, and its pages are then not matched to these.
    QStringList excerpts;
    // Whether the last of those pages belongs to a highlight, and whether it
    // waits for one. A request with page -1 takes that page only when it
    // has neither, or when the host shows that it is empty.
    bool lastOwned = false;
    bool lastWaiting = false;
    // The band above the page that shows the text. The reader supplies how
    // tall it is, in pixels, when the text may be `width` wide, and how it is
    // painted, so the text looks like its own. `band` is all of the band;
    // the text stays inside `area`, because the host keeps the band's right
    // end for a button of its own.
    std::function<int(const QString &text, int width)> excerptHeight;
    std::function<void(QPainter &painter, const QRect &band, const QRect &area,
                       const QString &text)>
            paintExcerpt;
};

// What the notebook holds when it closes.
struct HostNotebookResult
{
    // The pages are known only when the host's notebook reported them. When
    // false, `pages` is empty and the caller keeps what it had.
    bool known = false;
    // One entry for each page, in order: the index the page had when the
    // notebook opened, `requested` for the page opened for a request with
    // page -1, or `added` for another page that appeared meanwhile. No page
    // is `requested` when the request could not take one. When the notebook
    // was changed elsewhere, every page is `added` but the requested one.
    static constexpr int requested = -1, added = -2;
    QVector<int> pages;
};

// The Kobo firmware 4 adapter fills this in; elsewhere `open` is null.
struct HostNotebooks
{
    // Opens the notebook over `reader`, which stays open but hidden while the
    // host shows the notebook. When the notebook closes, the reader is shown
    // again and `closed` runs. False when the notebook cannot be opened; the
    // reader is then left as it was and `closed` never runs.
    bool (*open)(const HostNotebookRequest &request, QWidget *reader,
                 std::function<void(const HostNotebookResult &)> closed) = nullptr;

    bool available() const { return open; }
};

HostNotebooks &hostNotebooks();
} // namespace kobalt::platform
