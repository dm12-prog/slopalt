#pragma once
#include <QByteArray>
#include <QHash>
#include <QImage>
#include <QString>

namespace kobalt::storage {
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
} // namespace notebook
} // namespace kobalt::storage
