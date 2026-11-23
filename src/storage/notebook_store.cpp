#include "notebook_store.h"
#include "record_writer.h"
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>

namespace kobalt::storage::notebook {
namespace {
constexpr qint64 maxLinksBytes = 1 << 20;
// A page of dense handwriting is a few hundred kilobytes of SVG.
constexpr qint64 maxBookBytes = 64 << 20;
}

QString pageFile(const QString &folder, int page)
{
    return QStringLiteral("%1/page-%2.png").arg(folder).arg(page, 3, 10, QLatin1Char('0'));
}

int lastPage(const QString &folder)
{
    static const QRegularExpression name("^page-(\\d{3})\\.png$");
    int last = 0;
    const auto files = QDir(folder).entryList(QStringList() << "page-*.png", QDir::Files);
    for (const auto &file : files) {
        const auto match = name.match(file);
        if (match.hasMatch())
            last = qMax(last, match.captured(1).toInt());
    }
    return qMin(last, maxPages);
}

QHash<QString, int> readLinks(const QString &folder)
{
    QHash<QString, int> links;
    QFile file(folder + "/links.json");
    if (!file.open(QIODevice::ReadOnly) || file.size() > maxLinksBytes)
        return links;
    const auto object = QJsonDocument::fromJson(file.readAll()).object();
    for (auto it = object.begin(); it != object.end(); ++it) {
        const int page = it.value().toInt();
        if (page >= 1 && page <= maxPages)
            links.insert(it.key(), page);
    }
    return links;
}

bool writePage(const QString &folder, int page, const QImage &image)
{
    if (!QDir().mkpath(folder))
        return false;
    QSaveFile out(pageFile(folder, page));
    if (!out.open(QIODevice::WriteOnly) || !image.save(&out, "PNG"))
        return false;
    return out.commit();
}

bool writeLinks(const QString &folder, const QHash<QString, int> &links)
{
    QJsonObject object;
    for (auto it = links.begin(); it != links.end(); ++it)
        object.insert(it.key(), it.value());
    const QByteArray data = QJsonDocument(object).toJson();
    if (!QDir().mkpath(folder))
        return false;
    QSaveFile out(folder + "/links.json");
    if (!out.open(QIODevice::WriteOnly) || out.write(data) != data.size())
        return false;
    return out.commit();
}

QString bookFile(const QString &folder)
{
    return folder + ".notebook";
}

Book readBook(const QString &folder)
{
    Book book;
    QFile file(bookFile(folder));
    if (!file.open(QIODevice::ReadOnly) || file.size() > maxBookBytes)
        return book;
    const auto object = QJsonDocument::fromJson(file.readAll()).object();
    const auto pages = object.value("pages").toArray();
    for (const auto &page : pages) {
        if (book.pages.size() >= maxPages)
            break;
        book.pages.append(page.toString().toUtf8());
    }
    const auto links = object.value("links").toObject();
    for (auto it = links.begin(); it != links.end(); ++it) {
        const int page = it.value().toInt();
        if (page >= 1 && page <= maxPages)
            book.links.insert(it.key(), page);
    }
    return book;
}

bool writeBook(const QString &folder, const Book &book)
{
    QJsonArray pages;
    for (const auto &page : book.pages)
        pages.append(QString::fromUtf8(page));
    QJsonObject links;
    for (auto it = book.links.begin(); it != book.links.end(); ++it)
        links.insert(it.key(), it.value());
    QJsonObject object;
    object.insert("version", 1);
    object.insert("pages", pages);
    object.insert("links", links);
    const QByteArray data = QJsonDocument(object).toJson(QJsonDocument::Compact);
    if (!QDir().mkpath(QFileInfo(bookFile(folder)).path()))
        return false;
    QSaveFile out(bookFile(folder));
    if (!out.open(QIODevice::WriteOnly) || out.write(data) != data.size())
        return false;
    return out.commit();
}

QString scratchPage(const QByteArray &svg)
{
    const QString path = QStringLiteral("/tmp/kobalt-notebook-page.svg");
    QFile out(path);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate) || out.write(svg) != svg.size())
        return {};
    out.close();
    return path;
}

StockLinks readStockLinks(const QString &folder)
{
    StockLinks links;
    QFile file(folder + "/stock.json");
    if (!file.open(QIODevice::ReadOnly) || file.size() > maxLinksBytes)
        return links;
    const auto object = QJsonDocument::fromJson(file.readAll()).object();
    links.notebook = object.value("notebook").toString();
    links.pending = object.value("pending").toString();
    const auto places = object.value("places").toObject();
    for (auto it = places.begin(); it != places.end(); ++it) {
        const auto place = it.value().toObject();
        StockPlace value;
        value.part = place.value("part").toInt(-1);
        value.x = place.value("x").toInt();
        value.y = place.value("y").toInt();
        if (value.part >= 0)
            links.places.insert(it.key(), value);
    }
    return links;
}

bool writeStockLinks(const QString &folder, const StockLinks &links)
{
    QJsonObject places;
    for (auto it = links.places.begin(); it != links.places.end(); ++it) {
        QJsonObject place;
        place.insert("part", it.value().part);
        place.insert("x", it.value().x);
        place.insert("y", it.value().y);
        places.insert(it.key(), place);
    }
    QJsonObject object;
    object.insert("notebook", links.notebook);
    if (!links.pending.isEmpty())
        object.insert("pending", links.pending);
    object.insert("places", places);
    const QByteArray data = QJsonDocument(object).toJson();
    if (!QDir().mkpath(folder))
        return false;
    QSaveFile out(folder + "/stock.json");
    if (!out.open(QIODevice::WriteOnly) || out.write(data) != data.size())
        return false;
    return out.commit();
}

QStringList findStockNotebooks(const QString &root)
{
    QStringList found;
    const QString base = QDir(root).absolutePath();
    QDirIterator it(base, QStringList() << "*.nebo", QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext() && found.size() < 500) {
        const QString path = it.next();
        const QString relative = path.mid(base.size() + 1);
        if (relative.count('/') > 3 || relative.startsWith('.') || relative.contains("/."))
            continue;
        found.append(path);
    }
    found.sort(Qt::CaseInsensitive);
    return found;
}

RecordWriter &writer()
{
    static auto *shared = new RecordWriter(RecordWriter::Delivery::All);
    return *shared;
}
} // namespace kobalt::storage::notebook
