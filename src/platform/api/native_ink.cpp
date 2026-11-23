#include "native_ink.h"

namespace kobalt::platform {
NativeInk &nativeInk()
{
    static NativeInk ink;
    return ink;
}
} // namespace kobalt::platform
