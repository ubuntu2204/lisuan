/*
 * @Copyright   Copyright (c) Lisuan Tech. All Rights Reserved
 * @License     Dual MIT/GPLv2
 */

#ifndef _VC_9000_COMMON_H_
#define _VC_9000_COMMON_H_

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_CMDBUF_PRIORITY_TYPE   2 //0:normal priority,1:high priority
#define CMDBUF_PRIORITY_NORMAL     0
#define CMDBUF_PRIORITY_HIGH       1

/******************************************************************************
 * VCMD HW version IDs
 ******************************************************************************/
#define VCMD_HW_ID					0x4342
#define HW_ID_1_0_C                0x43421001
#define HW_ID_1_1_1                0x43421101
#define HW_ID_1_1_2                0x43421102
#define HW_ID_1_1_3                0x43421103
#define HW_ID_1_2_1                0x43421201
#define HW_ID_1_5_0                0x43421500
#define HW_ID_1_5_2                0x43421502

/*module_type support*/

enum vcmd_module_type {
	VCMD_TYPE_ENCODER = 0,
	VCMD_TYPE_CUTREE,
	VCMD_TYPE_DECODER,
	VCMD_TYPE_JPEG_ENCODER,
	VCMD_TYPE_JPEG_DECODER,
	MAX_VCMD_TYPE
};

#define VCMD_ARBITER_TIME_WINDOW_EXP 0x1D

#define MAX_SUBSYS_NUM 4 /* up to 4 subsystem (temporary) */

#ifdef __cplusplus
}
#endif

#endif