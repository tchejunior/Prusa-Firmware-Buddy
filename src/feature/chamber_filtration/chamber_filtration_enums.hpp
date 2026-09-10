#pragma once

#include <cstdint>

#include <option/xl_enclosure_support.h>
#include <option/has_xbuddy_extension.h>

namespace buddy {

// !!! DO NOT REMOVE ITEMS/CHANGE ITEM IDs, USED IN CONFIG STORE
enum class ChamberFiltrationBackend : uint8_t {
    none = 0,
#if XL_ENCLOSURE_SUPPORT()
    xl_enclosure = 1,
#endif
#if HAS_XBUDDY_EXTENSION()
    /// XBE Filtration fan is doing both cooling and filtration
    xbe_official_filter = 2,

    /// XBE Cooling fans are doing both cooling and filtration
    xbe_filter_on_cooling_fans = 3,

    /// Rear fans exhaust heat; the dedicated filtration fan recirculates air.
    xbe_custom_filter = 4,
#endif
};

} // namespace buddy
