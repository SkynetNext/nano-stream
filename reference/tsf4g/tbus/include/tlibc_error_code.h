#ifndef TSF4G_TLIBC_ERROR_CODE_H
#define TSF4G_TLIBC_ERROR_CODE_H

// Minimal compatibility header for the extracted TSF4G tbus code.
// The original project depends on "TLibC"; this repo only needs a few macros/types
// for the `tbus` snippet to compile standalone.

#include <stddef.h> // offsetof

#ifndef TLIBC_OFFSET_OF
#define TLIBC_OFFSET_OF(type, member) ((size_t)offsetof(type, member))
#endif

#endif // TSF4G_TLIBC_ERROR_CODE_H


