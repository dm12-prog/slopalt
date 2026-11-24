#include "notebook_store.h"
#include "record_writer.h"
#include <QDir>
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

QString hostBookFile(const QString &folder)
{
    return folder + "/book.nebo";
}

bool prepareHostBook(const QString &folder)
{
    return QDir().mkpath(folder);
}

HostBook readHostBook(const QString &folder)
{
    HostBook book;
    QFile file(folder + "/pages.json");
    if (!file.exists())
        return book;
    if (!file.open(QIODevice::ReadOnly) || file.size() > maxLinksBytes) {
        book.damaged = true;
        return book;
    }
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(file.readAll(), &error);
    const auto object = document.object();
    if (error.error != QJsonParseError::NoError || !document.isObject()
        || object.value("version").toInt(1) > 1) {
        book.damaged = true;
        return book;
    }
    const auto pages = object.value("pages").toArray();
    for (const auto &page : pages) {
        if (book.pages.size() >= maxPages)
            break;
        book.pages.append(page.toString());
    }
    book.pending = object.value("pending").toString();
    return book;
}

bool writeHostBook(const QString &folder, const HostBook &book)
{
    QJsonArray pages;
    for (const auto &page : book.pages)
        pages.append(page);
    QJsonObject object;
    object.insert("version", 1);
    object.insert("pages", pages);
    if (!book.pending.isEmpty())
        object.insert("pending", book.pending);
    const QByteArray data = QJsonDocument(object).toJson();
    if (!QDir().mkpath(folder))
        return false;
    QSaveFile out(folder + "/pages.json");
    if (!out.open(QIODevice::WriteOnly) || out.write(data) != data.size())
        return false;
    return out.commit();
}

RecordWriter &writer()
{
    static auto *shared = new RecordWriter(RecordWriter::Delivery::All);
    return *shared;
}
} // namespace kobalt::storage::notebook
