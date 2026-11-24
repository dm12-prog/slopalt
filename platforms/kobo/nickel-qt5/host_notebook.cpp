#include "native_types.h"
#include "platform/api/host_notebook.h"
#include "platforms/kobo/common/native_lifecycle.h"
#include "platforms/kobo/common/native_symbol.h"
#include "support/diagnostics.h"
#include "support/main_thread.h"
#include <QApplication>
#include <QByteArray>
#include <QEvent>
#include <QMetaType>
#include <QObject>
#include <QPainter>
#include <QPointer>
#include <QSettings>
#include <QStackedWidget>
#include <QTimer>
#include <QVariant>
#include <QVariantMap>
#include <QVector>
#include <QWidget>
#include <cstring>
#include <functional>
#include <memory>

// A book's own notebook in Nickel's notebook engine (MyScript iink, the
// engine of My Notebooks). Symbols and layouts checked in 4.38.23852.
//
// The notebook is a "basic" notebook file that the reader keeps in its own
// folder. The Volume that describes it is built here, the way Nickel's
// new-notebook dialog builds one. Nickel's notebook screen opens it with the
// reader opener that every notebook uses. Its editor creates the file the
// first time, and when it saves the notebook it also saves the Volume to
// Nickel's library, as it does for every notebook. From then on My Notebooks
// lists the notebook under the title given here.
//
// The reader stays open underneath. It is hidden the way the browser handoff
// hides it (the coordinator leaves a reader with that mark alone), Nickel
// pushes the notebook screen onto its page stack, and the reader comes back
// when that screen leaves the top of the stack.
//
// Pages. A basic notebook is a list of pages, and the editor adds an empty
// page behind the last one as soon as the last one has ink. The editor
// (IInkNotePadWidget, in the notebook plugin) reports the page shown and the
// last page as its scroll value and scroll maximum, and goes to a page with
// scrollTo. A request for a new page opens the last page. If that page has
// ink, the editor is asked to do what it does after a stroke, which adds the
// empty page, and that page is opened instead.
//
// The screen. In a basic notebook the editor fills Nickel's notebook screen.
// The screen's toolbar is not part of its layout: it lies over the top of
// the page while the menu is open, and a button that opens the menu sits in
// the top right corner while it is closed. The text above the page is one
// more widget of that kind, directly above the editor and below those two.
//
// Nickel keeps each notebook's page (PartIndex) in Kobo eReader.conf, group
// IInkSettings, under the file path with slashes replaced by underscores,
// and opens the notebook there. A page that the notebook does not have opens
// the first page. Nickel's settings answer from a cache of their own once
// read, so the page is set through Nickel's own notebook settings
// (IInkSettings), which change the cache and the file together.
namespace kobalt {
namespace {
const char *const settingsFile = "/mnt/onboard/.kobo/Kobo/Kobo eReader.conf";
const char *const pluginFile = "/usr/local/Kobo/libiinknote.so";
// Nickel's content type of a notebook, as its new-notebook dialog sets it.
constexpr int notebookContentType = 6;

// myscript::iink::Editor and the page (part) it shows. Only pointers to them
// pass through here.
struct Editor;
struct Part;

struct Api
{
    void (*construct)(void *) = symbol<decltype(construct)>("_ZN6VolumeC1Ev");
    void (*destroy)(void *) = symbol<decltype(destroy)>("_ZN6VolumeD1Ev");
    void (*setId)(void *, const QVariant &) =
            symbol<decltype(setId)>("_ZN7Content5setIdERK8QVariant");
    void (*setTitle)(void *, const QVariant &) =
            symbol<decltype(setTitle)>("_ZN7Content8setTitleERK8QVariant");
    void (*setContentType)(void *, int) =
            symbol<decltype(setContentType)>("_ZN7Content14setContentTypeE11ContentType");
    void (*setNumPages)(void *, const QVariant &) =
            symbol<decltype(setNumPages)>("_ZN7Content11setNumPagesERK8QVariant");
    void (*setMimeType)(void *, const QVariant &) =
            symbol<decltype(setMimeType)>("_ZN7Content11setMimeTypeERK8QVariant");
    void (*setUserId)(void *, const QVariant &) =
            symbol<decltype(setUserId)>("_ZN7Content9setUserIDERK8QVariant");
    bool (*basic)(const void *) = symbol<decltype(basic)>("_ZNK6Volume15isBasicNotebookEv");
    // The mime type of a basic notebook and the user of every notebook.
    const QByteArray *rawMime = symbol<decltype(rawMime)>("MIME_TYPE_NEBO_RAW");
    const QByteArray *user = symbol<decltype(user)>("NEBO_USER");
    QObject *(*create)(void *, const void *, const void *, void *) = symbol<decltype(create)>(
            "_ZN16ReadingViewMixin12createReaderERK6VolumeRK7QVectorI10ContentKeyEP25AbstractLibraryControllerIS0_E");
    void (*openNotePad)(QObject *, const void *) =
            symbol<decltype(openNotePad)>("_ZN14N3ReaderOpener11openNotePadERK6Volume");
    void *(*mainWindow)() =
            symbol<decltype(mainWindow)>("_ZN20MainWindowController14sharedInstanceEv");
    // The page on top of Nickel's page stack.
    QWidget *(*currentView)(const void *) =
            symbol<decltype(currentView)>("_ZNK20MainWindowController11currentViewEv");
    // Nickel's settings of one notebook (IInkSettings) and the device they
    // belong to.
    const void *(*device)() = symbol<decltype(device)>("_ZN6Device16getCurrentDeviceEv");
    void (*openSettings)(void *, const void *, const QString &) =
            symbol<decltype(openSettings)>("_ZN12IInkSettingsC1ERK6DeviceRK7QString");
    void (*setPartIndex)(void *, int) =
            symbol<decltype(setPartIndex)>("_ZN12IInkSettings12setPartIndexEi");
    void (*saveSettings)(void *) =
            symbol<decltype(saveSettings)>("_ZN12IInkSettings20saveNotebookSettingsEv");
    void (*closeSettings)(void *) = symbol<decltype(closeSettings)>("_ZN12IInkSettingsD1Ev");

    bool present() const
    {
        return construct && destroy && setId && setTitle && setContentType && setNumPages
                && setMimeType && setUserId && basic && rawMime && user && create && openNotePad
                && mainWindow && currentView;
    }

    bool settings() const
    {
        return device && openSettings && setPartIndex && saveSettings && closeSettings;
    }

    // The storage below holds a Volume only while Nickel's is this size.
    bool sized() const
    {
        const int type = QMetaType::type("Volume");
        return type == QMetaType::UnknownType || QMetaType::sizeOf(type) == int(sizeof(Volume));
    }
};

const Api &api()
{
    static const Api value;
    return value;
}

// A Volume that Nickel constructs in storage kept here.
class OwnVolume
{
public:
    OwnVolume() { api().construct(storage); }
    ~OwnVolume() { api().destroy(storage); }
    OwnVolume(const OwnVolume &) = delete;
    OwnVolume &operator=(const OwnVolume &) = delete;

    void *get() { return storage; }

private:
    alignas(Volume) unsigned char storage[sizeof(Volume)];
};

// The notebook plugin's functions. Nickel loads the plugin when it first
// needs it, so these are looked up once a notebook screen is open.
struct Plugin
{
    int (*page)(const QWidget *) = nullptr;
    int (*lastPage)(const QWidget *) = nullptr;
    // Refuses while the editor is busy. The page must exist: the editor
    // reports any other as an error to the user.
    void (*goTo)(QWidget *, int) = nullptr;
    bool (*basic)(QWidget *) = nullptr;
    // The editor's signal that its place changed. The screen's page bar
    // follows it.
    void (*moved)(QWidget *) = nullptr;
    void (*contentChanged)(QWidget *, std::shared_ptr<Editor>) = nullptr;
    // The MyScript editor's getPart and isIdle, which its table of virtual
    // functions holds at partFunction and idleFunction.
    const void *getPart = nullptr;
    const void *isIdle = nullptr;
    // True when the editor's code is the code of 4.38.23852 in the two
    // places that addPage relies on. The MyScript editor's table is checked
    // on the editor itself.
    bool editorKnown = false;

    bool valid() const { return page && lastPage && goTo && basic; }
};

// The editor's std::weak_ptr<myscript::iink::Editor>. Its control block is
// libstdc++'s with atomic counts, as in the firmware.
constexpr int editorOffset = 60;
#if defined(__GLIBCXX__)
static_assert(__gnu_cxx::__default_lock_policy == __gnu_cxx::_S_atomic,
              "the firmware's shared pointers count atomically");
#endif
// myscript::iink::Editor::getPart and isIdle in its table of virtual
// functions.
constexpr int partFunction = 8;
constexpr int idleFunction = 50;

bool sameCode(void *function, int offset, const unsigned char *expected, int size)
{
    const auto address = reinterpret_cast<quintptr>(function);
    if (!address)
        return false;
    // The low bit of a Thumb function's address is not part of the address.
    const auto *code = reinterpret_cast<const unsigned char *>(address & ~quintptr(1)) + offset;
    return std::memcmp(code, expected, size) == 0;
}

Plugin loadPlugin()
{
    Plugin plugin;
    // Present only when Nickel has loaded the plugin; never loads it.
    void *library = dlopen(pluginFile, RTLD_NOW | RTLD_NOLOAD);
    if (!library)
        return plugin;
    const auto find = [library](const char *name) { return dlsym(library, name); };
    plugin.page = reinterpret_cast<decltype(plugin.page)>(
            find("_ZNK17IInkNotePadWidget11scrollValueEv"));
    plugin.lastPage = reinterpret_cast<decltype(plugin.lastPage)>(
            find("_ZNK17IInkNotePadWidget13scrollMaximumEv"));
    plugin.goTo = reinterpret_cast<decltype(plugin.goTo)>(find("_ZN17IInkNotePadWidget8scrollToEi"));
    plugin.basic = reinterpret_cast<decltype(plugin.basic)>(
            find("_ZN17IInkNotePadWidget15isBasicNotebookEv"));
    plugin.moved = reinterpret_cast<decltype(plugin.moved)>(
            find("_ZN17IInkNotePadWidget15positionUpdatedEv"));
    void *contentChanged = find(
            "_ZN17IInkNotePadWidget16onContentChangedESt10shared_ptrIN8myscript4iink6EditorEE");
    plugin.contentChanged = reinterpret_cast<decltype(plugin.contentChanged)>(contentChanged);
    // setScrollValue locks the editor's weak pointer first: `mov r5, r0` ...
    // `add.w r1, r5, #60`. The same bytes mean the same place in the editor.
    static const unsigned char locks[] = { 0x05, 0x46, 0x07, 0xf1, 0x14, 0x08, 0x0c,
                                           0x46, 0x40, 0x46, 0x05, 0xf1, 0x3c, 0x01 };
    // onContentChanged takes the MyScript editor from the shared pointer that
    // r1 points to and asks it for its page first: `ldr r1, [r1]` ...
    // `ldr r3, [r1]`, `ldr r3, [r3, #32]`, `blx r3`.
    static const unsigned char asks[] = { 0x09, 0x68, 0x05, 0x46, 0xfb, 0x60, 0x07,
                                          0xf1, 0x28, 0x00, 0x07, 0xf1, 0x30, 0x08,
                                          0x0b, 0x68, 0x1b, 0x6a, 0x98, 0x47 };
    // In the MyScript library, which the plugin loads.
    plugin.getPart = find("_ZN8myscript4iink10EditorImpl7getPartEv");
    plugin.isIdle = find("_ZN8myscript4iink10EditorImpl6isIdleEv");
    plugin.editorKnown = plugin.getPart && plugin.isIdle
            && sameCode(find("_ZN17IInkNotePadWidget14setScrollValueEi"), 8, locks, sizeof(locks))
            && sameCode(contentChanged, 16, asks, sizeof(asks));
    return plugin;
}

// True when the MyScript editor shows a page. The notebook editor's
// onContentChanged takes that for granted.
bool hasPage(Editor *editor)
{
    using GetPart = std::shared_ptr<Part> (*)(Editor *);
    const auto *functions = *reinterpret_cast<const GetPart *const *>(editor);
    return functions[partFunction](editor) != nullptr;
}

// True when the MyScript editor's table of virtual functions has getPart and
// isIdle where hasPage and idle call them.
bool sameTable(Editor *editor, const Plugin &plugin)
{
    const auto *functions = *reinterpret_cast<const void *const *const *>(editor);
    return functions[partFunction] == plugin.getPart && functions[idleFunction] == plugin.isIdle;
}

// True when the MyScript editor has nothing left to work through. The
// notebook editor calls its onContentChanged only then.
bool idle(Editor *editor)
{
    using IsIdle = bool (*)(Editor *);
    const auto *functions = *reinterpret_cast<const IsIdle *const *>(editor);
    return functions[idleFunction](editor);
}

QString settingsKey(const QString &file)
{
    QString path = file;
    path.replace('/', '_');
    return QStringLiteral("IInkSettings/") + path;
}

// Sets the page that Nickel opens the notebook in `file` on. Nickel's own
// notebook settings read the notebook's settings, change the page and save
// them the way Nickel saves them when a notebook closes: in its cache, and in
// the file when the value changed. Without them the file alone is changed,
// which Nickel reads only while it has not cached the notebook's settings.
void rememberPage(const QString &file, int page)
{
    const auto &native = api();
    mainthread::BlockingOperation operation("host-notebook-page");
    const void *device = native.settings() ? native.device() : nullptr;
    if (device) {
        // An IInkSettings is 36 bytes in 4.38.23852.
        alignas(8) unsigned char storage[128];
        bool made = false, saved = false;
        try {
            native.openSettings(storage, device, file);
            made = true;
            native.setPartIndex(storage, page);
            // Nickel's save: its cache, and the file and a sync of the disks
            // when the value changed.
            native.saveSettings(storage);
            saved = true;
        } catch (...) {
        }
        if (made)
            native.closeSettings(storage);
        if (saved)
            return;
    }
    QSettings settings(settingsFile, QSettings::IniFormat);
    auto values = settings.value(settingsKey(file)).toMap();
    values.insert("PartIndex", page);
    settings.setValue(settingsKey(file), values);
}

// The text above the page, drawn by the reader's own painter. It lies over
// the top of the editor and takes the pen and finger presses there, so no
// ink lands under it.
class Band : public QWidget
{
public:
    Band(QWidget *parent, const platform::HostNotebookRequest &request)
        : QWidget(parent), height_(request.excerptHeight), paint_(request.paintExcerpt)
    {
        setObjectName("rrNotebookExcerpt");
        // Opaque: Qt then leaves the band out of what the editor repaints, so
        // ink is never drawn over it and it is not redrawn with the ink.
        setAttribute(Qt::WA_OpaquePaintEvent);
        setAttribute(Qt::WA_AcceptTouchEvents);
        hide();
    }

    // Covers the top of `editor` while there is text, and hides otherwise.
    void show(const QString &text, QWidget *editor)
    {
        const QRect page = editor->geometry();
        const int corner = cornerWidth(page);
        if (text != text_ || page.width() != width_ || corner != corner_) {
            text_ = text;
            width_ = page.width();
            corner_ = corner;
            textHeight_ = text.isEmpty() || !height_ || !paint_ || width_ - corner_ <= 0
                    ? 0
                    : height_(text, width_ - corner_);
            update();
        }
        const int height = qBound(0, textHeight_, page.height() / 3);
        if (height <= 0) {
            if (!isHidden())
                hide();
            return;
        }
        const QRect area(page.topLeft(), QSize(page.width(), height));
        if (area != geometry())
            setGeometry(area);
        stackOver(editor);
        if (isHidden())
            QWidget::show();
    }

protected:
    bool event(QEvent *event) override
    {
        switch (event->type()) {
        case QEvent::TouchBegin:
        case QEvent::TouchUpdate:
        case QEvent::TouchEnd:
        case QEvent::TouchCancel:
        case QEvent::MouseButtonPress:
        case QEvent::MouseButtonRelease:
        case QEvent::MouseButtonDblClick:
        case QEvent::MouseMove:
            // A press that starts on the band ends here.
            event->accept();
            return true;
        default:
            return QWidget::event(event);
        }
    }

    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.fillRect(rect(), Qt::white);
        if (paint_)
            paint_(painter, rect(), rect().adjusted(0, 0, -corner_, 0), text_);
    }

private:
    // The width that the screen's own button takes at the band's right end:
    // the button that opens the screen's menu sits in the top right corner of
    // the page, and the text stays clear of it.
    int cornerWidth(const QRect &page) const
    {
        int width = 0;
        for (QObject *child : parentWidget()->children()) {
            if (child == this || !child->isWidgetType() || !child->inherits("TouchLabel"))
                continue;
            const QRect button = static_cast<QWidget *>(child)->geometry();
            if (button.left() > page.center().x() && button.top() >= page.top()
                && button.top() < page.top() + page.height() / 8)
                width = qMax(width, page.right() + 1 - button.left());
        }
        return qBound(0, width, page.width() / 4);
    }

    // Keeps the band directly above the editor, below the screen's toolbar
    // and its other widgets. The screen can move the editor in its stack.
    void stackOver(QWidget *editor)
    {
        const QObjectList siblings = parentWidget()->children();
        const int below = siblings.indexOf(editor);
        QWidget *above = nullptr;
        for (int i = below + 1; i < siblings.size() && !above; ++i) {
            QObject *sibling = siblings.at(i);
            if (sibling != this && sibling->isWidgetType()
                && !static_cast<QWidget *>(sibling)->isWindow())
                above = static_cast<QWidget *>(sibling);
        }
        const int own = siblings.indexOf(this);
        if (own > below && (!above || own < siblings.indexOf(above)))
            return;
        if (above)
            stackUnder(above);
        else
            raise();
    }

    std::function<int(const QString &, int)> height_;
    std::function<void(QPainter &, const QRect &, const QRect &, const QString &)> paint_;
    QString text_;
    int width_ = 0, corner_ = 0, textHeight_ = 0;
};

// Follows one notebook from the request until it has closed: waits for
// Nickel's notebook screen, takes it to the requested page, keeps the text
// above the page and the list of pages current, and shows the reader again.
class Session : public QObject
{
public:
    Session(QWidget *reader, QWidget *before, const platform::HostNotebookRequest &request,
            std::function<void(const platform::HostNotebookResult &)> closed)
        : QObject(qApp), reader_(reader), before_(before), request_(request),
          closed_(std::move(closed))
    {
        poll_.setInterval(150);
        QObject::connect(&poll_, &QTimer::timeout, this, [this] { check(); });
        poll_.start();
    }

    // Deletes `opener` once the notebook screen has arrived.
    void keep(QObject *opener) { opener_ = opener; }

private:
    enum class Stage { Waiting, Finding, Settling, Following, Plain };

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
        if (stage_ == Stage::Waiting) {
            if (top && top != before_) {
                screen_ = top;
                stage_ = Stage::Finding;
                if (opener_)
                    opener_->deleteLater();
                diagnostic("host-notebook", { { "result", "shown" }, { "polls", polls_ } }, false);
                return;
            }
            // The screen arrives a moment after the request. Nickel may
            // first ask in a dialog of its own, as it does before it
            // downloads the notebook files, and open the notebook when that
            // is done. The reader waits while a dialog is open, a few
            // seconds after the last one, and a minute without one.
            if (dialogOpen()) {
                asked_ = true;
                quiet_ = 0;
            } else if (++quiet_ > (asked_ ? 20 : 400)) {
                finish("never-shown");
            }
            return;
        }
        // Sleep hides Nickel's pages without closing them.
        if (NativeLifecycle::suspended())
            return;
        if (!onStack(top)) {
            finish("closed");
            return;
        }
        // Another page of Nickel's lies over the notebook, as its choice of
        // page backgrounds does. The notebook is open under it.
        if (top != screen_)
            return;
        if (stage_ != Stage::Plain && !editor_ && stage_ != Stage::Finding) {
            // The screen replaced its editor; nothing known about it holds.
            plain("editor-gone");
            return;
        }
        if (stage_ == Stage::Finding)
            find();
        else if (stage_ == Stage::Settling)
            settle();
        else if (stage_ == Stage::Following)
            follow();
    }

    static bool dialogOpen()
    {
        for (QWidget *widget : QApplication::topLevelWidgets())
            if (widget->isVisible() && widget->inherits("QDialog"))
                return true;
        return false;
    }

    // Whether the notebook's screen is still one of Nickel's pages. Nickel
    // takes a closed page off its stack of pages at once and deletes it
    // later.
    bool onStack(const QWidget *top) const
    {
        if (!screen_)
            return false;
        if (top == screen_)
            return true;
        const auto *stack = qobject_cast<QStackedWidget *>(screen_->parentWidget());
        return stack && stack->indexOf(screen_.data()) >= 0;
    }

    // The editor's functions report a failure of the engine by throwing, and
    // nothing may be thrown through Qt's timer. A failure reads as no pages.
    int pageNow() const
    {
        try {
            return plugin_.page(editor_);
        } catch (...) {
            return -1;
        }
    }

    int lastNow() const
    {
        try {
            return plugin_.lastPage(editor_);
        } catch (...) {
            return -1;
        }
    }

    // The MyScript editor, kept alive while the result is. Empty when the
    // editor is gone or Nickel's editor is not laid out as checked.
    std::shared_ptr<Editor> lockEditor()
    {
        if (!plugin_.editorKnown || !editor_)
            return {};
        const auto &weak = *reinterpret_cast<const std::weak_ptr<Editor> *>(
                reinterpret_cast<const char *>(editor_.data()) + editorOffset);
        auto editor = weak.lock();
        if (editor && !sameTable(editor.get(), plugin_)) {
            // Another MyScript library: nothing is asked of its editor.
            plugin_.editorKnown = false;
            diagnostic("host-notebook", { { "result", "editor-table-unknown" } }, false);
            return {};
        }
        return editor;
    }

    bool showsPage()
    {
        const auto editor = lockEditor();
        try {
            return editor && hasPage(editor.get());
        } catch (...) {
            return false;
        }
    }

    // Looks for the notebook editor inside the screen and waits until it has
    // read the notebook.
    void find()
    {
        if (!editor_) {
            // Another screen of Nickel's has no editor; stop looking.
            if (++tries_ > 200) {
                plain("no-editor");
                return;
            }
            QList<QWidget *> widgets = screen_->findChildren<QWidget *>();
            widgets.prepend(screen_);
            for (auto *widget : widgets) {
                if (widget->inherits("IInkNotePadWidget")) {
                    editor_ = widget;
                    break;
                }
            }
        }
        if (!editor_)
            return;
        if (!plugin_.valid())
            plugin_ = loadPlugin();
        if (!plugin_.valid()) {
            plain("no-plugin");
            return;
        }
        // The editor learns its kind when it reads the notebook, in one step
        // with its pages.
        if (!plugin_.basic(editor_))
            return;
        // A notebook that the editor could not read shows no page. It is
        // left alone, and nothing is reported about its pages.
        if (plugin_.editorKnown && !showsPage()) {
            if (++waits_ > 20)
                plain("no-page");
            return;
        }
        waits_ = 0;
        opened_ = lastNow() + 1;
        // A page asked for that the notebook does not have is a new page.
        fresh_ = request_.page < 0 || request_.page >= opened_;
        stage_ = Stage::Settling;
        tries_ = 0;
        settle();
    }

    // Takes the notebook to the requested page.
    void settle()
    {
        // Each step that moves is checked by the next; a move that the editor
        // refused is tried again by the next poll.
        for (int step = 0; step < 4; ++step) {
            const int last = lastNow();
            const int at = pageNow();
            if (last < 0 || at < 0 || at > last) {
                plain("bad-pages");
                return;
            }
            const int target = fresh_ ? last : qMin(request_.page, last);
            if (at != target) {
                if (!go(target))
                    return;
                continue;
            }
            if (fresh_ && !grown_) {
                // On the last page. If it has ink, the editor adds the empty
                // page behind it, as it does after a stroke. Like Nickel,
                // this waits until the editor is idle, for a moment at most.
                if (!addPage() && ++waits_ <= 10)
                    return;
                grown_ = true;
                continue;
            }
            settled(target);
            return;
        }
    }

    void settled(int target)
    {
        // The pages as they were when the notebook opened. Pages added since
        // are behind them.
        pages_.clear();
        const int count = lastNow() + 1;
        if (count < 1) {
            plain("bad-pages");
            return;
        }
        for (int i = 0; i < count; ++i)
            pages_.append(i < opened_ ? i : platform::HostNotebookResult::added);
        if (fresh_ && target < pages_.size())
            pages_[target] = platform::HostNotebookResult::requested;
        shown_ = target;
        stage_ = Stage::Following;
        diagnostic("host-notebook",
                   { { "result", "settled" },
                     { "page", target },
                     { "pages", count },
                     { "opened_pages", opened_ },
                     { "asked", request_.page },
                     { "layout_known", plugin_.editorKnown },
                     { "polls", polls_ } },
                   false);
        follow();
    }

    // Goes to a page that exists and is not the page shown. False when the
    // editor did not go there.
    bool go(int page)
    {
        if (++tries_ > 40) {
            plain("stuck");
            return false;
        }
        try {
            plugin_.goTo(editor_, page);
            if (plugin_.moved)
                plugin_.moved(editor_);
        } catch (...) {
            plain("go-failed");
            return false;
        }
        return pageNow() == page;
    }

    // Asks the editor to do what it does after a stroke: when the page shown
    // is the last one and has ink, it adds an empty page behind it. False
    // while the editor is busy, when the call has to wait.
    bool addPage()
    {
        const auto editor = lockEditor();
        try {
            if (!editor || !hasPage(editor.get()))
                return true;
            if (!idle(editor.get()))
                return false;
            plugin_.contentChanged(editor_, editor);
        } catch (...) {
            diagnostic("host-notebook", { { "result", "add-page-failed" } }, false);
        }
        return true;
    }

    // Keeps the list of pages and the text above the page current.
    void follow()
    {
        const int count = lastNow() + 1;
        const int at = pageNow();
        if (count < 1 || at < 0 || at >= count)
            return;
        if (count != pages_.size()) {
            if (count == pages_.size() - 1 && shown_ >= 0 && shown_ < pages_.size()) {
                // Nickel deletes the page that is shown.
                pages_.remove(shown_);
            } else {
                // New pages come behind the last one.
                while (pages_.size() < count)
                    pages_.append(platform::HostNotebookResult::added);
                while (pages_.size() > count)
                    pages_.removeLast();
            }
        }
        shown_ = at;
        const int origin = pages_.value(at, platform::HostNotebookResult::added);
        QString text;
        if (origin == platform::HostNotebookResult::requested)
            text = request_.excerpt;
        else if (origin >= 0)
            text = request_.excerpts.value(origin);
        if (!band_ && !text.isEmpty() && editor_->parentWidget())
            band_ = new Band(editor_->parentWidget(), request_);
        if (band_)
            band_->show(text, editor_);
    }

    // Leaves the notebook as Nickel shows it: no page is chosen and none is
    // reported.
    void plain(const char *reason)
    {
        stage_ = Stage::Plain;
        if (band_)
            band_->hide();
        diagnostic("host-notebook", { { "result", reason }, { "polls", polls_ } }, false);
    }

    void finish(const char *result)
    {
        poll_.stop();
        platform::HostNotebookResult outcome;
        if (stage_ == Stage::Following && !pages_.isEmpty()) {
            outcome.known = true;
            outcome.pages = pages_;
        }
        diagnostic("host-notebook",
                   { { "result", result },
                     { "polls", polls_ },
                     { "pages", pages_.size() },
                     { "known", outcome.known } },
                   false);
        if (band_)
            band_->deleteLater();
        if (reader_) {
            reader_->setProperty("kobalt_native_browser", false);
            reader_->show();
            reader_->raise();
            reader_->activateWindow();
            if (closed_)
                closed_(outcome);
        }
        deleteLater();
    }

    QPointer<QWidget> reader_, before_, screen_, editor_;
    QPointer<QObject> opener_;
    QPointer<Band> band_;
    platform::HostNotebookRequest request_;
    std::function<void(const platform::HostNotebookResult &)> closed_;
    Plugin plugin_;
    QTimer poll_;
    Stage stage_ = Stage::Waiting;
    QVector<int> pages_;
    int polls_ = 0, quiet_ = 0, tries_ = 0, waits_ = 0, opened_ = 0, shown_ = -1;
    bool asked_ = false, fresh_ = false, grown_ = false;
};

bool open(const platform::HostNotebookRequest &request, QWidget *reader,
          std::function<void(const platform::HostNotebookResult &)> closed)
{
    const auto &native = api();
    if (!native.present() || !native.sized() || request.file.isEmpty() || !reader) {
        diagnostic("host-notebook", { { "result", "unavailable" } }, false);
        return false;
    }
    auto *main = native.mainWindow();
    if (!main) {
        diagnostic("host-notebook", { { "result", "no-main-window" } }, false);
        return false;
    }
    OwnVolume volume;
    native.setTitle(volume.get(), QVariant(request.title));
    // Nickel names a notebook by its file: file:// and the path as it is.
    native.setId(volume.get(), QVariant(QStringLiteral("file://") + request.file));
    native.setContentType(volume.get(), notebookContentType);
    native.setNumPages(volume.get(), QVariant(0));
    native.setMimeType(volume.get(), QVariant(*native.rawMime));
    native.setUserId(volume.get(), QVariant(*native.user));
    if (!native.basic(volume.get())) {
        diagnostic("host-notebook", { { "result", "not-basic-volume" } }, false);
        return false;
    }
    // An empty vector has the same layout for any element type.
    const QVector<int> keys;
    QPointer<QObject> opener = native.create(nullptr, volume.get(), &keys, nullptr);
    if (!opener) {
        diagnostic("host-notebook", { { "result", "no-opener" } }, false);
        return false;
    }
    auto *session = new Session(reader, native.currentView(main), request, std::move(closed));
    // The same mark and order as the browser handoff.
    reader->setProperty("kobalt_native_browser", true);
    reader->hide();
    // Nickel opens the notebook on this page. The empty page of a new
    // request is the last one. The reader takes no input from here on.
    const int page = request.page >= 0 ? request.page : qMax(0, request.excerpts.size() - 1);
    rememberPage(request.file, page);
    native.openNotePad(opener, volume.get());
    // Without its parent the opener is the caller's to delete. When the
    // notebook files are missing, Nickel first offers to download them with
    // the opener as the dialog's receiver, and it opens the notebook from
    // there; so the opener is kept until the notebook screen arrives.
    if (opener && !opener->parent())
        session->keep(opener);
    diagnostic("host-notebook",
               { { "result", "opened" }, { "asked", request.page }, { "page", page } }, false);
    return true;
}

const bool registered = [] {
    if (!api().present())
        return false;
    platform::hostNotebooks().open = &open;
    return true;
}();
} // namespace
} // namespace kobalt
