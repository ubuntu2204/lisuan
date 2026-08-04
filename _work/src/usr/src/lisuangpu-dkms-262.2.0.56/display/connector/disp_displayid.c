/*
 * @Copyright   Copyright (c) Lisuan Tech. All Rights Reserved
 * @License     Dual MIT/GPLv2
 */

#include "disp_displayid.h"
#include "disp_edid.h"

#define disp_print(level, log, ...) disp_print_ex(DISP_PRINT_MASK_DISPLAYID, level, log, ##__VA_ARGS__)

static const displayid_header_t *displayid_get_header(const disp_u8_t *displayid, disp_s32_t length, disp_s32_t index)
{
    const displayid_header_t *base;

    if (sizeof(*base) > length - index)
        return DISP_NULL;

    base = (const displayid_header_t *)&displayid[index];

    return base;
}

static const displayid_header_t *validate_displayid(const disp_u8_t *displayid, disp_s32_t length, disp_s32_t idx)
{
    disp_s32_t dispid_length, index;
    disp_u8_t csum = 0;
    const displayid_header_t *base;

    base = displayid_get_header(displayid, length, idx);
    if (!base)
        return DISP_NULL;

    disp_print(DDM_INFO, "displayid: base rev[0x%x] length[%d], prod[%d] ext_cnt[%d]\n",
              base->rev, base->bytes, base->prod_id, base->ext_count);

    /* +1 for DispID checksum */
    dispid_length = sizeof(*base) + base->bytes + 1;
    if (dispid_length > length - idx)
        return DISP_NULL;

    for (index = 0; index < dispid_length; index++)
        csum += displayid[idx + index];
    if (csum) {
        disp_print(DDM_WARNING, "DisplayID checksum invalid, remainder is %d\n", csum);
        return DISP_NULL;
    }

    return base;
}

static const disp_u8_t *disp_find_displayid_extension(const pedid_t pedid,
    disp_s32_t *length, disp_s32_t *idx, disp_s32_t *ext_index)
{
    const disp_u8_t *displayid = disp_find_edid_extension(pedid, DISPLAYID_EXT, ext_index);
    const displayid_header_t *base;

    if (!displayid)
        return DISP_NULL;

    /* EDID extensions block checksum isn't for us */
    *length = EDID_LENGTH - 1;
    *idx = 1;

    base = validate_displayid(displayid, *length, *idx);
    if (!base)
        return DISP_NULL;

    *length = *idx + sizeof(*base) + base->bytes;
    return displayid;
}

disp_void_t displayid_iter_edid_begin(const disp_edid_t *disp_edid, displayid_iter_t *iter)
{
    disp_memset(iter, 0, sizeof(*iter));

    iter->disp_edid = disp_edid;
}

static const displayid_block_t *displayid_iter_block(const displayid_iter_t *iter)
{
    const displayid_block_t *block;

    if (!iter->section)
        return DISP_NULL;

    block = (const displayid_block_t *)&iter->section[iter->idx];

    if (iter->idx + sizeof(*block) <= iter->length &&
        iter->idx + sizeof(*block) + block->num_bytes <= iter->length)
    {
        return block;
    }

    return DISP_NULL;
}

const displayid_block_t *displayid_iter_next(displayid_iter_t *iter)
{
    const displayid_block_t *block;
    disp_u8_t base_section = DISP_FALSE;

    if (!iter->disp_edid)
        return DISP_NULL;

    if (iter->section) {
        /* current block should always be valid */
        block = displayid_iter_block(iter);
        if (!block) {
            iter->section = DISP_NULL;
            iter->disp_edid = DISP_NULL;
            return DISP_NULL;
        }

        /* next block in section */
        iter->idx += sizeof(*block) + block->num_bytes;

        block = displayid_iter_block(iter);
        if (block)
            return block;
    }

    for (;;) {
        /* The first section we encounter is the base section */
        base_section = !iter->section;

        iter->section = disp_find_displayid_extension((const pedid_t)iter->disp_edid->edid,
                                 &iter->length,
                                 &iter->idx,
                                 &iter->ext_index);
        if (!iter->section) {
            iter->disp_edid = DISP_NULL;
            return DISP_NULL;
        }

        /* Save the structure version and primary use case. */
        if (base_section) {
            const displayid_header_t *base;

            base = displayid_get_header(iter->section, iter->length, iter->idx);
            if (!base) {
                iter->version = base->rev;
                iter->primary_use = base->prod_id;
            }
        }

        iter->idx += sizeof(displayid_header_t);

        block = displayid_iter_block(iter);
        if (block)
            return block;
    }
}

disp_void_t displayid_iter_end(displayid_iter_t *iter)
{
    disp_memset(iter, 0, sizeof(*iter));
}

/* DisplayID Structure Version/Revision from the Base Section. */
disp_u8_t displayid_version(const displayid_iter_t *iter)
{
    return iter->version;
}

/*
 * DisplayID Primary Use Case (2.0+) or Product Type Identifier (1.0-1.3) from
 * the Base Section.
 */
disp_u8_t displayid_primary_use(const displayid_iter_t *iter)
{
    return iter->primary_use;
}

