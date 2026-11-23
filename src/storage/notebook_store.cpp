#include "notebook_store.h"
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>

namespace kobalt::storage::notebook {
namespace {
constexpr qint64 maxLinksBytes = 1 << 20;
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
} // namespace kobalt::storage::notebook
