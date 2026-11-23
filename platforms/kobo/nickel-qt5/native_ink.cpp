#include "native_ink.h"
#include "platform/api/native_ink.h"
#include "platforms/kobo/common/native_symbol.h"
#include "support/diagnostics.h"
#include <QPointer>
#include <QTransform>
#include <QWidget>
#include <new>

// Nickel's SketchNote is the pen layer it puts over a book page for stylus
// markup: a transparent QWidget that turns pen touches into strokes, switches
// the screen to its handwriting refresh while the pen is down, erases with
// the stylus eraser, and keeps undo and redo. It saves with toSvg and loads
// with fromSvg, which wants the path of a file that ends in ".svg".
// Touches without the pen go to the child of its backing widget under them;
// where the backing widget has no child, a finger draws too.
// Symbols and the object size (296 bytes, from the operator new in
// Ui_ReadingView::setupUi) checked in 4.38.23852.
namespace kobalt {
namespace {
struct Api
{
    void (*construct)(void *, QWidget *, int) =
            symbol<decltype(construct)>("_ZN10SketchNoteC1EP7QWidget6QFlagsIN2Qt10WindowTypeEE");
    void (*setBacking)(QWidget *, QWidget *) =
            symbol<decltype(setBacking)>("_ZN10SketchNote16setBackingWidgetEP7QWidget");
    void (*setPressure)(QWidget *, bool) =
            symbol<decltype(setPressure)>("_ZN10SketchNote18setPressureEnabledEb");
    QByteArray (*toSvg)(QWidget *, const QRect &) =
            symbol<decltype(toSvg)>("_ZN10SketchNote5toSvgERK5QRect");
    void (*fromSvg)(QWidget *, const QString &, const QTransform &, const QRect &, bool,
                    const QRect &) =
            symbol<decltype(fromSvg)>("_ZN10SketchNote7fromSvgERK7QStringRK10QTransformRK5QRectbS8_");
    void (*renderAll)(QWidget *) = symbol<decltype(renderAll)>("_ZN10SketchNote14renderAllItemsEv");
    void (*clear)(QWidget *) = symbol<decltype(clear)>("_ZN10SketchNote5clearEv");
    void (*undo)(QWidget *) = symbol<decltype(undo)>("_ZN10SketchNote4undoEv");
    void (*redo)(QWidget *) = symbol<decltype(redo)>("_ZN10SketchNote4redoEv");
    void (*exitHandwriting)(QWidget *) =
            symbol<decltype(exitHandwriting)>("_ZN10SketchNote15exitHandwritingEv");

    bool valid() const
    {
        return construct && setBacking && setPressure && toSvg && fromSvg && renderAll && clear
                && undo && redo && exitHandwriting;
    }
};

const Api &api()
{
    static const Api value;
    return value;
}

// Room for the object, with spare in case another 4.x build made it larger.
constexpr std::size_t layerBytes = 1024;
QPointer<QWidget> liveLayer;

QWidget *create(QWidget *parent, QWidget *backing, const QRect &area)
{
    const auto &native = api();
    if (!native.valid() || !parent || !backing)
        return nullptr;
    // Nickel's deleting destructor frees this with operator delete.
    void *memory = ::operator new(layerBytes);
    native.construct(memory, parent, 0);
    auto *layer = static_cast<QWidget *>(memory);
    layer->setObjectName("rrNativeInk");
    native.setBacking(layer, backing);
    native.setPressure(layer, true);
    layer->setGeometry(area);
    layer->show();
    layer->raise();
    liveLayer = layer;
    diagnostic("native-ink", { { "result", "created" }, { "width", area.width() },
                               { "height", area.height() } },
               false);
    return layer;
}

QByteArray svg(QWidget *layer, const QRect &area)
{
    return layer ? api().toSvg(layer, area) : QByteArray();
}

void load(QWidget *layer, const QString &file)
{
    if (!layer)
        return;
    const auto &native = api();
    native.clear(layer);
    if (!file.isEmpty()) {
        native.fromSvg(layer, file, QTransform(), layer->rect(), false, QRect());
        native.renderAll(layer);
    }
    layer->update();
}

void undo(QWidget *layer)
{
    if (layer)
        api().undo(layer);
}

void redo(QWidget *layer)
{
    if (layer)
        api().redo(layer);
}

void finish(QWidget *layer)
{
    if (layer)
        api().exitHandwriting(layer);
}

const bool registered = [] {
    if (!api().valid())
        return false;
    auto &ink = platform::nativeInk();
    ink.create = &create;
    ink.svg = &svg;
    ink.load = &load;
    ink.undo = &undo;
    ink.redo = &redo;
    ink.finish = &finish;
    return true;
}();
} // namespace

namespace nickel_qt5 {
bool nativeInkShown()
{
    return liveLayer && liveLayer->isVisible();
}
} // namespace nickel_qt5
} // namespace kobalt
