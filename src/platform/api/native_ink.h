#pragma once
#include <QByteArray>
#include <QRect>
#include <QString>

class QWidget;

namespace kobalt::platform {
// The host's own handwriting layer, where it has one: a transparent widget
// that draws pen strokes itself with the host's fast pen refresh, pressure and
// eraser. A notebook page puts it over its paper instead of drawing strokes.
// Every function is null where the host has no such layer.
struct NativeInk
{
    // Creates the layer as a child of `parent`, covering `area`, and shows it.
    // Touches that are not the pen go to the child of `backing` under them.
    // `parent` owns the layer. Null when it cannot be made.
    QWidget *(*create)(QWidget *parent, QWidget *backing, const QRect &area) = nullptr;
    // The strokes inside `area` of the layer, as an SVG document.
    QByteArray (*svg)(QWidget *layer, const QRect &area) = nullptr;
    // Replaces the strokes with those of the SVG file `file`; empty clears.
    void (*load)(QWidget *layer, const QString &file) = nullptr;
    void (*undo)(QWidget *layer) = nullptr;
    void (*redo)(QWidget *layer) = nullptr;
    // Leaves the fast pen refresh. Call before hiding or destroying the layer.
    void (*finish)(QWidget *layer) = nullptr;

    bool available() const { return create && svg && load && undo && redo && finish; }
};

NativeInk &nativeInk();
} // namespace kobalt::platform
