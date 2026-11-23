#include "native_types.h"
#include "platform/api/stock_notebook.h"
#include "platforms/kobo/common/native_symbol.h"
#include "support/diagnostics.h"
#include <QObject>
#include <QPointer>
#include <QSettings>
#include <QUrl>
#include <QVariantMap>
#include <QVector>

// Opens a notebook in Nickel's own notebook app (MyScript .nebo files), the
// way its library does: the reader opener that Nickel creates for a volume has
// openNotePad for notebook volumes. Symbols checked in 4.38.23852.
//
// Nickel keeps each notebook's page (PartIndex) and scroll offset (ViewOffset)
// in Kobo eReader.conf, group IInkSettings, under the file path with slashes
// replaced by underscores, and opens the notebook there. QSettings objects of
// one file share their data within a process, so Nickel sees a change made here
// at once and writes it with its own settings.
namespace kobalt {
namespace {
const char *const settingsFile = "/mnt/onboard/.kobo/Kobo/Kobo eReader.conf";

struct Api
{
    Volume (*get)(const QString &, const QString &) =
            symbol<decltype(get)>("_ZN13VolumeManager7getByIdERK7QStringS2_");
    QString (*id)(const void *) = symbol<decltype(id)>("_ZNK7Content5getIdEv");
    void *(*device)() = symbol<decltype(device)>("_ZN6Device16getCurrentDeviceEv");
    const QString &(*dbName)(const void *) = symbol<decltype(dbName)>("_ZNK6Device9getDbNameEv");
    QObject *(*create)(void *, const void *, const void *, void *) = symbol<decltype(create)>(
            "_ZN16ReadingViewMixin12createReaderERK6VolumeRK7QVectorI10ContentKeyEP25AbstractLibraryControllerIS0_E");
    void (*openNotePad)(QObject *, const void *) =
            symbol<decltype(openNotePad)>("_ZN14N3ReaderOpener11openNotePadERK6Volume");

    bool valid() const { return get && id && device && dbName && create && openNotePad; }
};

const Api &api()
{
    static const Api value;
    return value;
}

QString settingsKey(const QString &contentId)
{
    QString path = contentId.startsWith("file://") ? QUrl(contentId).toLocalFile() : contentId;
    path.replace('/', '_');
    return QStringLiteral("IInkSettings/") + path;
}

platform::StockNotebookSpot spot(const QString &contentId)
{
    QSettings settings(settingsFile, QSettings::IniFormat);
    const auto values = settings.value(settingsKey(contentId)).toMap();
    platform::StockNotebookSpot result;
    if (values.contains("PartIndex")) {
        result.part = values.value("PartIndex").toInt();
        result.offset = values.value("ViewOffset").toPoint();
    }
    return result;
}

bool open(const QString &contentId, const platform::StockNotebookSpot &place)
{
    const auto &native = api();
    if (!native.valid() || contentId.isEmpty()) {
        diagnostic("stock-notebook", { { "result", "unavailable" } }, false);
        return false;
    }
    const Volume volume = native.get(contentId, native.dbName(native.device()));
    if (native.id(&volume) != contentId) {
        diagnostic("stock-notebook", { { "result", "unknown-volume" } }, false);
        return false;
    }
    if (place.valid()) {
        QSettings settings(settingsFile, QSettings::IniFormat);
        auto values = settings.value(settingsKey(contentId)).toMap();
        values.insert("PartIndex", place.part);
        values.insert("ViewOffset", place.offset);
        settings.setValue(settingsKey(contentId), values);
    }
    // An empty vector has the same layout for any element type.
    const QVector<int> keys;
    QPointer<QObject> opener = native.create(nullptr, &volume, &keys, nullptr);
    if (!opener) {
        diagnostic("stock-notebook", { { "result", "no-opener" } }, false);
        return false;
    }
    native.openNotePad(opener, &volume);
    if (opener && !opener->parent())
        opener->deleteLater();
    diagnostic("stock-notebook",
               { { "result", "opened" }, { "part", place.part }, { "y", place.offset.y() } },
               false);
    return true;
}

const bool registered = [] {
    auto &notebooks = platform::stockNotebooks();
    notebooks.open = &open;
    notebooks.spot = &spot;
    notebooks.root = QStringLiteral("/mnt/onboard/My Notebooks");
    return true;
}();
} // namespace
} // namespace kobalt
