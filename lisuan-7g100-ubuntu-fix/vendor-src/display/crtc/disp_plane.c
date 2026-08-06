/*
 * @Copyright   Copyright (c) Lisuan Tech. All Rights Reserved
 * @License     Dual MIT/GPLv2
 */

#include "disp_plane.h"
#include "disp_device.h"
#include "disp_pipe.h"
#include "disp_reg.h"
#include "PLNS_register.h"
#include "disp_hdr.h"

#define plane_write_field(crtc_index, plane_index, reg_name, reg_start, num, ...) \
{ \
    reg32_write_field(reg_name, (reg_start * 4 + pdd->reg_off.plns[crtc_index] + pdd->reg_off.layer_plns[plane_index]), num, __VA_ARGS__) \
}

#define plane_read_field(crtc_index, plane_index, reg_name, reg_start, num, ...) \
{ \
    reg32_read_field(reg_name, (reg_start * 4 + pdd->reg_off.plns[crtc_index] + pdd->reg_off.layer_plns[plane_index]), num, __VA_ARGS__) \
}

#define disp_print(level, log, ...) disp_print_ex(DISP_PRINT_MASK_PLANE, level, log, ##__VA_ARGS__)

disp_void_t disp_rect_dump(pdisp_rect_t disp_rect)
{
    disp_print(DDM_TRACE, "\t%d %d %d %d\n", disp_rect->x, disp_rect->y, disp_rect->w, disp_rect->h);
}

disp_void_t disp_fb_dump(pdisp_fb_t pdisp_fb)
{
    disp_print(DDM_TRACE, "\taddr: 0x%x, offset: 0x%x, format: %d, bit_cnt: %d, pitch = %d\n",
        pdisp_fb->address, pdisp_fb->offset, pdisp_fb->format, pdisp_fb->bit_cnt, pdisp_fb->pitch);
    disp_print(DDM_TRACE, "\twidth: %d, height: %d, align_width: %d\n", pdisp_fb->width, pdisp_fb->height, pdisp_fb->align_width);
    disp_print(DDM_TRACE, "\tcompress: %d, range_type: %d, bl_index = %d\n", pdisp_fb->flag.compress, pdisp_fb->range_type, pdisp_fb->bl_index);
    disp_print(DDM_TRACE, "\tcolor sapce: 0x%x, corlor range: 0x%x\n", pdisp_fb->color_space, pdisp_fb->color_range);
}

disp_void_t disp_csc_dump(pdisp_csc_t pdisp_csc)
{
    disp_print(DDM_TRACE, "\tcsc\n");
    disp_print(DDM_TRACE, "\tinput format: %d, output format: %d\n",
        pdisp_csc->input_fmt, pdisp_csc->output_fmt);
    disp_print(DDM_TRACE, "\tbright: %d, prog: %d, bypass: %d\n", pdisp_csc->brightness, pdisp_csc->prog, pdisp_csc->bypss);
    disp_print(DDM_TRACE, "\tmatrix:[%u, %u, %u; %u, %u, %u; %u, %u, %u]\n", pdisp_csc->coef1, pdisp_csc->coef2, pdisp_csc->coef3,
        pdisp_csc->coef4, pdisp_csc->coef5, pdisp_csc->coef6, pdisp_csc->coef7, pdisp_csc->coef8, pdisp_csc->coef9);
}

disp_void_t disp_blend_dump(pdisp_blend_info_t pdisp_blend_info)
{
    disp_print(DDM_TRACE, "\tblend info\n");
    disp_print(DDM_TRACE, "\tblend mode: %d\n", pdisp_blend_info->blend_mode);
    disp_print(DDM_TRACE, "\tkp_kmix: %d, ks_kt: %d, key_value: 0x%x\n", pdisp_blend_info->factor.kp_kmix, pdisp_blend_info->factor.ks_kt, pdisp_blend_info->key_value);
}

disp_void_t disp_plane_dump(pdisp_plane_t pdisp_plane)
{
    if(pdisp_plane->plane_index == DISP_CURSOR_PLANE)
        return;

    disp_print(DDM_TRACE, "plane: %d(%d), flip mode: %d\n", pdisp_plane->plane_index, pdisp_plane->crtc_index, pdisp_plane->flip_mode);
    disp_rect_dump(&pdisp_plane->src);
    disp_rect_dump(&pdisp_plane->dst);
    disp_fb_dump(&pdisp_plane->fb);
    disp_csc_dump(&pdisp_plane->csc);
    disp_blend_dump(&pdisp_plane->blend_info);
    if(pdisp_plane->flag.bits.flush_hdr_module)
    {
        disp_hdr_module_dump(&pdisp_plane->hdr_module);
    }
    if(pdisp_plane->plane_index == DISP_CURSOR_PLANE)
    {
        disp_print(DDM_TRACE, "\t\tcursor type: %d\n", pdisp_plane->cursor_type);
    }
}

DISP_STATUS disp_plane_sw_init(pdisp_plane_t pdisp_plane)
{
    disp_print(DDM_TRACE, "tofill: %#x\n", pdisp_plane);
    return DISP_OK;
}

DISP_STATUS disp_plane_hw_init(pdisp_plane_t pdisp_plane)
{
    pdisp_device_t pdd = (pdisp_device_t)pdisp_plane->disp_device;
    disp_print(DDM_TRACE, "tofill: pdd[%#x]\n", pdd);

    return DISP_OK;
}

DISP_STATUS disp_plane_hw_deinit(pdisp_plane_t pdisp_plane)
{
    pdisp_device_t pdd = (pdisp_device_t)pdisp_plane->disp_device;
    disp_print(DDM_TRACE, "tofill: pdd[%#x]\n", pdd);

    return DISP_OK;
}

DISP_STATUS disp_plane_check(pdisp_plane_t pdisp_plane, pdisp_plane_update_para_t pupdate_para)
{
    disp_print(DDM_TRACE, "tofill: %#x, %#x\n", pdisp_plane, pupdate_para);
    //check the pupdate_para
    return DISP_OK;
}

static disp_void_t plane_generate(pdisp_plane_t pdisp_plane, pdisp_plane_update_para_t pupdate_para)
{
    //here the plane check has been success
    pdisp_plane->flip_mode = pupdate_para->flip_mode;
    disp_memcpy(&pdisp_plane->src, &pupdate_para->src, sizeof(disp_rect_t));
    disp_memcpy(&pdisp_plane->dst, &pupdate_para->dst, sizeof(disp_rect_t));
    disp_memcpy(&pdisp_plane->fb, &pupdate_para->fb, sizeof(disp_fb_t));
    disp_memcpy(&pdisp_plane->blend_info, &pupdate_para->blend, sizeof(disp_blend_info_t));
    pdisp_plane->cursor_type = pupdate_para->cs_type;
    pdisp_plane->mono_bg_color = pupdate_para->mono_bg;
    pdisp_plane->mono_fg_color = pupdate_para->mono_fg;
    disp_hdr_check(pdisp_plane, &pupdate_para->hdr_module, pupdate_para->flag.valid.hdr_module ? 1 : 0);
}

static disp_void_t plane_flush(pdisp_plane_t pdisp_plane)
{
    pdisp_device_t pdd = (pdisp_device_t)pdisp_plane->disp_device;
    disp_pus_t pus = {0};
    disp_u8_t dpe_format = 0;
    disp_u8_t dpe_format_swap = 0;//swap r b or cr y
    disp_u8_t dpe_csc_in_format = 0;
    disp_u32_t pixel_offset = 0;
    disp_u32_t epl_w, epl_h, dst_w, dst_h;
    disp_u64_t temp = 0;
    disp_u8_t plane_csc_bypass;
    
    switch(pdisp_plane->fb.format) {
        case DISP_FB_FMT_R5G6B5:
            dpe_format = PLNS_MMIO_10_EPL_FMT_RGB565;
            dpe_format_swap = PLNS_MMIO_10_EPL_DAT_ORDER_NORMAL;
            dpe_csc_in_format = PLNS_MMIO_17_EPL_CSC_IN_FMT_RGB_FULL;
            break;
        case DISP_FB_FMT_A8R8G8B8:
            dpe_format = PLNS_MMIO_10_EPL_FMT_ARGB8888;
            dpe_format_swap = PLNS_MMIO_10_EPL_DAT_ORDER_NORMAL;
            dpe_csc_in_format = PLNS_MMIO_17_EPL_CSC_IN_FMT_RGB_FULL;
            break;
        case DISP_FB_FMT_X8R8G8B8:
            dpe_format = PLNS_MMIO_10_EPL_FMT_ARGB8888;
            dpe_format_swap = PLNS_MMIO_10_EPL_DAT_ORDER_NORMAL;
            dpe_csc_in_format = PLNS_MMIO_17_EPL_CSC_IN_FMT_RGB_FULL;
            break;
        case DISP_FB_FMT_A8B8G8R8:
            dpe_format = PLNS_MMIO_10_EPL_FMT_ARGB8888;
            dpe_format_swap = PLNS_MMIO_10_EPL_DAT_ORDER_REVERSE;
            dpe_csc_in_format = PLNS_MMIO_17_EPL_CSC_IN_FMT_RGB_FULL;
            break;
        case DISP_FB_FMT_X8B8G8R8:
            dpe_format = PLNS_MMIO_10_EPL_FMT_ARGB8888;
            dpe_format_swap = PLNS_MMIO_10_EPL_DAT_ORDER_REVERSE;
            dpe_csc_in_format = PLNS_MMIO_17_EPL_CSC_IN_FMT_RGB_FULL;
            break;
        case DISP_FB_FMT_A2R10G10B10:
            dpe_format = PLNS_MMIO_10_EPL_FMT_ARGB2101010;
            dpe_format_swap = PLNS_MMIO_10_EPL_DAT_ORDER_NORMAL;
            dpe_csc_in_format = PLNS_MMIO_17_EPL_CSC_IN_FMT_RGB_FULL;
            break;
        case DISP_FB_FMT_X2R10G10B10:
            dpe_format = PLNS_MMIO_10_EPL_FMT_ARGB2101010;
            dpe_format_swap = PLNS_MMIO_10_EPL_DAT_ORDER_NORMAL;
            dpe_csc_in_format = PLNS_MMIO_17_EPL_CSC_IN_FMT_RGB_FULL;
            break;
        case DISP_FB_FMT_A2B10G10R10:
            dpe_format = PLNS_MMIO_10_EPL_FMT_ARGB2101010;
            dpe_format_swap = PLNS_MMIO_10_EPL_DAT_ORDER_REVERSE;
            dpe_csc_in_format = PLNS_MMIO_17_EPL_CSC_IN_FMT_RGB_FULL;
            break;
        case DISP_FB_FMT_X2B10G10R10:
            dpe_format = PLNS_MMIO_10_EPL_FMT_ARGB2101010;
            dpe_format_swap = PLNS_MMIO_10_EPL_DAT_ORDER_REVERSE;
            dpe_csc_in_format = PLNS_MMIO_17_EPL_CSC_IN_FMT_RGB_FULL;
            break;
        case DISP_FB_FMT_YCBCR422_16BIT:
            dpe_format = PLNS_MMIO_10_EPL_FMT_YUV422_8BPC;
            dpe_format_swap = PLNS_MMIO_10_EPL_DAT_ORDER_REVERSE;
            dpe_csc_in_format = PLNS_MMIO_17_EPL_CSC_IN_FMT_YUV709;
            break;
        case DISP_FB_FMT_CRYCB422_16BIT:
            dpe_format = PLNS_MMIO_10_EPL_FMT_YUV422_8BPC;
            dpe_format_swap = PLNS_MMIO_10_EPL_DAT_ORDER_NORMAL;
            dpe_csc_in_format = PLNS_MMIO_17_EPL_CSC_IN_FMT_YUV709;
            break;
        case DISP_FB_FMT_YCBCR422_20BIT:
        case DISP_FB_FMT_YCBCR422_24BIT:
        case DISP_FB_FMT_YCBCR422_32BIT:
            dpe_format = PLNS_MMIO_10_EPL_FMT_YUV422_10;
            dpe_format_swap = PLNS_MMIO_10_EPL_DAT_ORDER_REVERSE;
            dpe_csc_in_format = PLNS_MMIO_17_EPL_CSC_IN_FMT_YUV709;
            break;
        case DISP_FB_FMT_CRYCB422_20BIT:
        case DISP_FB_FMT_CRYCB422_24BIT:
        case DISP_FB_FMT_CRYCB422_32BIT:
            dpe_format = PLNS_MMIO_10_EPL_FMT_YUV422_10;
            dpe_format_swap = PLNS_MMIO_10_EPL_DAT_ORDER_NORMAL;
            dpe_csc_in_format = PLNS_MMIO_17_EPL_CSC_IN_FMT_YUV709;
            break;
        case DISP_FB_FMT_YCBCR8888_32BIT:
            dpe_format = PLNS_MMIO_10_EPL_FMT_YUV444_8;
            dpe_format_swap = PLNS_MMIO_10_EPL_DAT_ORDER_NORMAL;
            dpe_csc_in_format = PLNS_MMIO_17_EPL_CSC_IN_FMT_YUV709;
            break;
        case DISP_FB_FMT_CRYCB8888_32BIT:
            dpe_format = PLNS_MMIO_10_EPL_FMT_YUV444_8;
            dpe_format_swap = PLNS_MMIO_10_EPL_DAT_ORDER_REVERSE;
            dpe_csc_in_format = PLNS_MMIO_17_EPL_CSC_IN_FMT_YUV709;
            break;
        case DISP_FB_FMT_YCBCR2101010_32BIT:
            dpe_format = PLNS_MMIO_10_EPL_FMT_YUV444_10;
            dpe_format_swap = PLNS_MMIO_10_EPL_DAT_ORDER_NORMAL;
            dpe_csc_in_format = PLNS_MMIO_17_EPL_CSC_IN_FMT_YUV709;
            break;
        case DISP_FB_FMT_CRYCB2101010_32BIT:
            dpe_format = PLNS_MMIO_10_EPL_FMT_YUV444_10;
            dpe_format_swap = PLNS_MMIO_10_EPL_DAT_ORDER_REVERSE;
            dpe_csc_in_format = PLNS_MMIO_17_EPL_CSC_IN_FMT_YUV709;
            break;
        case DISP_FB_FMT_A16R16G16B16_FLOAT:
            dpe_format = PLNS_MMIO_10_EPL_FMT_FP16;
            dpe_format_swap = PLNS_MMIO_10_EPL_DAT_ORDER_NORMAL;
            dpe_csc_in_format = PLNS_MMIO_17_EPL_CSC_IN_FMT_RGB_FULL;
            break;
        case DISP_FB_FMT_A16B16G16R16_FLOAT:
            dpe_format = PLNS_MMIO_10_EPL_FMT_FP16;
            dpe_format_swap = PLNS_MMIO_10_EPL_DAT_ORDER_REVERSE;
            dpe_csc_in_format = PLNS_MMIO_17_EPL_CSC_IN_FMT_RGB_FULL;
            break;
        default:
            disp_print(DDM_WARNING, "unhandled format, check again\n");
            break;
    }

    if(pdisp_plane->flip_mode == DISP_FLIP_DISABLE)
    {
        if(pdisp_plane->plane_index == DISP_PRIMARY_PLANE)
        {
            crtc_write_field(pdisp_plane->crtc_index, Reg_Plns_Mmio_0, Register_Plns_Mmio_0_Start, 1,
                Epl0_Stop_Req, 1);
        }
        else if(pdisp_plane->plane_index == DISP_OVERLAY_PLANE)
        {
            crtc_write_field(pdisp_plane->crtc_index, Reg_Plns_Mmio_0, Register_Plns_Mmio_0_Start, 1,
                Epl1_Stop_Req, 1);
        }
        else
        {
            crtc_write_field(pdisp_plane->crtc_index, Reg_Plns_Mmio_0, Register_Plns_Mmio_0_Start, 1,
                Cs_Stop_Req, 1);
            crtc_write_field(pdisp_plane->crtc_index, Reg_Plns_Mmio_9, Register_Plns_Mmio_9_Start, 1,
                Cs_En, 0);
        }

        if (pdisp_plane->plane_index == DISP_PRIMARY_PLANE || pdisp_plane->plane_index == DISP_OVERLAY_PLANE)
        {
            plane_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_10, Register_Plns_Mmio_10_Start, 1,
                Epl_En, PLNS_MMIO_10_EPL_EN_DISABLE);
        }
    }
    else
    {
        if(pdisp_plane->plane_index == DISP_PRIMARY_PLANE)
        {
            crtc_write_field(pdisp_plane->crtc_index, Reg_Plns_Mmio_0, Register_Plns_Mmio_0_Start, 1,
                Epl0_Stop_Req, 0);
        }
        else if(pdisp_plane->plane_index == DISP_OVERLAY_PLANE)
        {
            crtc_write_field(pdisp_plane->crtc_index, Reg_Plns_Mmio_0, Register_Plns_Mmio_0_Start, 1,
                Epl1_Stop_Req, 0);
        }
        else
        {
            crtc_write_field(pdisp_plane->crtc_index, Reg_Plns_Mmio_0, Register_Plns_Mmio_0_Start, 1,
                Cs_Stop_Req, 0);
        }

        if(pdisp_plane->plane_index == DISP_PRIMARY_PLANE || pdisp_plane->plane_index == DISP_OVERLAY_PLANE) 
        {
            /* HW not support X truncate
             * Support Y truncate(vde do truncate), but compress use little will cause cl not consume finish.
             * So, truncate both X Y.
             */
            dst_w = pdisp_plane->dst.w;
            dst_h = pdisp_plane->dst.h;
            if (dst_w + pdisp_plane->dst.x > pdd->disp_crtc[pdisp_plane->crtc_index].pipe.tg.hde)
            {
                dst_w = pdd->disp_crtc[pdisp_plane->crtc_index].pipe.tg.hde - pdisp_plane->dst.x;
            }
            if (dst_h + pdisp_plane->dst.y > pdd->disp_crtc[pdisp_plane->crtc_index].pipe.tg.vde)
            {
                dst_h = pdd->disp_crtc[pdisp_plane->crtc_index].pipe.tg.vde - pdisp_plane->dst.y;
            }

            //set pu
            pus.pus_ratio_mode = PLNS_MMIO_4_HW_RATIO_SW_RATIO;
            pus.pus_cos2tap = PLNS_MMIO_4_HCOSTAP_COSINE;
            pus.pus_ratio_plus = PLNS_MMIO_4_RATIO_PLUS_ORG;
            pus.pus_vsync_dlyval = PLNS_MMIO_4_VDUP_INTERP;

            if((pdisp_plane->src.w != pdisp_plane->dst.w) && (pdisp_plane->dst.w != 0))
            {
                pus.pus_hen = PLNS_MMIO_4_H_EN_ENABLE;
                temp = (disp_u64_t)0x400000 * pdisp_plane->src.w;
                pus.pus_hacc = (temp / pdisp_plane->dst.w) & 0x3FFFFF;
            }
            else
            {
                pus.pus_hen = PLNS_MMIO_4_H_EN_DISABLE;
                pus.pus_hacc = 0x400000;
            }

            if((pdisp_plane->src.h != pdisp_plane->dst.h) && (pdisp_plane->dst.h != 0))
            {
                pus.pus_ven = PLNS_MMIO_4_V_EN_ENABLE;
                temp = (disp_u64_t)0x400000 * pdisp_plane->src.h;
                pus.pus_vacc = (temp / pdisp_plane->dst.h) & 0x3FFFFF;
            }
            else
            {
                pus.pus_ven = PLNS_MMIO_4_V_EN_DISABLE;
                pus.pus_vacc = 0x400000;
            }

            pus.pus_en = pus.pus_hen | pus.pus_ven;

            if(pus.pus_en) 
            {
                pus.pus_width_dst = dst_w;
                pus.pus_height_dst = dst_h;
                epl_w = pdisp_plane->src.w * dst_w / pdisp_plane->dst.w; // truncate too
                epl_h = pdisp_plane->src.h * dst_h / pdisp_plane->dst.h;

                if(pdisp_plane->src.w > pdisp_plane->dst.w ||
                    pdisp_plane->src.h > pdisp_plane->dst.h) 
                    disp_print(DDM_WARNING, "src window > dst window, not support\n");
            } 
            else 
            {//if scaler disable, no conception of dst window
                pus.pus_width_dst = 0;
                pus.pus_height_dst = 0;

                epl_w = dst_w; // Same w/h for src & dst
                epl_h = dst_h;
            }

            crtc_write_field(pdisp_plane->crtc_index, Reg_Plns_Mmio_4, Register_Plns_Mmio_4_Start, 10, 
                V_En, pus.pus_ven,
                H_En, pus.pus_hen,
                Scl_Src_Sel, (pdisp_plane->plane_index == DISP_PRIMARY_PLANE) ? PLNS_MMIO_4_SCL_SRC_SEL_SCL_4EPL0 : PLNS_MMIO_4_SCL_SRC_SEL_SCL_4EPL1,
                Vdup, pus.pus_vsync_dlyval,
                Hcostap, pus.pus_cos2tap,
                Vcostap, pus.pus_cos2tap,
                Hw_Ratio, pus.pus_ratio_mode,
                Ratio_Plus, pus.pus_ratio_plus,
                Epl0_Share_Lb, PLNS_MMIO_4_EPL0_SHARE_LB_NOT_SHARE,
                Epl1_Share_Lb, PLNS_MMIO_4_EPL1_SHARE_LB_NOT_SHARE);

            crtc_write_field(pdisp_plane->crtc_index, Reg_Plns_Mmio_5, Register_Plns_Mmio_5_Start, 1,
                V_Ratio, (disp_u32_t)pus.pus_vacc);
            crtc_write_field(pdisp_plane->crtc_index, Reg_Plns_Mmio_6, Register_Plns_Mmio_6_Start, 1,
                H_Ratio, (disp_u32_t)pus.pus_hacc);

            crtc_write_field(pdisp_plane->crtc_index, Reg_Plns_Mmio_7, Register_Plns_Mmio_7_Start, 2,
                Dst_Ht, pus.pus_height_dst,
                Dst_Wd, pus.pus_width_dst);

            pdisp_plane->fb.offset = (disp_u64_t)pdisp_plane->src.y * pdisp_plane->fb.pitch
                + (disp_u64_t)pdisp_plane->src.x * pdisp_plane->fb.bit_cnt / 8;

            pixel_offset = (disp_u32_t)(pdisp_plane->fb.offset * 8 / pdisp_plane->fb.bit_cnt);

            pdisp_plane->fb.offset &= ~0x1f;

            plane_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_10, Register_Plns_Mmio_10_Start, 11,
                Epl_En, PLNS_MMIO_10_EPL_EN_ENABLE,
                Epl_Rlen, PLNS_MMIO_10_EPL_RLEN_R2K,
                Epl_Req_Pre4or8, PLNS_MMIO_10_EPL_REQ_PRE4OR8_PRE8,
                Epl_Fmt, dpe_format,
                Epl_Dat_Order, dpe_format_swap,
                Epl_Pxl_Off, pixel_offset,
                Epl_Stride, pdisp_plane->fb.pitch * 8 / 256,
                Epl_Sec, 0,
                Epl_Snoop, 0,
                Epl_Fb, 1,
                Epl_Compress_En, pdisp_plane->fb.flag.compress);

            plane_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_11, Register_Plns_Mmio_11_Start, 1,
                Epl_Base_Adr, (disp_u32_t)(pdisp_plane->fb.address >> 5));

            plane_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_12, Register_Plns_Mmio_12_Start, 1,
                Epl_Baseoff, (disp_u32_t)(pdisp_plane->fb.offset >> 5));

            plane_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_13, Register_Plns_Mmio_13_Start, 2,
                Epl_Cl_Idx, pdisp_plane->fb.bl_index,
                Epl_Cl_Type, pdisp_plane->fb.range_type);

            plane_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_14, Register_Plns_Mmio_14_Start, 2,
                Epl_Width, epl_w,
                Epl_Height, epl_h);

            plane_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_15, Register_Plns_Mmio_15_Start, 2,
                Epl_Xstart, pdisp_plane->dst.x,
                Epl_Ystart, pdisp_plane->dst.y);

            plane_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_16, Register_Plns_Mmio_16_Start, 1,
                Epl_Key, pdisp_plane->blend_info.key_value);

            //CTS need bypass csc, for video pattern CRC check
            plane_csc_bypass = (pdd->flag.info.dp_cts) ? 1 : 0;

            plane_write_field(pdisp_plane->crtc_index, pdisp_plane->plane_index, Reg_Plns_Mmio_17, Register_Plns_Mmio_17_Start, 4,
                Epl_Csc_In_Fmt, dpe_csc_in_format,
                Epl_Csc_Out_Fmt, PLNS_MMIO_17_EPL_CSC_IN_FMT_RGB_FULL,
                Epl_Csc_Prog, 0,
                Epl_Csc_Bypass, plane_csc_bypass);
        }
        else
        {
            crtc_write_field(pdisp_plane->crtc_index, Reg_Plns_Mmio_9, Register_Plns_Mmio_9_Start, 5,
                Cs_En, 1,
                Cs_Type, pdisp_plane->cursor_type,
                Cs_Height, pdisp_plane->fb.height,
                Cs_Width, pdisp_plane->fb.width,
                Cs_Out_Fmt, PLNS_MMIO_9_CS_OUT_FMT_RGB_FULL);
            crtc_write_field(pdisp_plane->crtc_index, Reg_Plns_Mmio_A, Register_Plns_Mmio_A_Start, 2,
                Cs_Scrn_Xpos, pdisp_plane->dst.x,
                Cs_Scrn_Ypos, pdisp_plane->dst.y);
            crtc_write_field(pdisp_plane->crtc_index, Reg_Plns_Mmio_B, Register_Plns_Mmio_B_Start, 3,
                Cs_Y, 0,
                Cs_X, 0,
                Cs_Rlen, PLNS_MMIO_B_CS_RLEN_R2K);
            crtc_write_field(pdisp_plane->crtc_index, Reg_Plns_Mmio_C, Register_Plns_Mmio_C_Start, 1,
                Cs_Base_Adr, (disp_u32_t)(pdisp_plane->fb.address >> 5));
            crtc_write_field(pdisp_plane->crtc_index, Reg_Plns_Mmio_D, Register_Plns_Mmio_D_Start, 1,
                Cs_Bg_Color, pdisp_plane->mono_bg_color);
            crtc_write_field(pdisp_plane->crtc_index, Reg_Plns_Mmio_E, Register_Plns_Mmio_E_Start, 1,
                Cs_Fg_Color, pdisp_plane->mono_fg_color);
            crtc_write_field(pdisp_plane->crtc_index, Reg_Plns_Mmio_F, Register_Plns_Mmio_F_Start, 3,
                Cs_Sec, 0,
                Cs_Snoop, 0,
                Cs_Fb, 1);
        }
    }

    if(pdisp_plane->flag.bits.flush_hdr_module)
    {
        disp_hdr_flush(pdisp_plane);
    }
}

DISP_STATUS disp_plane_update(pdisp_plane_t pdisp_plane, pdisp_plane_update_para_t pupdate_para)
{
    plane_generate(pdisp_plane, pupdate_para);
    plane_flush(pdisp_plane);
    disp_plane_dump(pdisp_plane);

    return DISP_OK;
}

DISP_STATUS disp_plane_update_scale(pdisp_plane_t pdisp_plane, pdisp_scale_t pscale)
{
    pdisp_plane->src.w = pscale->src_width;
    pdisp_plane->src.h = pscale->src_height;
    pdisp_plane->dst.w = pscale->dst_width;
    pdisp_plane->dst.h = pscale->dst_height;
    pdisp_plane->dst.x = pscale->start_x;
    pdisp_plane->dst.y = pscale->start_y;
    plane_flush(pdisp_plane); // refine later, flush plane scale only
    return DISP_OK;
}

DISP_STATUS disp_plane_get_addr(pdisp_void_t pdisp_device, pdisp_get_fb_addr_t paddr)
{
    pdisp_device_t pdd = (pdisp_device_t)pdisp_device;
    disp_u32_t addr = 0;
    DISP_STATUS ret = DISP_OK;

    switch(paddr->plane_index)
    {
        case DISP_PRIMARY_PLANE:
        case DISP_OVERLAY_PLANE:
            plane_read_field(paddr->crtc_index, paddr->plane_index, Reg_Plns_Mmio_11, Register_Plns_Mmio_11_Start, 1,
                Epl_Base_Adr, &addr);
            break;
        case DISP_CURSOR_PLANE:
            crtc_read_field(paddr->crtc_index, Reg_Plns_Mmio_C, Register_Plns_Mmio_C_Start, 1,
                Cs_Base_Adr, &addr);
            break;
        default:
            disp_print(DDM_WARNING, "disp_crtc_plane_get_addr, invalid plane_index[%d]\n", paddr->plane_index);
            addr = 0;
            ret = -DISP_EINVAL; 
            break;
    }

    paddr->address = (disp_u64_t)addr << 5;

    return ret;
}

static disp_u8_t is_out_of_boundary(disp_s32_t value1, disp_s32_t value2, disp_s32_t boundary)
{
    return (DISP_MAX(value1, value2) - DISP_MIN(value1, value2) > boundary) ? 1 : 0;
}
