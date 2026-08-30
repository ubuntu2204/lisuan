/*
 * @Copyright   Copyright (c) Lisuan Tech. All Rights Reserved
 * @License     Dual MIT/GPLv2
 */

#ifndef LISUAN_DRM_FOURCC_H
#define LISUAN_DRM_FOURCC_H

#ifdef _WIN32
#include <stdint.h>
typedef uint64_t __u64;
#else
#include "drm/drm_fourcc.h"
#endif

#define DRM_FORMAT_MOD_VENDOR_NONE       0
#define DRM_FORMAT_MOD_VENDOR_LISUAN     0x32
#define DRM_FORMAT_RESERVED	      ((1ULL << 56) - 1)

#ifndef fourcc_mod_code
#define fourcc_mod_code(vendor, val) \
	((((__u64)DRM_FORMAT_MOD_VENDOR_## vendor) << 56) | ((val) & 0x00ffffffffffffffULL))
#endif

#define DRM_FORMAT_MOD_INVALID	fourcc_mod_code(NONE, DRM_FORMAT_RESERVED)
#define DRM_FORMAT_MOD_LINEAR	fourcc_mod_code(NONE, 0)
#define LISUAN_FMT_MOD          fourcc_mod_code(LISUAN, 0)
#define IS_LISUAN_FMT_MOD(val)  (((val) >> 56) == DRM_FORMAT_MOD_VENDOR_LISUAN)

#define LISUAN_DRM_FMT_MOD_TILE_SHIFT       0
#define LISUAN_DRM_FMT_MOD_TILE_MASK        0x1
#define LISUAN_DRM_FMT_MOD_COMPRESS_SHIFT   1
#define LISUAN_DRM_FMT_MOD_COMPRESS_MASK    0x1
#define LISUAN_DRM_FMT_MOD_DISPLAY_SHIFT   2
#define LISUAN_DRM_FMT_MOD_DISPLAY_MASK    0x1


#define LISUAN_DRM_FMT_MOD_SET(field, value) \
	((uint64_t)(value) << LISUAN_DRM_FMT_MOD_##field##_SHIFT)
#define LISUAN_DRM_FMT_MOD_GET(field, value) \
	(((value) >> LISUAN_DRM_FMT_MOD_##field##_SHIFT) & LISUAN_DRM_FMT_MOD_##field##_MASK)


#define LISUAN_DRM_FMT_MOD_LINEAR_COMPRESS    \
        (LISUAN_FMT_MOD |                     \
        LISUAN_DRM_FMT_MOD_SET(TILE, 0) |     \
        LISUAN_DRM_FMT_MOD_SET(COMPRESS, 1) | \
        LISUAN_DRM_FMT_MOD_SET(DISPLAY, 1))

#define LISUAN_DRM_FMT_MOD_TILE_COMPRESS      \
        (LISUAN_FMT_MOD |					  \
        LISUAN_DRM_FMT_MOD_SET(TILE, 1) |	  \
        LISUAN_DRM_FMT_MOD_SET(COMPRESS, 1))

#define LISUAN_DRM_FMT_MOD_TILE_NONECOMPRESS \
        (LISUAN_FMT_MOD |                     \
        LISUAN_DRM_FMT_MOD_SET(TILE, 1) |     \
        LISUAN_DRM_FMT_MOD_SET(COMPRESS, 0))

#define LISUAN_DRM_FMT_MOD_LINEAR_NONECOMPRESS \
        (LISUAN_FMT_MOD |                     \
        LISUAN_DRM_FMT_MOD_SET(TILE, 0) |     \
        LISUAN_DRM_FMT_MOD_SET(COMPRESS, 0) | \
        LISUAN_DRM_FMT_MOD_SET(DISPLAY, 1))


enum modifier_priority {
   MODIFIER_PRIORITY_INVALID = 0,
   MODIFIER_PRIORITY_LINEAR,
   MODIFIER_PRIORITY_LINEAR_NONECOMPRESS,
   MODIFIER_PRIORITY_TILE_NONECOMPRESS,
   MODIFIER_PRIORITY_TILE_COMPRESS,
   MODIFIER_PRIORITY_LINEAR_COMPRESS,
};

static const uint64_t priority_to_modifier[] = {
   DRM_FORMAT_MOD_INVALID,
   DRM_FORMAT_MOD_LINEAR,
   LISUAN_DRM_FMT_MOD_LINEAR_NONECOMPRESS,
   LISUAN_DRM_FMT_MOD_TILE_NONECOMPRESS,
   LISUAN_DRM_FMT_MOD_TILE_COMPRESS,
   LISUAN_DRM_FMT_MOD_LINEAR_COMPRESS,
};

#endif /* LISUAN_DRM_FOURCC_H */

