#ifndef RTSIM_KMD_SHARE_H
#define RTSIM_KMD_SHARE_H

#include <stdint.h>
#include <ntddk.h>
#include <windef.h> 
#include <wingdi.h>
#include <winerror.h>
#include <dispmprt.h>
#include <d3dkmddi.h>
#include <d3dkmthk.h>

typedef struct
{
    uint32_t bNullCmodel;
    uint32_t bNullRendering;
    uint32_t deviceId;
    uint32_t bForceLocal;
    uint32_t bForceKmdLocal;    
    uint32_t bForceBypass;
    uint32_t bGpuMemoryTest;
    uint32_t localVisableSegPageSize;
    uint32_t localInvisableSegPageSize;
    uint32_t pcieNonSnoopSegPageSize;
    uint32_t pcieSnoopSegPageSize;
    uint32_t mcEnMask;
    uint32_t glbSlcMask;
    uint32_t bVideoCodecDump;
    uint32_t videoMemType;
    uint32_t videoMemSizeMb;
    uint32_t bVideoMemClearOp;
    uint32_t bVideohighResolution;
    uint32_t videoVsiProcessId;
    uint32_t bVideoHWPerformance;
    uint32_t videoMMUCfg;
    uint32_t bVideoGl2Enable;
    uint32_t bVideoWaitAtEnd;
    uint32_t videoClocksAlwaysOn;
} RS_CONFIG;

typedef NTSTATUS (*PFN_DxgkInitialize)(
    PDRIVER_OBJECT              DriverObject,
    PUNICODE_STRING             RegistryPath,
    PDRIVER_INITIALIZATION_DATA DriverInitializationData);

typedef PVOID (*PFN_ExAllocatePoolWithTag)(
    ULONG PoolType,
    ULONG ByteCount,
    ULONG PoolTag);

typedef VOID (*PFN_ExFreePool)(
    PVOID pPoolMem);

typedef VOID (*PFN_GetMemInfo)(
        UINT64 *pCpuVisableLocalSize,
        UINT64 *pCpuInvisableLocalSize,
        UINT64 *pPcieSize,
        UINT64 *pCpuVisableLocalCpuPa,
        UINT64 *pCpuInvisableLocalCpuPa);

typedef PMDL (*PFN_MmAllocatePagesForMdl)(
    _In_ PHYSICAL_ADDRESS LowAddress,
    _In_ PHYSICAL_ADDRESS HighAddress,
    _In_ PHYSICAL_ADDRESS SkipBytes,
    _In_ SIZE_T TotalBytes);

typedef VOID (*PFN_MmFreePagesFromMdl)(
    _Inout_ PMDL MemoryDescriptorList);

typedef ULONG (*PFN_READ_REGISTER_ULONG)(
    _In_ volatile ULONG *Register);

typedef void (*PFN_WRITE_REGISTER_ULONG)(
    _In_ volatile ULONG *Register,
    _In_ ULONG Value);

typedef UCHAR (*PFN_READ_REGISTER_UCHAR)(
    _In_ volatile UCHAR *Register);

typedef void (*PFN_WRITE_REGISTER_UCHAR)(
    _In_ volatile UCHAR *Register,
    _In_ UCHAR Value);

typedef void (*PFN_WRITE_REGISTER_ULONG_FOR_VIDEO_CODEC)(
    _In_ volatile ULONG *Register,
    _In_ ULONG Value);

typedef int (*PFN_CMODEL_OP_FUNC)(
    _In_ CHAR* op_name,
    _In_ PVOID op_arg);

typedef void (*PFN_CFG_QUERY_STRING) (
    _In_ CHAR* key,
    _Out_ CHAR *value,
    _In_ ULONG len);

typedef int (*PFN_CFG_QUERY_INT) (
    _In_ CHAR* key);

typedef void (*PFN_RT_CALLBACK_FUNC)(
    _In_ CHAR* op_name,
    _In_ PVOID InputBuffer,
    _In_ UINT InputBufferLen,
    _Out_ PVOID OutputBuffer,
    _In_ UINT OutputBufferLen);

typedef int (*PFN_DCD_DUMP_OP_FUNC)(
    _In_ CHAR* op_name,
    _In_ PVOID op_arg);

typedef struct {
    PFN_DxgkInitialize pfnDxgkInitialize;
    PFN_ExAllocatePoolWithTag pfnExAllocatePoolWithTag;
    PFN_ExFreePool pfnExFreePool;
    PFN_GetMemInfo pfnGetMemInfo;
    PFN_MmAllocatePagesForMdl pfnMmAllocatePagesFroMdl;
    PFN_MmFreePagesFromMdl pfnMmFreePagesFromMdl;
    PFN_READ_REGISTER_ULONG pfnReadRegisterUlong;
    PFN_WRITE_REGISTER_ULONG pfnWriteRegisterUlong;
    PFN_READ_REGISTER_UCHAR pfnReadRegisterUchar;
    PFN_WRITE_REGISTER_UCHAR pfnWriteRegisterUchar;
    PFN_WRITE_REGISTER_ULONG_FOR_VIDEO_CODEC pfnWriteRegisterUlongForVideoCodec;
    PFN_CMODEL_OP_FUNC pfnCmodelOpFunc;
    PFN_CFG_QUERY_STRING pfnCfgQueryString;
    PFN_CFG_QUERY_INT pfnCfgQueryInt;
    PFN_DCD_DUMP_OP_FUNC pfnDcdDumpOpFunc;
    PFN_RT_CALLBACK_FUNC pfnRTCallBackFunc;

    RS_CONFIG rsConfig;
} RsStubFunc;

#endif /*RTSIM_KMD_SHARE_H*/
