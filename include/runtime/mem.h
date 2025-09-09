/*
 * Minimal aligned memory helpers for DSD-FME runtime
 *
 * Copyright (C) 2025
 *
 * This module centralizes aligned allocations to a single place to make
 * ownership explicit and rollback safe.
 */

#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Default alignment for hot DSP buffers */
#ifndef DSD_FME_ALIGN
#define DSD_FME_ALIGN 64
#endif

/* Allocate size bytes aligned to DSD_FME_ALIGN. Returns NULL on failure. */
void* dsd_fme_aligned_malloc(size_t size);

/* Free memory allocated by dsd_fme_aligned_malloc (or plain malloc fallback). */
void dsd_fme_aligned_free(void* ptr);

#ifdef __cplusplus
}
#endif
