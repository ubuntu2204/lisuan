/*
 * @Copyright   Copyright (c) Lisuan Tech. All Rights Reserved
 * @License     Dual MIT/GPLv2
 */

#include "gf_debugfs.h"
#include <drm/drm_debugfs.h>
#include "gf_os_interface.h"
#include <linux/debugfs.h>
#include <drm/drm_device.h>
#include <linux/uaccess.h>
#include "lisuan_device.h"
#include "gf_kms_dm.h"
#include "lisuan_ring.h"
#include "gf_kmd_event.h"
#include "gf_hw_misc.h"

static ssize_t mmio_write(struct file *f, const char __user * buf, size_t size, loff_t * pos)
{
	struct drm_device *ddev = file_inode(f)->i_private;
	struct lisuan_device *gdev = ddev->dev_private;
	struct lisuan_pcie_info *pdev_info = &gdev->pdev_info;
	unsigned int mmio_size = pdev_info->end_addr_io - pdev_info->start_addr_io + 1;
	unsigned char *mmio_base = gdev->mmio_base;
	uint32_t value;

//not check the aligned here
	if (size == 4) {
		get_user(value, (uint32_t *) buf);
		gf_write_mmio32(mmio_base + *pos, value);
	} else if (size == 2) {
		get_user(value, (uint16_t *) buf);
		gf_write_mmio16(mmio_base + *pos, value);
	} else {
		get_user(value, (uint8_t *) buf);
		gf_write_mmio8(mmio_base + *pos, value);
	}

	//gf_printk(GF_DRV_INFO, "write 0x%x 0x%x", (uint32_t)*pos, value);
	return size;
}

static ssize_t mmio_read(struct file *f, char __user * buf, size_t size, loff_t * pos)
{
	struct drm_device *ddev = file_inode(f)->i_private;
	struct lisuan_device *gdev = ddev->dev_private;
	struct lisuan_pcie_info *pdev_info = &gdev->pdev_info;
	unsigned int mmio_size = pdev_info->end_addr_io - pdev_info->start_addr_io + 1;
	unsigned char *mmio_base = gdev->mmio_base;
	uint32_t value = 0;

	if (size == 4) {
		value = gf_read_mmio32(mmio_base + *pos);
		put_user(value, (uint32_t *) buf);
	} else if (size == 2) {
		value = gf_read_mmio16(mmio_base + *pos);
		put_user(value, (uint16_t *) buf);
	} else {
		value = gf_read_mmio8(mmio_base + *pos);
		put_user(value, (uint8_t *) buf);
	}

	//gf_printk(GF_DRV_INFO, "read 0x%x = 0x%x", (uint32_t)*pos, value);
	return size;
}

static const struct file_operations debugfs_reg_fops = {
	.owner = THIS_MODULE,
	.read = mmio_read,
	.write = mmio_write,
	.llseek = default_llseek,
};

#define DP_AUX_MAX_PAYLOAD 16

static ssize_t aux_transfer_write(struct file *f, const char __user * buf, size_t size, loff_t * pos)
{
	struct connector_t *gconnector = file_inode(f)->i_private;
	disp_aux_para_t aux_para = { 0 };
	uint8_t temp_buf[DP_AUX_MAX_PAYLOAD] = { 0 };
	ssize_t ret, todo, i = 0;

	while (size) {
		todo = min(size, sizeof(temp_buf));

		ret = copy_from_user(temp_buf, buf + (i * DP_AUX_MAX_PAYLOAD), todo);
		if (ret)
			break;

		aux_para.op = DISP_DP_NATIVE_AUX_WRITE;
		aux_para.connector_index = gconnector->index;
		aux_para.addr = *pos + (i * DP_AUX_MAX_PAYLOAD);
		aux_para.pbuf = temp_buf;
		aux_para.size = todo;

		if (DISP_OK != gconnector->ddc_func.aux_transfer(gconnector, &aux_para)) {
			gf_error("dp aux transfer fail\n");
			break;
		}

		size = size - todo;
		i++;
	}

	return ret;
}

static ssize_t aux_transfer_read(struct file *f, char __user * buf, size_t size, loff_t * pos)
{
	struct connector_t *gconnector = file_inode(f)->i_private;
	disp_aux_para_t aux_para = { 0 };
	uint8_t temp_buf[DP_AUX_MAX_PAYLOAD] = { 0 };
	ssize_t ret, todo, i = 0;

	while (size) {
		todo = min(size, sizeof(temp_buf));

		aux_para.op = DISP_DP_NATIVE_AUX_READ;
		aux_para.connector_index = gconnector->index;
		aux_para.addr = *pos + (i * DP_AUX_MAX_PAYLOAD);
		aux_para.pbuf = temp_buf;
		aux_para.size = todo;

		if (DISP_OK != gconnector->ddc_func.aux_transfer(gconnector, &aux_para)) {
			gf_error("dp aux transfer fail\n");
			break;
		}

		ret = copy_to_user(buf + (i * DP_AUX_MAX_PAYLOAD), temp_buf, todo);
		if (ret)
			break;

		size = size - todo;
		i++;
	}

	return ret;
}

static ssize_t i2c_over_aux_transfer_read(struct file *f, char __user * buf, size_t size, loff_t * pos)
{
    struct connector_t *gconnector = file_inode(f)->i_private;
    disp_aux_para_t aux_para = { 0 };
    uint8_t temp_buf[DP_AUX_MAX_PAYLOAD] = { 0 };
    ssize_t ret, todo, i = 0;

    while (size) {
        todo = min(size, sizeof(temp_buf));

        aux_para.op = DISP_DP_I2C_OVER_AUX_READ;
        aux_para.connector_index = gconnector->index;
        aux_para.addr = *pos + (i * DP_AUX_MAX_PAYLOAD);
        aux_para.pbuf = temp_buf;
        aux_para.size = todo;

        if (DISP_OK != gconnector->ddc_func.aux_transfer(gconnector, &aux_para)) {
            gf_error("dp i2c over aux transfer fail\n");
            break;
        }

        ret = copy_to_user(buf + (i * DP_AUX_MAX_PAYLOAD), temp_buf, todo);
        if (ret)
            break;

        size = size - todo;
        i++;
    }

    return ret;
}

static unsigned long query_target_pid = 0;
#define MEM_USAGE_PID_BUF_SIZE    128
#define MEM_USAGE_RESULT_BUF_SIZE 8192*4 

static void query_mm_usage(struct lisuan_mman *mmgr,
	unsigned long long *total,
	unsigned long long *used,
	unsigned long long *free)
{
	spin_lock(&mmgr->lock);
	gf_mm_print(mmgr->mm, used, free, total);
	spin_unlock(&mmgr->lock);
}

struct mem_used
{
	unsigned long long visual_vram_used;
	unsigned long long invisual_vram_used;
	unsigned long long system_used;
};

static int
accum_gem_mem_usage(int id, void *ptr, void *data)
{
	struct mem_used * usage = (struct mem_used *)data;
	struct drm_gem_object *obj = ptr;
	struct lisuan_gem_bo *buffer = gem_to_gf_bo(obj);

	mutex_lock(&buffer->lock.lock);

	if(!obj->import_attach && !buffer->evicted){
		if(buffer->domain == dom_visual_vram){
			usage->visual_vram_used += buffer->size;
		}else if(buffer->domain == dom_invisual_vram){
			usage->invisual_vram_used += buffer->size;
		}else if(buffer->domain <= dom_snoop_system_pages){
			usage->system_used += buffer->size;
		}
	}
	mutex_unlock(&buffer->lock.lock);
	return 0;
}

static void query_drmfd_mm_usage(struct lisuan_device * ls_dev, struct lisuan_file * ls_file,
	unsigned long long * visual_vram_used, unsigned long long * invisual_vram_used, unsigned long long * system_used)
{
	struct mem_used usage;

	gf_memset(&usage, 0, sizeof(struct mem_used));

	mutex_lock(&ls_dev->ddev->object_name_lock);
	idr_for_each(&ls_file->drm_file->object_idr,
		&accum_gem_mem_usage, &usage);
	mutex_unlock(&ls_dev->ddev->object_name_lock);

	if(visual_vram_used) *visual_vram_used = usage.visual_vram_used;
	if(invisual_vram_used) *invisual_vram_used = usage.invisual_vram_used;
	if(system_used) *system_used = usage.system_used;
}

static ssize_t mem_usage_read(struct file *f, char __user * buf, size_t size, loff_t * pos)
{
	struct drm_device *ddev = file_inode(f)->i_private;
	struct lisuan_device *ls_dev = ddev->dev_private;
	unsigned long long visual_total = 0, visual_used = 0, visual_free = 0;
	unsigned long long invisual_total = 0, invisual_used = 0, invisual_free = 0;
	unsigned long long system_total = 0, system_free = 0, system_used = 0;
	unsigned int visual_usage_percent = 0, invisual_usage_percent = 0, system_usage_percent = 0;
	struct lisuan_file *ls_file = NULL;
	struct sysinfo *si = NULL;
	char *tmp = NULL;
	int len = 0;
	ssize_t ret = 0;

	if (*pos != 0)
		return 0;

	tmp = kmalloc(MEM_USAGE_RESULT_BUF_SIZE, GFP_KERNEL);
	if (!tmp){
		ret = -ENOMEM;
		goto exit;
	}
	si = kmalloc(sizeof(struct sysinfo), GFP_KERNEL);
	if (!si){
		ret = -ENOMEM;
		goto exit;
	}

	si_meminfo(si);
	system_total = si->totalram;
	system_free  = si->freeram;
	system_total = system_total << ls_dev->sys_page_shift;
	system_free  = system_free  << ls_dev->sys_page_shift;
	system_used  = system_total - system_free;
	len += snprintf(tmp + len, MEM_USAGE_RESULT_BUF_SIZE - len,
						"System        memory: total: %8llu MB, used %8llu MB (%2llu.%1llu%%)\n",
						system_total >> 20, system_used >> 20,
						100*system_used/system_total, (1000*system_used/system_total)%10);
	
	query_mm_usage(&ls_dev->mmgr[dom_visual_vram - 1],
						&visual_total, &visual_used, &visual_free);
	
	query_mm_usage(&ls_dev->mmgr[dom_invisual_vram - 1],
						&invisual_total, &invisual_used, &invisual_free);
	
	visual_usage_percent   = visual_total ? visual_used * 1000 / visual_total  : 0;
	invisual_usage_percent = invisual_total ? invisual_used * 1000 / invisual_total  : 0;
	
	len += snprintf(tmp + len, MEM_USAGE_RESULT_BUF_SIZE - len,
						"Visual   vram memory: total: %8llu MB, used %8llu MB (%2u.%1u%%)\n"
						"Invisual vram memory: total: %8llu MB, used %8llu MB (%2u.%1u%%)\n",
						visual_total >> 20, visual_used >> 20, visual_usage_percent / 10 , visual_usage_percent ? visual_usage_percent % 10 : 0,
						invisual_total >> 20, invisual_used >> 20, invisual_usage_percent / 10 , invisual_usage_percent ? invisual_usage_percent % 10 : 0);

	len += snprintf(tmp + len, MEM_USAGE_RESULT_BUF_SIZE - len,
									"---- Usage Detail ----\n");
	
	mutex_lock(&ls_dev->file_lock.lock);
	list_for_each_entry(ls_file, &ls_dev->file_list, dev_object)
	{
			if(query_target_pid && query_target_pid != ls_file->pid)
					continue;
	
			query_drmfd_mm_usage(ls_dev, ls_file, &visual_used, &invisual_used, &system_used);
	
			len += snprintf(tmp + len, MEM_USAGE_RESULT_BUF_SIZE - len,
					"PID %8lu(%-20s used by %20s(PID:%8lu)) memory used: vram %8llu MB(%2llu.%1llu%%), system ram %8llu MB(%2llu.%1llu%%)\n",
					ls_file->tgid, ls_file->name, ls_file->usedname, ls_file->used_pid,
					(visual_used + invisual_used) >> 20, 100*(visual_used + invisual_used)/(visual_total+invisual_total), (1000*(visual_used + invisual_used)/(visual_total+invisual_total))%10,
					system_used >> 20, 100*system_used/system_total, (1000*system_used/system_total)%10);
	}
	mutex_unlock(&ls_dev->file_lock.lock);


	query_target_pid = 0;

	if (copy_to_user(buf, tmp, len)){
		ret = -EFAULT;
		goto exit;
	}

	*pos += len;
exit:
	if(tmp) kfree(tmp);
	if(si) kfree(si);
	return len;
}

static ssize_t mem_usage_write(struct file *file, const char __user *buf,
							size_t count, loff_t *ppos)
{
	struct drm_device *ddev = file_inode(file)->i_private;
	struct lisuan_device *gdev = ddev->dev_private;
	unsigned long pid = 0;
	char *kbuf = NULL;
	int i = 0;

	if (count == 0 || count == 1)
		return 0;

	if (count > MEM_USAGE_PID_BUF_SIZE - 1)
		count = MEM_USAGE_PID_BUF_SIZE - 1;

	kbuf = kmalloc(MEM_USAGE_PID_BUF_SIZE, GFP_KERNEL);
	if (!kbuf)
		return -ENOMEM;

	if (copy_from_user(kbuf, buf, count)) {
		kfree(kbuf);
		return -EFAULT;
	}

	for (i = 0; i < count - 1; i++) {
		if (kbuf[i] < '0' || kbuf[i] > '9') {
			pr_warn("mem_usage_write: invalid PID string\n");
			kfree(kbuf);
			return -EINVAL;
		}
	}

	if (kstrtoul(kbuf, 10, &pid)) {
		pr_warn("mem_usage_write: failed to convert to unsigned long\n");
		kfree(kbuf);
		return -EINVAL;
	}
	query_target_pid = pid;

	kfree(kbuf);
	return count;
}

static const struct file_operations debugfs_aux_fops = {
	.owner = THIS_MODULE,
	.read = aux_transfer_read,
	.write = aux_transfer_write,
	.llseek = default_llseek,
};

static const struct file_operations debugfs_i2c_fops = {
    .owner = THIS_MODULE,
    .read = i2c_over_aux_transfer_read,
    .llseek = default_llseek,
};

static const struct file_operations debugfs_mem_usage = {
	.owner = THIS_MODULE,
	.read = mem_usage_read,
	.write = mem_usage_write,
	.llseek = default_llseek,
};
static ssize_t displayport_test_active_write(struct file *f,
						  const char __user *ubuf,
						  size_t len, loff_t *offp)
{
	struct connector_t *gconnector = file_inode(f)->i_private;
	char *input_buffer;
	int status = 0;
	struct drm_connector *connector = &gconnector->base;
	struct connector_state_t *connector_state = dconnector_state_to_gconnector(connector->state);
	int val = 0;

	if (len == 0)
		return 0;

	input_buffer = memdup_user_nul(ubuf, len);
	if (IS_ERR(input_buffer))
		return PTR_ERR(input_buffer);

	gf_debug("Copied %d bytes from user\n", (unsigned int)len);

	if (connector->connector_type != DRM_MODE_CONNECTOR_DisplayPort)
		return -1;

	if (connector->encoder && connector->status == connector_status_connected) {
		status = kstrtoint(input_buffer, 10, &val);
		if (status < 0)
			return -1;
		gf_debug("Got %d for test active\n", val);
		/* To prevent erroneous activation of the compliance
			* testing code, only accept an actual value of 1 here
			*/
		if (val == 1)
			connector_state->compliance.test_active = true;
		else
			connector_state->compliance.test_active = false;
	}

	kfree(input_buffer);
	if (status < 0)
		return status;

	*offp += len;
	return len;
}

static int displayport_test_active_show(struct seq_file *m, void *data)
{
	struct connector_t *gconnector = dconnector_to_gconnector(m->private);
	struct drm_connector *connector = &gconnector->base;
	struct connector_state_t *gconnector_state = dconnector_state_to_gconnector(connector->state);

	if (connector->connector_type != DRM_MODE_CONNECTOR_DisplayPort)
		return -1;

	if (connector->encoder && connector->status == connector_status_connected) {
		if (gconnector_state->compliance.test_active)
			seq_puts(m, "1");
		else
			seq_puts(m, "0");
	} else
		seq_puts(m, "0");

	return 0;
}

static int displayport_test_active_open(struct inode *inode,
					     struct file *file)
{
	return single_open(file, displayport_test_active_show,
			   inode->i_private);
}

static const struct file_operations displayport_test_active_fops = {
	.owner = THIS_MODULE,
	.open = displayport_test_active_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	.write = displayport_test_active_write
};

static int displayport_test_data_show(struct seq_file *m, void *data)
{
	struct connector_t *gconnector = dconnector_to_gconnector(m->private);
	struct drm_connector *connector = &gconnector->base;
	struct connector_state_t *gconnector_state = dconnector_state_to_gconnector(connector->state);

	if (connector->connector_type != DRM_MODE_CONNECTOR_DisplayPort)
		return -1;

	if (connector->encoder && connector->status == connector_status_connected) {
		if (gconnector_state->compliance.test_type ==
			GF_DP_TEST_LINK_EDID_READ)
			seq_printf(m, "%lx",
					gconnector_state->compliance.test_data.edid);
		else if (gconnector_state->compliance.test_type ==
				GF_DP_TEST_LINK_VIDEO_PATTERN) {
			seq_printf(m, "hdisplay: %d\n",
					gconnector_state->compliance.test_data.hdisplay);
			seq_printf(m, "vdisplay: %d\n",
					gconnector_state->compliance.test_data.vdisplay);
			seq_printf(m, "bpc: %u\n",
					gconnector_state->compliance.test_data.bpc);
			seq_printf(m, "pattern: %u\n",
					gconnector_state->compliance.test_data.video_pattern);
			seq_printf(m, "format: %u\n",
					gconnector_state->compliance.test_data.color_format);
			seq_printf(m, "range_type: %u\n",
					gconnector_state->compliance.test_data.range_type);
		} else if (gconnector_state->compliance.test_type ==
				DP_TEST_LINK_PHY_TEST_PATTERN) {
			seq_printf(m, "pattern: %d\n",
					gconnector_state->compliance.test_data.phytest.phy_pattern);
			seq_printf(m, "Number of lanes: %d\n",
					gconnector_state->compliance.test_data.phytest.num_lanes);
			seq_printf(m, "Link Rate: %d\n",
					gconnector_state->compliance.test_data.phytest.link_rate);
			seq_printf(m, "level: %02x\n",
					gconnector->train_set[0]);
		}
	} else
		seq_puts(m, "0");

	return 0;
}

static int displayport_test_data_open(struct inode *inode, struct file *file)
{
	return single_open(file, displayport_test_data_show, inode->i_private);
}

static const struct file_operations displayport_test_data_fops = {
	.owner		= THIS_MODULE,
	.open		= displayport_test_data_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int displayport_test_type_show(struct seq_file *m, void *data)
{
	struct connector_t *gconnector = dconnector_to_gconnector(m->private);
	struct drm_connector *connector = &gconnector->base;
	struct connector_state_t *gconnector_state = dconnector_state_to_gconnector(connector->state);

	if (connector->connector_type != DRM_MODE_CONNECTOR_DisplayPort)
		return -1;

	if (connector->encoder && connector->status == connector_status_connected) {
		seq_printf(m, "%lx\n", gconnector_state->compliance.test_type);
	} else
		seq_puts(m, "0");

	return 0;
}

static int displayport_test_type_open(struct inode *inode, struct file *file)
{
	return single_open(file, displayport_test_type_show, inode->i_private);
}

static const struct file_operations displayport_test_type_fops = {
	.owner		= THIS_MODULE,
	.open		= displayport_test_type_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int sched_status_show(struct seq_file *m, void *data)
{
	struct drm_device *ddev = m->private;
	struct lisuan_device *ls_dev = ddev->dev_private;

	lisuan_sched_status_show(ls_dev);
	seq_puts(m, "0");

	return 0;
}

static int sched_status_open(struct inode *inode, struct file *file)
{
	return single_open(file, sched_status_show, inode->i_private);
}

static const struct file_operations debugfs_sched_status_fops = {
	.owner		= THIS_MODULE,
	.open		= sched_status_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int ring_job_dump(struct seq_file *m, void *data)
{
	struct drm_device *ddev = m->private;
	struct lisuan_device *ls_dev = ddev->dev_private;

	lisuan_ring_dump_jobs(ls_dev);
	seq_puts(m, "0");

	return 0;
}

static int ring_open(struct inode *inode, struct file *file)
{
	return single_open(file, ring_job_dump, inode->i_private);
}

static const struct file_operations debugfs_ring_fops = {
	.owner		= THIS_MODULE,
	.open		= ring_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int debugbus_show(struct seq_file *m, void *data)
{
	struct drm_device *ddev = m->private;
	struct lisuan_device *ls_dev = ddev->dev_private;

	lisuan_dump_debugbus(ls_dev);
	seq_puts(m, "0");

	return 0;
}

static int debugbus_open(struct inode *inode, struct file *file)
{
	return single_open(file, debugbus_show, inode->i_private);
}

static const struct file_operations debugfs_debugbus_fops = {
	.owner		= THIS_MODULE,
	.open		= debugbus_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static const struct file_operations debugfs_kmdEvent_log_fops = {
	.owner = THIS_MODULE,
	.read = gf_kmdEvent_log_read,
	.write = gf_kmdEvent_log_write,
	.poll = gf_kmdEvent_log_poll,
};

static const struct file_operations debugfs_gpu_usage_fops = {
	.owner = THIS_MODULE,
	.open = gf_gpuusage_open,
	.read = gf_gpuusage_read,
	.write = gf_gpuusage_write,
	.llseek = default_llseek,
};

static ssize_t hw_infos_read(struct file *f, char __user * buf, size_t size, loff_t * pos)
{
	struct drm_device *ddev = file_inode(f)->i_private;
	struct lisuan_device *ls_dev = ddev->dev_private;
	struct module_clk clk= {0};
	struct voltage vol= {0};
	struct therm thr = {0};
	struct misc_info info = {0};
	unsigned char fm_v[32]={0};
	unsigned int customerid, deviceid, i, fan_speed;
	char *tmp = NULL;
	int len = 0;
	ssize_t ret = 0;

	if (*pos != 0)
		return 0;

	tmp = kmalloc(MEM_USAGE_RESULT_BUF_SIZE, GFP_KERNEL);
	if (!tmp){
		ret = -ENOMEM;
		goto exit;
	}

	mutex_lock(&ls_dev->mailbox_lock.lock);
	gf_get_hw_FM_version(ls_dev->mmio_base, fm_v);
	len += snprintf(tmp + len, MEM_USAGE_RESULT_BUF_SIZE - len,
		"FM version:%s\n", fm_v);

	if(ls_dev->debugfs_detail)
	{
		gf_get_hw_clk(ls_dev->mmio_base, &clk);
		len += snprintf(tmp + len, MEM_USAGE_RESULT_BUF_SIZE - len,
		"\nHW modules clk:\n");

		len += snprintf(tmp + len, MEM_USAGE_RESULT_BUF_SIZE - len,
			"Eng clk:%d MHz\n", clk.gpc_clk);
		len += snprintf(tmp + len, MEM_USAGE_RESULT_BUF_SIZE - len,
			"CGM clk:%d MHz, GL2 clk:%d MHz, GPC clk:%d MHz\n", clk.cgm_clk, clk.gl2_clk, clk.gpc_clk);
		len += snprintf(tmp + len, MEM_USAGE_RESULT_BUF_SIZE - len,
			"vdd0 clk:%d MHz, vdd1 clk:%d MHz, vem clk:%d MHz\n", clk.vdd0_clk, clk.vdd1_clk, clk.vem_clk);
		len += snprintf(tmp + len, MEM_USAGE_RESULT_BUF_SIZE - len,
			"MHB clk:%d MHz\n", clk.mhb_clk);
	}

	gf_get_hw_voltage(ls_dev->mmio_base, &vol);
	len += snprintf(tmp + len, MEM_USAGE_RESULT_BUF_SIZE - len,
		"\nHW power:\n");
	len += snprintf(tmp + len, MEM_USAGE_RESULT_BUF_SIZE - len,
		"Power:%d W\n", vol.total_power);
	if(ls_dev->debugfs_detail)
	{
		len += snprintf(tmp + len, MEM_USAGE_RESULT_BUF_SIZE - len,
			"total power:%d W, engine_power:%d W, mem_power:%d W\n", vol.total_power , vol.engine_power, vol.mem_power);
		len += snprintf(tmp + len, MEM_USAGE_RESULT_BUF_SIZE - len,
			"\nHW voltage:\n");
		len += snprintf(tmp + len, MEM_USAGE_RESULT_BUF_SIZE - len,
			"engine_voltage:%d mV, soc_voltage:%d mV\n", vol.engine_voltage , vol.soc_voltage);
	}

	gf_get_hw_therm(ls_dev->mmio_base, &thr);
	gf_get_hw_therm_fan_speed(ls_dev->mmio_base, &fan_speed);
	len += snprintf(tmp + len, MEM_USAGE_RESULT_BUF_SIZE - len,
		"\nHW temperature:\n");

	len += snprintf(tmp + len, MEM_USAGE_RESULT_BUF_SIZE - len,
		"Avg Temp:%d C\n", (thr.therm[0] + thr.therm[1] + thr.therm[2] + thr.therm[3] + thr.therm[4] + thr.therm[5])/6);

	if(ls_dev->debugfs_detail)
	{
		for(i=0; i<6; i++)
		{
			len += snprintf(tmp + len, MEM_USAGE_RESULT_BUF_SIZE - len,
				"therm[%d]:%d C\n", i, thr.therm[i]);
		}
	}
	len += snprintf(tmp + len, MEM_USAGE_RESULT_BUF_SIZE - len,
		"fan speed:%d r/min\n", fan_speed);

	if(ls_dev->debugfs_detail)
	{
		len += snprintf(tmp + len, MEM_USAGE_RESULT_BUF_SIZE - len,
			"\nGpu misc infos:\n");
		gf_get_misc_infos(ls_dev->mmio_base, &info);
		len += snprintf(tmp + len, MEM_USAGE_RESULT_BUF_SIZE - len,
		"Real:%s\n", info.realMemroyinfos);
		len += snprintf(tmp + len, MEM_USAGE_RESULT_BUF_SIZE - len,
		"FM:%s\n", info.firmwareMemroyinfos);
		len += snprintf(tmp + len, MEM_USAGE_RESULT_BUF_SIZE - len,
		"ClusterMask:%d\n", info.gpcMask);
	}

	if(ls_dev->debugfs_detail)
	{
		gf_get_customer_id(ls_dev->mmio_base, &customerid);
		len += snprintf(tmp + len, MEM_USAGE_RESULT_BUF_SIZE - len,
			"\ncustomer id:0x%x\n", customerid);

		gf_get_device_id(ls_dev->mmio_base, &deviceid);
		len += snprintf(tmp + len, MEM_USAGE_RESULT_BUF_SIZE - len,
			"\ndevice id:0x%x\n", deviceid);
	}

	mutex_unlock(&ls_dev->mailbox_lock.lock);

	if (copy_to_user(buf, tmp, len)){
		ret = -EFAULT;
		goto exit;
	}

	*pos += len;
exit:
	if(tmp) kfree(tmp);
	return len;

}

ssize_t hw_infos_write(struct file *file, const char __user *user_buf, size_t count, loff_t *offp)
{
	struct drm_device *ddev = file_inode(file)->i_private;
	struct lisuan_device *ls_device = ddev->dev_private;

	char set_cmd[25] = {0};
	if (count >= sizeof(set_cmd))
	{
		return -EINVAL;
	}
	if (copy_from_user(set_cmd, user_buf, count))
	{
		return -EFAULT;
	}

	if (strstr(set_cmd, "detail"))
	{
		sscanf(set_cmd, "detail %d", &ls_device->debugfs_detail);
	}

	return count;
}


static ssize_t hw_temperature_read(struct file *f, char __user * buf, size_t size, loff_t * pos)
{
	struct drm_device *ddev = file_inode(f)->i_private;
	struct lisuan_device *ls_dev = ddev->dev_private;
	char *tmp = NULL;
	int len = 0;
	ssize_t ret = 0;
	struct therm thr = {0};
	unsigned i=0;


	if (*pos != 0)
		return 0;

	tmp = kmalloc(MEM_USAGE_RESULT_BUF_SIZE, GFP_KERNEL);
	if (!tmp){
		ret = -ENOMEM;
		goto exit;
	}

	mutex_lock(&ls_dev->mailbox_lock.lock);
	gf_get_hw_therm(ls_dev->mmio_base, &thr);
	len += snprintf(tmp + len, MEM_USAGE_RESULT_BUF_SIZE - len,
		"HW temperature:\n");
	for(i=0; i<6; i++)
	{
		len += snprintf(tmp + len, MEM_USAGE_RESULT_BUF_SIZE - len,
			"therm[%d]:%d C\n", i, thr.therm[i]);
	}
	mutex_unlock(&ls_dev->mailbox_lock.lock);

	if (copy_to_user(buf, tmp, len)){
		ret = -EFAULT;
		goto exit;
	}

	*pos += len;
exit:
	if(tmp) kfree(tmp);
	return len;
}

static const struct file_operations debugfs_hw_infos = {
	.owner = THIS_MODULE,
	.read = hw_infos_read,
	.write = hw_infos_write,
	.llseek = default_llseek,
};

static const struct file_operations debugfs_hw_temperature = {
	.owner = THIS_MODULE,
	.read = hw_temperature_read,
	.llseek = default_llseek,
};

static ssize_t fan_speed_read(struct file *f, char __user * buf, size_t size, loff_t * pos)
{
	struct drm_device *ddev = file_inode(f)->i_private;
	struct lisuan_device *ls_dev = ddev->dev_private;
	char *tmp = NULL;
	int len = 0;
	ssize_t ret = 0;
	unsigned int speed = 0;

	if (*pos != 0)
		return 0;

	tmp = kmalloc(MEM_USAGE_RESULT_BUF_SIZE, GFP_KERNEL);
	if (!tmp){
		ret = -ENOMEM;
		goto exit;
	}
	mutex_lock(&ls_dev->mailbox_lock.lock);
	gf_get_hw_therm_fan_speed(ls_dev->mmio_base, &speed);
	len += snprintf(tmp + len, MEM_USAGE_RESULT_BUF_SIZE - len,
						"fan speed: %d r/min\n", speed);
	mutex_unlock(&ls_dev->mailbox_lock.lock);
	if (copy_to_user(buf, tmp, len)){
		ret = -EFAULT;
		goto exit;
	}

	*pos += len;
exit:
	if(tmp) kfree(tmp);
	return len;
}

static const struct file_operations debugfs_fan = {
	.owner = THIS_MODULE,
	.read = fan_speed_read,
	.llseek = default_llseek,
};

static const struct {
	char *name;
	const struct file_operations *fops;
	umode_t mode;
} gf_debugfs_list[] = {
	{ "reg", &debugfs_reg_fops, 0664},
	{ "ring", &debugfs_ring_fops, 0664},
	{ "debugbus", &debugfs_debugbus_fops, 0664},
	{ "sched_status", &debugfs_sched_status_fops, 0664},
	{ "kmdEvent_log", &debugfs_kmdEvent_log_fops, 0664},
	{ "gpu_usage", &debugfs_gpu_usage_fops, 0664},
	{ "mem_usage",&debugfs_mem_usage, 0664},
	{ "hw_infos",&debugfs_hw_infos, 0664},
	{ "hw_temperature",&debugfs_hw_temperature, 0664},
	{ "fan",&debugfs_fan, 0664},
};

static const struct {
	char *name;
	const struct file_operations *fops;
} dp_debugfs_entries[] = {
	{"aux", &debugfs_aux_fops},
	{"i2c", &debugfs_i2c_fops},
	{"dp_test_data", &displayport_test_data_fops},
	{"dp_test_type", &displayport_test_type_fops},
	{"dp_test_active", &displayport_test_active_fops},
};

//here should check why rendor device also have these files generate
#if DRM_VERSION_CODE < KERNEL_VERSION(5,8,0)
int gf_debugfs_init(struct drm_minor *minor)
#else
void gf_debugfs_init(struct drm_minor *minor)
#endif
{
	struct drm_info_node *tmp;
	int i;

	if (minor->dev->primary != minor)
#if DRM_VERSION_CODE < KERNEL_VERSION(5,8,0)
		return 0;
#else
		return;
#endif


	for (i = 0; i < ARRAY_SIZE(gf_debugfs_list); i++) {
		debugfs_create_file(gf_debugfs_list[i].name, gf_debugfs_list[i].mode, minor->debugfs_root, minor->dev, gf_debugfs_list[i].fops);
	}
#if DRM_VERSION_CODE < KERNEL_VERSION(5,8,0)
	return 0;
#endif
}

void connector_debugfs_init(struct connector_t *gconnector)
{
	int i;
	struct dentry *dir = gconnector->base.debugfs_entry;

	if (gconnector->base.connector_type == DRM_MODE_CONNECTOR_DisplayPort ||
	    gconnector->base.connector_type == DRM_MODE_CONNECTOR_eDP) {
		for (i = 0; i < ARRAY_SIZE(dp_debugfs_entries); i++) {
			debugfs_create_file(dp_debugfs_entries[i].name,
					    0644, dir, gconnector,
					    dp_debugfs_entries[i].fops);
		}
	}
}
