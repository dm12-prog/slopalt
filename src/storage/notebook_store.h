#pragma once
#include <QByteArray>
#include <QHash>
#include <QImage>
#include <QString>
#include <QStringList>
#include <QVector>

namespace kobalt::storage {
class RecordWriter;

// Files of the handwritten notebooks, one folder per book:
// page-001.png and on, and links.json that maps a highlight ID to its page.
// The writes sync storage, so callers run them on the writer thread.
namespace notebook {
inline constexpr int maxPages = 999;
QString pageFile(const QString &folder, int page);
// The highest page number with a file, or 0.
int lastPage(const QString &folder);
// Highlight ID to page number. Empty when the file is missing or unreadable.
QHash<QString, int> readLinks(const QString &folder);
bool writePage(const QString &folder, int page, const QImage &image);
bool writeLinks(const QString &folder, const QHash<QString, int> &links);

// A book's whole notebook in one file, `folder`.notebook, used where the
// host's own pen layer draws the pages: each page is the SVG document of its
// strokes, and `links` maps a highlight ID to its page number.
struct Book
{
    QVector<QByteArray> pages;
    QHash<QString, int> links;
};

QString bookFile(const QString &folder);
// Empty when the file is missing or unreadable.
Book readBook(const QString &folder);
bool writeBook(const QString &folder, const Book &book);
// Puts one page's SVG in a file in memory (/tmp), for a pen layer that loads
// strokes from a file only, and returns its path. Empty on failure. It does
// not touch storage, so the main thread may call it.
QString scratchPage(const QByteArray &svg);

// The book's own notebook in the host's notebook format, where the host has
// a notebook engine: book.nebo in the book's folder holds every page, and
// pages.json beside it names, for each page in order, the highlight the page
// belongs to (an empty string for a page without one).
struct HostBook
{
    QStringList pages;
    // The highlight that the last page opened for, while that page belongs
    // to none. The host puts an empty page behind a page only once it has
    // ink, so the last page may still be empty: it becomes this highlight's
    // page when it is no longer the last one.
    QString pending;

    // The page of this highlight, counted from 0, or -1.
    int pageOf(const QString &highlight) const
    {
        return highlight.isEmpty() ? -1 : pages.indexOf(highlight);
    }

    // The highlight that the last page waits for, if any.
    QString waiting() const
    {
        return !pages.isEmpty() && pages.last().isEmpty() ? pending : QString();
    }

    // The page that opens for this highlight: its own, or the last page
    // while that one waits for it. -1 asks for a new page.
    int pageFor(const QString &highlight) const
    {
        const int own = pageOf(highlight);
        if (own < 0 && !highlight.isEmpty() && highlight == waiting())
            return pages.size() - 1;
        return own;
    }
};

QString hostBookFile(const QString &folder);
// Makes the book's folder. The host creates the notebook file, not its folder.
bool prepareHostBook(const QString &folder);
// Empty when the file is missing or unreadable.
HostBook readHostBook(const QString &folder);
bool writeHostBook(const QString &folder, const HostBook &book);
// One writer for every notebook write, never destroyed, so a write posted
// just before a notebook or reader closes still finishes.
RecordWriter &writer();
} // namespace notebook
} // namespace kobalt::storage
