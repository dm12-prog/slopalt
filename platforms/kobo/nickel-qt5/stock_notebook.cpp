#include "native_types.h"
#include "platform/api/stock_notebook.h"
#include "platforms/kobo/common/native_symbol.h"
#include "support/diagnostics.h"
#include "platforms/kobo/common/browser_handoff.h"
#include "platforms/kobo/common/native_lifecycle.h"
#include <QApplication>
#include <QTimer>
#include <QWidget>
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
//
// The reader stays open underneath. It is hidden the way the browser handoff
// hides it (the coordinator leaves a reader with that mark alone), Nickel
// pushes the notebook onto its page stack, and the reader comes back when the
// notebook leaves the top of that stack.
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
    void *(*mainWindow)() =
            symbol<decltype(mainWindow)>("_ZN20MainWindowController14sharedInstanceEv");
    // The page on top of Nickel's page stack.
    QWidget *(*currentView)(const void *) =
            symbol<decltype(currentView)>("_ZNK20MainWindowController11currentViewEv");

    bool valid() const
    {
        return get && id && device && dbName && create && openNotePad && mainWindow
                && currentView;
    }
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

// Watches Nickel's page stack from the moment the notebook is asked for until
// it has closed, then shows the reader again.
class Handoff : public QObject
{
public:
    Handoff(QWidget *reader, QWidget *before, std::function<void()> closed)
        : QObject(qApp), reader_(reader), before_(before), closed_(std::move(closed))
    {
        poll_.setInterval(200);
        QObject::connect(&poll_, &QTimer::timeout, this, [this] { check(); });
        poll_.start();
    }

private:
    void check()
    {
        const auto &native = api();
        auto *main = native.mainWindow();
        QWidget *top = main ? native.currentView(main) : nullptr;
        ++polls_;
        if (!reader_) {
            deleteLater();
            return;
        }
        if (!seen_) {
            // The notebook arrives a moment after the request. Nickel may
            // first ask to download handwriting files; give that a minute.
            if (top && top != before_) {
                notebook_ = top;
                seen_ = true;
                diagnostic("stock-notebook", { { "result", "shown" }, { "polls", polls_ } },
                           false);
            } else if (polls_ > 300) {
                finish("never-shown");
            }
            return;
        }
        // Sleep hides Nickel's pages without closing them.
        if (NativeLifecycle::suspended())
            return;
        if (!notebook_ || top != notebook_)
            finish("closed");
    }

    void finish(const char *result)
    {
        poll_.stop();
        diagnostic("stock-notebook", { { "result", result }, { "polls", polls_ } }, false);
        if (reader_) {
            reader_->setProperty("kobalt_native_browser", false);
            reader_->show();
            reader_->raise();
            reader_->activateWindow();
            if (closed_)
                closed_();
        }
        deleteLater();
    }

    QPointer<QWidget> reader_, before_, notebook_;
    std::function<void()> closed_;
    QTimer poll_;
    bool seen_ = false;
    int polls_ = 0;
};

bool open(const QString &contentId, const platform::StockNotebookSpot &place, QWidget *reader,
          std::function<void()> closed)
{
    const auto &native = api();
    if (!native.valid() || contentId.isEmpty() || !reader) {
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
    auto *main = native.mainWindow();
    if (!main) {
        diagnostic("stock-notebook", { { "result", "no-main-window" } }, false);
        return false;
    }
    // An empty vector has the same layout for any element type.
    const QVector<int> keys;
    QPointer<QObject> opener = native.create(nullptr, &volume, &keys, nullptr);
    if (!opener) {
        diagnostic("stock-notebook", { { "result", "no-opener" } }, false);
        return false;
    }
    new Handoff(reader, native.currentView(main), std::move(closed));
    // The same mark and order as the browser handoff.
    reader->setProperty("kobalt_native_browser", true);
    reader->hide();
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
