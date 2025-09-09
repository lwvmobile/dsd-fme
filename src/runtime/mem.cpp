/*
 * Minimal aligned memory helpers for DSD-FME runtime
 */

#include <cstdlib>

#include "runtime/mem.h"

void*
dsd_fme_aligned_malloc(size_t size) {
    if (size == 0) {
        return NULL;
    }
#if defined(_POSIX_C_SOURCE) && (_POSIX_C_SOURCE >= 200112L)
    void* mem_ptr = NULL;
    if (posix_memalign(&mem_ptr, DSD_FME_ALIGN, size) != 0) {
        mem_ptr = std::malloc(size);
    }
    return mem_ptr;
#else
    return std::malloc(size);
#endif
}

void
dsd_fme_aligned_free(void* ptr) {
    std::free(ptr);
}
