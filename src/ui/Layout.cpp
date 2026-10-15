#include "ui/Layout.h"

namespace kb
{
Layout& layout()
{
    static Layout instance;
    return instance;
}
} // namespace kb
