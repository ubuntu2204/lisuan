/*
 * @Copyright   Copyright (c) Lisuan Tech. All Rights Reserved
 * @License     Dual MIT/GPLv2
 */
#include "gf_kmd_event.h"
#include <linux/proc_fs.h>
#include <linux/poll.h>
#include <linux/wait.h>
#include <linux/ktime.h>
#include <linux/seq_file.h>
#include <linux/hrtimer.h>
#include"lisuan_device.h"


#define PROC_FILENAME "kmdEvent_log_control"
#define RING_BUFFER_SIZE 4096  // Or larger if needed


extern int log_enabled;  //log control flag from log_control module


static uint32_t buffer_size_offset = 0;
static u64 file_index = 0;

uint64_t timestamp_base = 0;


static uint32_t crtc_vsync_idx[4]={0};
static uint64_t get_timestamp32(void);

static void gf_crtc_vsync_idx_plus(uint32_t crtc_index)
{
    if (crtc_index < 4) {
        crtc_vsync_idx[crtc_index]++;
    } else {
        gf_error("Invalid CRTC index: %u", crtc_index);
    }
}



static uint64_t get_timestamp32(void)
{
    uint32_t timestamp32;
    if(timestamp_base == 0)
    {
        timestamp_base = ktime_get_ns();
        return timestamp32 = 0;
    }
    else
    {
        timestamp32 = (uint32_t)((ktime_get_ns() - timestamp_base) / 1000);
    }
    return  timestamp32;
}


void gf_init_evt_kickoff(event_cmd_kickoff_s *kick_ptr,  uint32_t cmd_idx, uint32_t ctx, uint32_t q_idx)
{
    kick_ptr->header.type = EVENT_TYPE_CMD_KICKOFF;
    kick_ptr->header.size = EVENT_CMD_KICKOFF_SIZE;
    kick_ptr->time_stamp =  get_timestamp32();
    kick_ptr->cmd_index = cmd_idx;
    kick_ptr->context = ctx;
    kick_ptr->queue_index = q_idx;
}

void gf_init_evt_submit(event_cmd_submit_s *submit_ptr, uint32_t cmd_idx, uint32_t ctx, uint32_t q_idx, uint32_t sub_type, uint32_t f_id)
{
    submit_ptr->header.type = EVENT_TYPE_CMD_SUBMIT;
    submit_ptr->header.size = EVENT_CMD_SUBMIT_SIZE;
    submit_ptr->time_stamp = get_timestamp32();
    submit_ptr->cmd_index = cmd_idx;
    submit_ptr->context = ctx;
    submit_ptr->queue_index = q_idx;
    submit_ptr->submit_type = sub_type;
    submit_ptr->fence_id = f_id;
}

void gf_init_evt_finish(event_cmd_finish_s *finish_ptr,  uint32_t q_id, uint32_t f_id)
{
    finish_ptr->header.type = EVENT_TYPE_CMD_FINISH;
    finish_ptr->header.size = EVENT_CMD_FINISH_SIZE;
    finish_ptr->time_stamp =  get_timestamp32();
    finish_ptr->queue_index = q_id;
    finish_ptr->fence_id = f_id;
}

void gf_init_evt_vsync(event_vsync_s *vsync_ptr,  uint32_t iga_idx)
{
    vsync_ptr->header.type = EVENT_TYPE_VSYNC;
    vsync_ptr->header.size = EVENT_VSYNC_SIZE;
    vsync_ptr->time_stamp = get_timestamp32();
    vsync_ptr->iga_index = iga_idx;
    vsync_ptr->vsync_index = crtc_vsync_idx[iga_idx]++;
}




static void gf_record_ring_buffer(struct lisuan_device *ls_dev, void* event, uint32_t event_size)
{
    uint32_t head = ls_dev->gpu_event_log.head;
    uint32_t tail = ls_dev->gpu_event_log.tail;

    uint32_t free_space = (tail + RING_BUFFER_SIZE - head - 1) % RING_BUFFER_SIZE;

    if (free_space >= event_size)
    {
        uint32_t index = head % RING_BUFFER_SIZE;
        if (index + event_size <= RING_BUFFER_SIZE)
        {
            gf_memcpy(&ls_dev->gpu_event_log.data[index], event, event_size);
        }
        else
        {
            // wraparound case
            uint32_t first_part = RING_BUFFER_SIZE - index;
            gf_memcpy(&ls_dev->gpu_event_log.data[index], event, first_part);
            gf_memcpy(&ls_dev->gpu_event_log.data[0], ((uint8_t*)event) + first_part, event_size - first_part);
        }

        ls_dev->gpu_event_log.head = (head + event_size) % (2 * RING_BUFFER_SIZE);  // modulo wrap
        spin_unlock_irqrestore(&ls_dev->gpu_event_log.lock, ls_dev->gpu_event_log.flags);

        wake_up_interruptible(&ls_dev->gpu_event_log.wq);  // wake user-space reader
    }
    else
    {
        // Buffer full – drop or count loss (or could block/spin)
        spin_unlock_irqrestore(&ls_dev->gpu_event_log.lock, ls_dev->gpu_event_log.flags);
        gf_error("Ring buffer full! Lost kickoff event.");
    }
    
}

void gf_record_evt_kickoff(struct lisuan_device *ls_dev, uint32_t context, uint32_t cmd_index, uint32_t queue_index)
{
    event_cmd_kickoff_s event;

    if (!ls_dev->gpu_event_log.log_enabled)
        return;

    // Lockless ring buffer write with spinlock protection
    // unsigned long flags;
    spin_lock_irqsave(&ls_dev->gpu_event_log.lock, ls_dev->gpu_event_log.flags);
    
    gf_memset(&event, 0, sizeof(event_cmd_kickoff_s));

    //cmd_index++;
    gf_init_evt_kickoff(&event, cmd_index, context, queue_index);

    ls_dev->gpu_event_log.record_func(ls_dev, &event, sizeof(event_cmd_kickoff_s));

}




void gf_record_evt_submit(struct lisuan_device *ls_dev,uint32_t  context, uint32_t cmd_index, uint32_t queue_index, uint32_t submit_type, uint32_t fence_id)
{
    event_cmd_submit_s event;

    if(!ls_dev->gpu_event_log.log_enabled)
    {
        return; // If logging is disabled, do not record the event
    }

    // Lockless ring buffer write with spinlock protection
    // unsigned long flags;
    spin_lock_irqsave(&ls_dev->gpu_event_log.lock, ls_dev->gpu_event_log.flags);
    
    gf_memset(&event, 0, sizeof(event_cmd_submit_s));
    gf_init_evt_submit(&event, cmd_index, context, queue_index, submit_type, fence_id);

    ls_dev->gpu_event_log.record_func(ls_dev, &event, sizeof(event_cmd_submit_s));
}



void gf_record_evt_finish(struct lisuan_device *ls_dev,uint32_t queue_index, uint32_t fence_id)
{
    event_cmd_finish_s event;

    if(!ls_dev->gpu_event_log.log_enabled)
    {
        return; // If logging is disabled, do not record the event
    }

    // Lockless ring buffer write with spinlock protection
    // unsigned long flags;
    spin_lock_irqsave(&ls_dev->gpu_event_log.lock, ls_dev->gpu_event_log.flags);
    
    gf_memset(&event, 0, sizeof(event_cmd_finish_s));

    gf_init_evt_finish(&event, queue_index,  fence_id);

    ls_dev->gpu_event_log.record_func(ls_dev, &event, sizeof(event_cmd_finish_s));
    
}

void gf_record_evt_vsync(struct lisuan_device *ls_dev,int iga_index)
{
    event_vsync_s event;

    if(!ls_dev->gpu_event_log.log_enabled)
    {
        return; // If logging is disabled, do not record the event
    }

    spin_lock_irqsave(&ls_dev->gpu_event_log.lock, ls_dev->gpu_event_log.flags);

    gf_memset(&event, 0, sizeof(event_vsync_s));
    gf_init_evt_vsync(&event, iga_index);

    ls_dev->gpu_event_log.record_func(ls_dev, &event, sizeof(event_vsync_s));
}


ssize_t gf_kmdEvent_log_read(struct file *file, char __user *user_buf, size_t count, loff_t *offp)
{
    unsigned long flags;
    ssize_t copied = 0;
    uint32_t head = 0, tail = 0, available = 0;
    size_t to_read = 0;
	struct drm_device *ddev = file_inode(file)->i_private;
	struct lisuan_device *ls_dev = ddev->dev_private;

    spin_lock_irqsave(&ls_dev->gpu_event_log.lock, flags);

    head = ls_dev->gpu_event_log.head;
    tail = ls_dev->gpu_event_log.tail;
    available = (head + RING_BUFFER_SIZE - tail) % RING_BUFFER_SIZE;

    to_read = min((size_t)available, count);

    if (to_read > 0)
    {
        uint32_t index = tail % RING_BUFFER_SIZE;
        if (index + to_read <= RING_BUFFER_SIZE)
        {
            if (copy_to_user(user_buf, &ls_dev->gpu_event_log.data[index], to_read))
            {
                spin_unlock_irqrestore(&ls_dev->gpu_event_log.lock, flags);
                return -EFAULT;
            }
        }
        else
        {
            // wraparound read
            uint32_t first_chunk = RING_BUFFER_SIZE - index;
            if (copy_to_user(user_buf, &ls_dev->gpu_event_log.data[index], first_chunk) ||
                copy_to_user(user_buf + first_chunk, &ls_dev->gpu_event_log.data[0], to_read - first_chunk))
            {
                spin_unlock_irqrestore(&ls_dev->gpu_event_log.lock, flags);
                return -EFAULT;
            }
        }

        ls_dev->gpu_event_log.tail = (tail + to_read) % (2 * RING_BUFFER_SIZE);
        copied = to_read;
    }

    spin_unlock_irqrestore(&ls_dev->gpu_event_log.lock, flags);

    return copied;
}

ssize_t gf_kmdEvent_log_write(struct file *file, const char __user *user_buf, size_t count, loff_t *offp)
{	
	struct drm_device *ddev = file_inode(file)->i_private;
	struct lisuan_device *ls_dev = ddev->dev_private;

	char buffer[2] = {0};
    if (copy_from_user(buffer, user_buf, count)) {
        return -EFAULT;
    }
    buffer[count] = '\0';

    if (buffer[0] == '1') {
        ls_dev->gpu_event_log.log_enabled = true;
        printk(KERN_INFO "Logging enabled.\n");
    } else if (buffer[0] == '0') {
        ls_dev->gpu_event_log.log_enabled = false;
        printk(KERN_INFO "Logging disabled.\n");
    } else {
        printk(KERN_INFO "Invalid input. Use '1' to enable or '0' to disable logging.\n");
    }

    return count;
}

__poll_t gf_kmdEvent_log_poll(struct file *file, poll_table *wait)
{
    uint32_t available = 0;
	struct drm_device *ddev = file_inode(file)->i_private;
	struct lisuan_device *ls_device = ddev->dev_private;

    poll_wait(file, &ls_device->gpu_event_log.wq, wait);

    available = (ls_device->gpu_event_log.head + RING_BUFFER_SIZE - ls_device->gpu_event_log.tail) % RING_BUFFER_SIZE;
    if (available > 0)
        return EPOLLIN | EPOLLRDNORM;
    return 0;
}

void gf_kmdEvent_log_init(struct lisuan_device *ls_device)
{
    ls_device->gpu_event_log.dev = ls_device;
    spin_lock_init(&ls_device->gpu_event_log.lock);
    init_waitqueue_head(&ls_device->gpu_event_log.wq);
    ls_device->gpu_event_log.record_func = gf_record_ring_buffer;
}


void gf_kmdEvent_log_exit(struct lisuan_device *ls_device)
{
    // Clear ring buffer
    spin_lock_irq(&ls_device->gpu_event_log.lock);
    ls_device->gpu_event_log.head = ls_device->gpu_event_log.tail = 0;
    spin_unlock_irq(&ls_device->gpu_event_log.lock);
}

/**
 * ============================== gpu usage (lock-free implementation) ==============================
 */

#define GPUUSAGE_BUF_SIZE 256
#define GPUUSAGE_DEFAULT_INTERVAL_MS 1000 // default sampling interval: 1 second

static char *rb_index_str[RB_INDEX_LAST] = {"3D", "COPY", "VPP", "DEC0", "DEC1", "ENC", "IM"};
static char gpu_usage_buffer[GPUUSAGE_BUF_SIZE]; // buffer for gpu usage info

/**
 * @brief get time delta (ns)
 */
static inline uint64_t get_delta_time_ns(uint64_t start_ns, uint64_t end_ns)
{
	return (end_ns - start_ns);
}

/**
 * @brief atomic operation: check if idle by fence_id
 */
static inline bool is_start_idle_atomic(uint64_t fence_id, uint64_t last_fence_id)
{
	return (fence_id == last_fence_id);
}

/**
 * @brief initialize gpu usage (lock-free version)
 *
 * Steps:
 * 1. initialize gpu_usage_info
 * 2. initialize atomic variables for each queue
 * 3. initialize snapshot
 */
void gf_gpuusage_init(struct lisuan_device *ls_device)
{
	gpu_usage_info_t *gpu_usage_info = &ls_device->gpu_usage_info;
	int i;

	// 1. init gpu_usage_info
	gf_memset(gpu_usage_info, 0, sizeof(gpu_usage_info_t));

	gpu_usage_info->queue_num = RB_INDEX_LAST;
	atomic_set(&gpu_usage_info->is_log_enabled, 0);
	atomic_set(&gpu_usage_info->is_started, 0);
	atomic_set(&gpu_usage_info->timer_running, 0);
	atomic64_set(&gpu_usage_info->start_stamp, 0);

	// 2. initialize atomic variables for each queue
	for (i = 0; i < RB_INDEX_LAST; i++)
	{
		atomic64_set(&gpu_usage_info->queue_infos[i].last_fence_id, 0);
		atomic64_set(&gpu_usage_info->queue_infos[i].idle_time, 0);
		atomic64_set(&gpu_usage_info->queue_infos[i].pre_time_stamp, 0);
		atomic_set(&gpu_usage_info->queue_infos[i].usage, 0);
		atomic_set(&gpu_usage_info->queue_infos[i].is_idle, 1); // initial state is idle
		atomic_set(&gpu_usage_info->queue_infos[i].snapshot_seq, 0);
	}

	// 3. initialize snapshot
	gf_memset(gpu_usage_info->snapshot, 0, sizeof(gpu_usage_info->snapshot));

	pr_debug("gpuusage: lock-free mode initialized\n");
}

/**
 * @brief exit gpu usage (lock-free version)
 */
void gf_gpuusage_exit(struct lisuan_device *ls_device)
{
	// ensure timer is stopped
	gf_gpuusage_stop_timer(ls_device);

	pr_debug("gpuusage: lock-free mode exited\n");
}

/**
 * @brief called on submit (lock-free version)
 *
 * Steps:
 * 1. atomically update fence ID
 * 2. check if previously idle, if so accumulate idle time, and mark as busy
 *  2.1 accumulate idle time
 */
void gf_gpuusage_submit(struct lisuan_device *ls_device, gf_rb_index queue_type, unsigned long long fence_id)
{
	gpu_usage_info_t *gpu_usage_info = &ls_device->gpu_usage_info;
	queue_info_atomic_t *queue_info = &gpu_usage_info->queue_infos[queue_type];
	uint64_t last_fence;
	int was_idle;

	// return directly if not enabled
	if (atomic_read(&gpu_usage_info->is_log_enabled) == 0)
	{
		return;
	}

	last_fence = atomic64_read(&queue_info->last_fence_id);
	was_idle = atomic_read(&queue_info->is_idle);

	// 1. atomically update fence ID
	atomic64_set(&queue_info->last_fence_id, fence_id);

	// 2. check if previously idle, if so accumulate idle time, and mark as busy
	if (atomic_xchg(&queue_info->is_idle, 0) == 1)
	{
		// 2.1 accumulate idle time
		uint64_t current_time = ktime_get_ns();
		uint64_t pre_time = atomic64_read(&queue_info->pre_time_stamp);
		uint64_t delta = get_delta_time_ns(pre_time, current_time);

		atomic64_add(delta, &queue_info->idle_time);
		atomic64_set(&queue_info->pre_time_stamp, current_time);

		// gf_info("gpuusage: [%s] idle->busy, fence=%llu->%llu, idle_time+=%llu ns, total_idle=%llu ns\n",
		// 	rb_index_str[queue_type], last_fence, fence_id, delta,
		// 	atomic64_read(&queue_info->idle_time));
	}
}

/**
 * @brief called on complete (lock-free version)
 *
 * Steps:
 * 1. check if becoming idle and status is if not idle
 *  1.1 record begining sampler timestamp and set idle flag
 */
void gf_gpuusage_complete(struct lisuan_device *ls_device, gf_rb_index queue_type, unsigned long long fence_id)
{
	gpu_usage_info_t *gpu_usage_info = &ls_device->gpu_usage_info;
	queue_info_atomic_t *queue_info = &gpu_usage_info->queue_infos[queue_type];
	uint64_t last_fence;
	uint64_t current_time;

	if (atomic_read(&gpu_usage_info->is_log_enabled) == 0)
	{
		return;
	}

	// 1. check if becoming idle and status is if not idle
	last_fence = atomic64_read(&queue_info->last_fence_id);
	if (is_start_idle_atomic(fence_id, last_fence))
	{
		if (atomic_cmpxchg(&queue_info->is_idle, 0, 1) == 0)
		{
			// 1.1 record begining sampler timestamp and set idle flag
			current_time = ktime_get_ns();
			atomic64_set(&queue_info->pre_time_stamp, current_time);

			// gf_info("gpuusage: [%s] busy->idle, fence=%llu completed, idle_time=%llu ns, usage=%u%%\n",
			// 	rb_index_str[queue_type], fence_id,
			// 	atomic64_read(&queue_info->idle_time),
			// 	atomic_read(&queue_info->usage));
		}
	}
}

/**
 * @brief calculate usage (lock-free version)
 * @return usage percentage (0-100)
 */
static uint32_t calculate_gpuusage_atomic(gpu_usage_info_t *usage_info, gf_rb_index queue_type)
{
	queue_info_atomic_t *queue_info = &usage_info->queue_infos[queue_type];
	uint64_t busy_time;
	uint32_t usage;

	uint64_t current_time = ktime_get_ns();
	uint64_t start_stamp = atomic64_read(&usage_info->start_stamp);
	uint64_t idle_time = atomic64_read(&queue_info->idle_time);

	uint64_t total_time = get_delta_time_ns(start_stamp, current_time);

	// prevent division by zero
	if (total_time == 0)
	{
		return 0;
	}

	busy_time = (idle_time >= total_time) ? 0 : (total_time - idle_time);
	usage = (uint32_t)((busy_time * 100) / total_time);

	pr_debug("gpuusage: [%s] usage=%u%%, total=%llu, idle=%llu ns, is_idle=%d\n",
			 rb_index_str[queue_type], usage,
			 (unsigned long long)total_time,
			 (unsigned long long)atomic64_read(&queue_info->idle_time),
			 atomic_read(&queue_info->is_idle));
	return usage;
}

/**
 * @brief end sampling and calculate usage (lock-free version)
 *
 * Steps:
 * 1. accumulate final idle time and calculate usage for each queue
 *  1.1 if queue is currently idle, accumulate idle time
 *  1.2 calculate usage
 *  1.3 reset timestamp and idle time
 *  1.4 update snapshot sequence number
 * 2. sampling status became not started
 */
static void sample_gpuusage_end_atomic(gpu_usage_info_t *usage_info)
{
	int i;
	uint64_t pre_time, delta_time, usage;

	pr_debug("gpuusage: === sampling end ===\n");

	// 1. accumulate final idle time and calculate usage for each queue
	for (i = 0; i < usage_info->queue_num; i++)
	{
		queue_info_atomic_t *queue_info = &usage_info->queue_infos[i];
		uint64_t current_time = ktime_get_ns();

		// 1.1 if queue is currently idle, accumulate idle time
		if (atomic_read(&queue_info->is_idle) == 1)
		{
			pre_time = atomic64_read(&queue_info->pre_time_stamp);
			delta_time = get_delta_time_ns(pre_time, current_time);
			atomic64_add(delta_time, &queue_info->idle_time);
		}

		// 1.2 calculate usage
		usage = calculate_gpuusage_atomic(usage_info, (gf_rb_index)i);
		atomic_set(&queue_info->usage, usage);

		// 1.3 reset timestamp and idle time
		atomic64_set(&queue_info->pre_time_stamp, current_time);
		atomic64_set(&queue_info->idle_time, 0);

		// 1.4 update snapshot sequence number
		atomic_inc(&queue_info->snapshot_seq);
	}

	// 2. sampling status became not started
	atomic_set(&usage_info->is_started, 0);
}

/**
 * @brief start sampling (lock-free version)
 *
 * Steps:
 * 1. record start timestamp and sampling status
 * 2. if queue is currently idle, set previous timestamp
 */
static void sample_gpuusage_begin_atomic(gpu_usage_info_t *usage_info)
{
	uint64_t current_time = ktime_get_ns();
	int i;

	// 1. record start timestamp and sampling status
	atomic64_set(&usage_info->start_stamp, current_time);
	atomic_set(&usage_info->is_started, 1);

	pr_debug("gpuusage: === sampling begin, stamp=%llu ns ===\n", current_time);

	// 2. if queue is currently idle, set previous timestamp
	for (i = 0; i < usage_info->queue_num; i++)
	{
		queue_info_atomic_t *queue_info = &usage_info->queue_infos[i];
		if (atomic_read(&queue_info->is_idle) == 1)
		{
			atomic64_set(&queue_info->pre_time_stamp, current_time);
		}
	}
}

/**
 * @brief timer callback function (hard IRQ context, lock-free)
 *
 * Steps:
 * 1. if sampling status is started, end sampling and start new sampling
 * 2. restart timer
 */
enum hrtimer_restart gpuusage_timer_callback(struct hrtimer *timer)
{
	gpu_usage_info_t *usage_info = container_of(timer, gpu_usage_info_t, sampling_timer);
	struct lisuan_device *ls_device = usage_info->dev;

	if (atomic_read(&usage_info->is_log_enabled) == 0)
	{
		return HRTIMER_NORESTART;
	}

	// 1. if sampling status is started, end sampling and start new sampling
	if (atomic_read(&usage_info->is_started) == 1)
	{
		sample_gpuusage_end_atomic(usage_info);
	}

	sample_gpuusage_begin_atomic(usage_info);

	// 2. restart timer
	hrtimer_forward_now(timer, usage_info->sampling_interval);
	return HRTIMER_RESTART;
}

/**
 * @brief start sampling timer
 *
 * Steps:
 * 1. check if already running
 * 2. initialize timer when not already running
 * 3. before starting timer, need to begin sampling
 * 4. start timer
 */
void gf_gpuusage_start_timer(struct lisuan_device *ls_device)
{
	gpu_usage_info_t *usage_info = &ls_device->gpu_usage_info;

	// 1. check if already running
	if (atomic_xchg(&usage_info->timer_running, 1) == 1)
	{
		gf_warning("gpuusage: timer already running\n");
		return;
	}

	// 2. initialize timer when not already running
	usage_info->sampling_interval = ktime_set(0, GPUUSAGE_DEFAULT_INTERVAL_MS * NSEC_PER_MSEC);
#if DRM_VERSION_CODE >= KERNEL_VERSION(6,15,0)
	hrtimer_setup(&usage_info->sampling_timer, gpuusage_timer_callback, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
#else
	hrtimer_init(&usage_info->sampling_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	usage_info->sampling_timer.function = gpuusage_timer_callback;
#endif
	// initialize timestamps BEFORE starting timer to prevent wrong idle_time accumulation
	// when fence submit happens before first timer callback
	// 3. before starting timer, need to begin sampling
	sample_gpuusage_begin_atomic(usage_info);

	// 4. start timer
	hrtimer_start(&usage_info->sampling_timer,
				  usage_info->sampling_interval,
				  HRTIMER_MODE_REL);

	pr_debug("gpuusage: timer started (interval=%d ms)\n", GPUUSAGE_DEFAULT_INTERVAL_MS);
}

/**
 * @brief stop sampling timer
 *
 * Steps:
 * 1. cancel timer (may wait for callback to complete)
 */
void gf_gpuusage_stop_timer(struct lisuan_device *ls_device)
{
	gpu_usage_info_t *usage_info = &ls_device->gpu_usage_info;
	int i;

	// if not running, return directly
	if (atomic_xchg(&usage_info->timer_running, 0) == 0)
	{
		return;
	}

	// 1. cancel timer (may wait for callback to complete)
	hrtimer_cancel(&usage_info->sampling_timer);

	pr_debug("gpuusage: timer stopped, final usage:\n");
	for (i = 0; i < usage_info->queue_num; i++)
	{
		queue_info_atomic_t *queue_info = &usage_info->queue_infos[i];
		pr_debug("gpuusage: [%s] usage=%u%%, idle=%llu ns, is_idle=%d, last_fence=%llu\n",
				 rb_index_str[i],
				 atomic_read(&queue_info->usage),
				 (unsigned long long)atomic64_read(&queue_info->idle_time),
				 atomic_read(&queue_info->is_idle),
				 (unsigned long long)atomic64_read(&queue_info->last_fence_id));
	}
}

/**
 * @brief read snapshot data (lock-free, using seqlock concept for consistency)
 *
 * Steps:
 * 1. read seq number about snapshot
 * 2. read usage and idle status
 * 3. read sequence number again, check if changed
 * 4. if sequence number changed, data may be inconsistent, mark as invalid
 */
static void take_snapshot(gpu_usage_info_t *usage_info)
{
	int i;

	for (i = 0; i < usage_info->queue_num; i++)
	{
		queue_info_atomic_t *atomic_info = &usage_info->queue_infos[i];
		queue_info_snapshot_t *snap = &usage_info->snapshot[i];
		uint32_t seq1, seq2;
		// 1. read seq number about snapshot
		seq1 = atomic_read(&atomic_info->snapshot_seq);

		// 2. read usage and idle status
		snap->usage = atomic_read(&atomic_info->usage);
		snap->is_idle = (atomic_read(&atomic_info->is_idle) == 1);

		// 3. read sequence number again, check if changed
		seq2 = atomic_read(&atomic_info->snapshot_seq);

		// 4. if sequence number changed, data may be inconsistent, mark as invalid
		// if invalid, when read snapshot, need to re-read data again
		snap->seq = (seq1 == seq2) ? seq1 : 0;
	}
}

/**
 * @brief read interface: return snapshot data (lock-free)
 */
ssize_t gf_gpuusage_read(struct file *file, char __user *user_buf, size_t count, loff_t *offp)
{
	struct drm_device *ddev = file_inode(file)->i_private;
	struct lisuan_device *ls_device = ddev->dev_private;
	gpu_usage_info_t *usage_info = &ls_device->gpu_usage_info;
	queue_info_snapshot_t *snap;
	int len = 0;
	int i = 0, retry = 0;
	bool nsnapshot_valid = false;

	if (atomic_read(&usage_info->is_log_enabled) == 0)
	{
		return 0;
	}

	// 1. lock snapshot data for reading
	for (retry = 0; retry < 3; ++retry)
	{
		// 1.1 get snapshot data
		take_snapshot(usage_info);
		// 1.2 check if snapshot data is valid
		nsnapshot_valid = true;
		for (i = 0; i < usage_info->queue_num; i++)
		{
			if (usage_info->snapshot[i].seq == 0)
			{
				nsnapshot_valid = false;
				break;
			}
		}
	}

	if (false == nsnapshot_valid)
	{
		gf_warning("gpuusage: snapshot data is invalid, retry=%d\n", retry);
		return 0;
	}

	gf_memset(gpu_usage_buffer, 0, GPUUSAGE_BUF_SIZE);
	len = snprintf(gpu_usage_buffer, GPUUSAGE_BUF_SIZE, "GPU Usage :");

	for (i = 0; i < usage_info->queue_num; i++)
	{
		snap = &usage_info->snapshot[i];
		if (i < usage_info->queue_num - 1)
		{
			len += snprintf(gpu_usage_buffer + len, GPUUSAGE_BUF_SIZE - len,
							" [%s] - %u%%, ", rb_index_str[i], snap->usage);
		}
		else
		{
			len += snprintf(gpu_usage_buffer + len, GPUUSAGE_BUF_SIZE - len,
							" [%s] - %u%%", rb_index_str[i], snap->usage);
		}

		gf_debug("  [%s] usage=%u%%, idle=%d, seq=%u\n",
				 rb_index_str[i], snap->usage, snap->is_idle, snap->seq);
	}
	len += snprintf(gpu_usage_buffer + len, GPUUSAGE_BUF_SIZE - len, "\n");

	return simple_read_from_buffer(user_buf, count, offp, gpu_usage_buffer, len);
}

/**
 * @brief write interface: enable/disable sampling (lock-free)
 */
ssize_t gf_gpuusage_write(struct file *file, const char __user *user_buf, size_t count, loff_t *offp)
{
	struct drm_device *ddev = file_inode(file)->i_private;
	struct lisuan_device *ls_device = ddev->dev_private;
	gpu_usage_info_t *usage_info = &ls_device->gpu_usage_info;

	char set_cmd[8] = {0};
	if (count >= sizeof(set_cmd))
	{
		return -EINVAL;
	}
	if (copy_from_user(set_cmd, user_buf, count))
	{
		return -EFAULT;
	}

	if (0 == strcmp(set_cmd, "1"))
	{
		// enable
		if (atomic_xchg(&usage_info->is_log_enabled, 1) == 0)
		{
			// transition from disabled to enabled, start timer
			pr_debug("gpuusage: enable sampling\n");
			gf_gpuusage_start_timer(ls_device);
		}
	}
	else if (0 == strcmp(set_cmd, "0"))
	{
		// disable
		if (atomic_xchg(&usage_info->is_log_enabled, 0) == 1)
		{
			// transition from enabled to disabled, stop timer
			pr_debug("gpuusage: disable sampling\n");
			gf_gpuusage_stop_timer(ls_device);
		}
	}
	else
	{
		return -EINVAL;
	}

	return count;
}

/**
 * @brief open interface
 */
int gf_gpuusage_open(struct inode *inode, struct file *file)
{
	gf_memset(gpu_usage_buffer, 0, GPUUSAGE_BUF_SIZE);
	return 0;
}

/**
 * @brief vsync interface (kept for compatibility, but no longer used)
 */
void gf_gpuusage_vsync(gpu_usage_info_t *_usage_info, struct drm_crtc *crtc, int crtc_index)
{
	return;
}