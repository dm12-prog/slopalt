#pragma once
#include <QByteArray>
#include <QHash>
#include <QImage>
#include <QString>
#include <QStringList>

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

// The book's Kobo (Nickel) notebook, in stock.json: its content ID, the place
// in it that each linked highlight opens, and the highlight waiting for the
// place where the notebook is left the next time it closes.
struct StockPlace
{
    int part = -1, x = 0, y = 0;
};

struct StockLinks
{
    QString notebook, pending;
    QHash<QString, StockPlace> places;
};

StockLinks readStockLinks(const QString &folder);
bool writeStockLinks(const QString &folder, const StockLinks &links);
// The .nebo files under `root`, at most three folders deep, sorted by path.
// Hidden entries are skipped.
QStringList findStockNotebooks(const QString &root);
// One writer for every notebook write, never destroyed, so a write posted
// just before a notebook or reader closes still finishes.
RecordWriter &writer();
} // namespace notebook
} // namespace kobalt::storage
