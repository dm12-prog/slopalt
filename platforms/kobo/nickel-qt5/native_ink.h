#pragma once

namespace kobalt::nickel_qt5 {
// Nickel's pen layer is on screen. Its updates carry Nickel's own handwriting
// refresh policy, which the reader's flush must pass on unchanged.
bool nativeInkShown();
} // namespace kobalt::nickel_qt5
