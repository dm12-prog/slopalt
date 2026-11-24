#include "host_notebook.h"

namespace kobalt::platform {
HostNotebooks &hostNotebooks()
{
    static HostNotebooks notebooks;
    return notebooks;
}
} // namespace kobalt::platform
