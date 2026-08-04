/*
 * @Copyright   Copyright (c) Lisuan Tech. All Rights Reserved
 * @License     Dual MIT/GPLv2
 */
#pragma once
#ifndef BASIC_EVENT_H
#define BASIC_EVENT_H

#include "gf_os_interface.h"
#include "gf_hw_cmds.h"

struct lisuan_device;

#define EVENT_MAGIC_VALUE           0x5645534C  // LSEV

#ifdef __cplusplus
extern "C" {
#endif
    typedef enum event_type_e
    {
        EVENT_TYPE_MAGIC = 0x0000,
        EVENT_TYPE_TIMESTAMP32 = 0x0001,
        EVENT_TYPE_TIMESTAMP64 = 0x0002,
        EVENT_TYPE_SIMPLE_DWORD = 0x0003,
        EVENT_TYPE_SIMPLE_QWORD = 0x0004,
        EVENT_TYPE_STRING = 0x0005,

        EVENT_TYPE_CMD_KICKOFF = 0x0010,   // UMD kick off this cmd buffer to KMD
        EVENT_TYPE_CMD_SUBMIT = 0x0011,   // KMD submit this cmd buffer to HW
        EVENT_TYPE_CMD_FINISH = 0x0012,   // FENCE of this CMD returns
        EVENT_TYPE_VSYNC = 0x0013,   // VSYNC
        EVENT_TYPE_FLIP_MAIN = 0x0014,   // DISPLAY IGA FLIP
        EVENT_TYPE_FLIP_OVERLAY = 0x0015,   // DISPLAY IGA FLIP OVERLAY

        EVENT_TYPE_UMD_EVENT_BEGIN = 0x1000,
        EVENT_TYPE_UMD_DRAW_ENTER = 0x1000,   // LSB should be 0
        EVENT_TYPE_UMD_DRAW_EXIT = 0x1001,   // LSB should be 1
        EVENT_TYPE_UMD_DISPATCH_ENTER = 0x1002,
        EVENT_TYPE_UMD_DISPATCH_EXIT = 0x1003,
        EVENT_TYPE_UMD_MAP_ENTER = 0x1004,
        EVENT_TYPE_UMD_MAP_EXIT = 0x1005,
        EVENT_TYPE_UMD_UNMAP_ENTER = 0x1006,
        EVENT_TYPE_UMD_UNMAP_EXIT = 0x1007,
        EVENT_TYPE_UMD_EVENT_END = 0x1FFF,


        EVENT_TYPE_API_OGL_BEGIN = 0x2000,
        EVENT_TYPE_API_OGL_END = 0x2FFF,


        EVENT_TYPE_API_VULKAN_BEGIN = 0x3000,
        EVENT_TYPE_API_VULKAN_END = 0x3FFF,

    }event_type_e;

#ifdef __cplusplus
} // extern "C"
#endif


#define EVENT_HEADER_SIZE sizeof(event_header_s)
#define EVENT_TIME_STAMP_SIZE sizeof(event_time_stamp_s)
#define EVENT_TIME_STAMP64_SIZE sizeof(event_time_stamp64_s)
#define EVENT_MAGIC_SIZE sizeof(event_magic_s)
#define EVENT_SIMPLE_DWORD_SIZE sizeof(event_simple_dword_s)
#define EVENT_SIMPLE_QWORD_SIZE sizeof(event_simple_qword_s)
#define EVENT_CMD_KICKOFF_SIZE sizeof(event_cmd_kickoff_s)
#define EVENT_CMD_SUBMIT_SIZE sizeof(event_cmd_submit_s)
#define EVENT_CMD_FINISH_SIZE sizeof(event_cmd_finish_s)
#define EVENT_VSYNC_SIZE sizeof(event_vsync_s)
#define EVENT_FLIP_MAIN_SIZE sizeof(event_flip_main_s)
#define EVENT_FLIP_OVERLAY_SIZE sizeof(event_flip_overlay_s)


#define EVENT_SUBMIT_TYPE_GFX  0
#define EVENT_SUBMIT_TYPE_GCE  1
#define EVENT_SUBMIT_TYPE_VEM  2
#define EVENT_SUBMIT_TYPE_DEC0 3
#define EVENT_SUBMIT_TYPE_DEC1 4
#define EVENT_SUBMIT_TYPE_ENC  5
#define EVENT_SUBMIT_TYPE_IM   6



typedef struct event_header_s
{
    union {
        struct {
            uint32_t    type : 16;   // match with EVENT_TYPE
            uint32_t    size : 12;   // size of each event_type_s structure
            uint32_t    reserved : 2;
            uint32_t    is_time_stamp64 : 1;    // sometimes need to use 64bits timestamp
            uint32_t    ext_flag : 1;    // have extension header or not
        };
        uint32_t        value;
    };
} event_header_s;


typedef struct event_time_stamp_s {
    event_header_s      header;
    uint64_t            time_stamp;
} event_time_stamp_s;

typedef struct event_time_stamp64_s {
    event_header_s      header;
    union {
        struct {
            uint32_t    time_stamp_l;
            uint32_t    time_stamp_h;
        };
        uint64_t        time_stamp_64;
    };
} event_time_stamp64_s;

typedef struct event_magic_s {
    event_header_s      header;
    union {
        uint8_t 	    magic[4];            // "LSEV", check event file
        uint32_t        magic_value;         // EVENT_MAGIC_VALUE 0x5645534C
    };
} event_magic_s;

typedef struct event_simple_dword_s{
    event_header_s      header;
    uint32_t            dw_value;
}event_simple_dword_s;


typedef struct event_simple_qword_s{
    event_header_s      header;
    uint64_t            qw_value;
}event_simple_qword_s;


typedef struct event_cmd_kickoff_s{
    event_header_s      header;
    uint32_t            time_stamp;
    uint32_t            cmd_index;     // cmd_index ++
    uint32_t            context;       // lisuan_file->pid
    uint32_t            queue_index;   // ring buffer
}event_cmd_kickoff_s;

typedef struct event_cmd_submit_s{
    event_header_s      header;
    uint32_t            time_stamp;
    uint32_t            cmd_index;
    uint32_t            context;        // pid
    uint32_t            queue_index;    // ring buffer index
    uint32_t            submit_type;    // 2D, 3D,Pagine,Present, Overlay, Video,.....
    uint32_t            fence_id;       // fence id, used to match with finish event
}event_cmd_submit_s;


typedef struct event_cmd_finish_s{
    event_header_s      header;
    uint32_t            time_stamp;
    uint32_t            queue_index;
    uint32_t            fence_id;
}event_cmd_finish_s;


typedef struct event_vsync_s{
    event_header_s      header;
    uint32_t            time_stamp;
    uint32_t            iga_index;
    uint32_t            vsync_index;
}event_vsync_s;

typedef struct event_flip_main_s{
    event_header_s      header;
    uint32_t            time_stamp;
    uint32_t            iga_index;
    uint32_t            allocation_id;
}event_flip_main_s;


typedef struct event_flip_overlay_s{
    event_header_s      header;
    uint32_t            time_stamp;
    uint32_t            iga_index;
    uint32_t            allocation_id;
}event_flip_overlay_s;


typedef struct event_umd_basic_s{
    event_header_s      header;
    uint32_t time_stamp;
    uint32_t process_id;
    uint32_t thread_id;
}event_umd_basic_s;

//#endif // __cplusplus
#define RING_BUFFER_SIZE 4096  // Or larger if needed

typedef void (*record_func_t)(struct lisuan_device *ls_dev, void *event_ptr, uint32_t event_size);

typedef struct {
    uint8_t  data[RING_BUFFER_SIZE];
    uint32_t head;
    uint32_t tail;
    spinlock_t lock;
    unsigned long flags;        // For spinlock protection 
    record_func_t record_func;  // Function pointer for recording events
	bool log_enabled;
    wait_queue_head_t wq;       // every card has its own wait queue
    struct lisuan_device *dev;
} gpu_event_log_t;

void gf_kmdEvent_log_init(struct lisuan_device *ls_dev);
void gf_kmdEvent_log_exit(struct lisuan_device *ls_dev);

ssize_t gf_kmdEvent_log_read(struct file *file, char __user *user_buf, size_t count, loff_t *offp);
ssize_t gf_kmdEvent_log_write(struct file *file, const char __user *user_buf, size_t count, loff_t *offp);
__poll_t gf_kmdEvent_log_poll(struct file *file, poll_table *wait);

void gf_init_evt_kickoff(event_cmd_kickoff_s *kick_ptr,  uint32_t cmd_idx, uint32_t ctx, uint32_t q_idx);
void gf_init_evt_submit(event_cmd_submit_s *submit_ptr, uint32_t cmd_idx, uint32_t ctx, uint32_t q_idx, uint32_t sub_type, uint32_t f_id);
void gf_init_evt_finish(event_cmd_finish_s *finish_ptr,  uint32_t q_id, uint32_t f_id);
void gf_init_evt_vsync(event_vsync_s *vsync_ptr,  uint32_t iga_idx);
void gf_init_evt_flip_main(event_flip_main_s *flip_ptr, uint32_t iga_idx, uint32_t allo_id);
void gf_init_evt_flip_overlay(event_flip_overlay_s *flip_overlay_ptr, uint32_t iga_idx, uint32_t allo_id);

void gf_record_evt_kickoff(struct lisuan_device *ls_dev, uint32_t context, uint32_t cmd_index, uint32_t queue_index);
void gf_record_evt_submit(struct lisuan_device *ls_dev, uint32_t  context, uint32_t cmd_index, uint32_t queue_index, uint32_t submit_type, uint32_t fence_id);
void gf_record_evt_finish(struct lisuan_device *ls_dev, uint32_t queue_index,  uint32_t fence_id);
void gf_record_evt_vsync(struct lisuan_device *ls_dev, int iga_index);
void gf_record_evt_flip_main(struct lisuan_device *ls_dev, event_flip_main_s *flip_ptr,void *vir_addr);
void gf_record_evt_flip_overlay(struct lisuan_device *ls_dev, event_flip_overlay_s *flip_overlay_ptr,void *vir_addr);

/**
 * ============================== gpu usage (lock-free) ==============================
 */


/**
 * @brief use atomic_t to protect the following fields:
 */
typedef struct queue_info_atomic {
	atomic64_t last_fence_id;       // last submitted fence ID
	atomic64_t idle_time;           // accumulated idle time (ns)
	atomic64_t pre_time_stamp;      // previous timestamp (ns)
	atomic_t usage;                 // usage rate (percentage, 0-100)
	atomic_t is_idle;               // idle status (0=busy, 1=idle)
	atomic_t snapshot_seq;          // snapshot sequence number (for consistency check)
} queue_info_atomic_t;

/**
 * @brief snapshot structure for reading (no atomic operations needed)
 */
typedef struct queue_info_snapshot {
	// uint64_t last_fence_id;
	// uint64_t idle_time;
	// uint64_t pre_time_stamp;
	uint32_t usage;
	bool is_idle;
	uint32_t seq;                   // sequence number
} queue_info_snapshot_t;

/**
 * @brief gpu usage sampling information (lock-free)
 */
typedef struct gpu_usage_info {
	struct lisuan_device *dev;
	atomic_t is_log_enabled;        // atomic variable: whether enabled
	atomic64_t start_stamp;         // sampling start time
	atomic_t is_started;            // whether sampling has started

	// timer related
	struct hrtimer sampling_timer;
	ktime_t sampling_interval;
	atomic_t timer_running;         // whether timer is running

	uint16_t queue_num;

	// sampling data (protected by atomic operations)
	queue_info_atomic_t queue_infos[RB_INDEX_LAST];

	// snapshot data (for read only, no lock protection needed)
	queue_info_snapshot_t snapshot[RB_INDEX_LAST];

} gpu_usage_info_t;

void gf_gpuusage_init(struct lisuan_device *ls_device);

void gf_gpuusage_exit(struct lisuan_device *ls_device);

int gf_gpuusage_open(struct inode *inode, struct file *file);

ssize_t gf_gpuusage_read(struct file *file, char __user *user_buf, size_t count, loff_t *offp);

ssize_t gf_gpuusage_write(struct file *file, const char __user *user_buf, size_t count, loff_t *offp);

void gf_gpuusage_submit(struct lisuan_device *ls_device, gf_rb_index queue_type, unsigned long long fence_id);

void gf_gpuusage_complete(struct lisuan_device *ls_device, gf_rb_index queue_type, unsigned long long fence_id);

void gf_gpuusage_vsync(gpu_usage_info_t *usage_info, struct drm_crtc *crtc, int crtc_index);

void gf_gpuusage_start_timer(struct lisuan_device *ls_device);

void gf_gpuusage_stop_timer(struct lisuan_device *ls_device);

#endif // end of BASIC_EVENT_H