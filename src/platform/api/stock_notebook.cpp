#include "stock_notebook.h"

namespace kobalt::platform {
StockNotebooks &stockNotebooks()
{
    static StockNotebooks notebooks;
    return notebooks;
}
} // namespace kobalt::platform
