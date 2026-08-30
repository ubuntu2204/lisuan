/*
 * @Copyright   Copyright (c) Lisuan Tech. All Rights Reserved
 * @License     Dual MIT/GPLv2
 */

#pragma once

typedef struct driver_settings_s
{
    unsigned int null_rendering;
    unsigned int force_local;          
    unsigned int force_nonlocal;       
    unsigned int cmd_buffer_no_overlap;
    unsigned int per_draw_kickoff;     
    unsigned int pcnt_global_mode;     
    unsigned int pcnt_mode;            
    unsigned int pcnt_slc_index;       
    unsigned int enable_pcnt_dump;     
    unsigned int enable_cmodel_cnt_dump;
    unsigned int counter_dump_flags;
    unsigned int per_draw_counter;
    unsigned int per_cmd_buf_counter;
    unsigned int per_frame_counter;
    unsigned int chip_image_dump;
    unsigned int cmd_dump;    
    unsigned int perf_frame_index_start;
    unsigned int perf_frame_index_end; //include
    unsigned int player_frame_index; //from player
    unsigned int player_line_num; //from players
    unsigned int skip_debug_code;
    unsigned int flush_pipe_per_draw;
    unsigned int enable_fp32_bordercolor;
    char         dump_path[260];
}driver_settings_s;
