/*
 * @Copyright   Copyright (c) Lisuan Tech. All Rights Reserved
 * @License     Dual MIT/GPLv2
 */

#ifndef __GF_KMS_HDR_H__
#define __GF_KMS_HDR_H__

#include "lisuan_drm.h"

#define HDR_CSC_RGB2Y_LEN 3
#define HDR_SAT_TYPE_COUNT 1
typedef enum _HdrSaturationType_e
{
    HdrSaturationType_Off = 0,
    HdrSaturationType_0_25,        // satgain = 0.25
} HdrSaturationType_e;

typedef enum _HdrRGB2RGBType_e
{
    HdrRGB2RGBTypeBypass = 0,

    HdrRGB2RGBTypeSrgbToAdobeRgb,
    HdrRGB2RGBTypeSrgbToBt2020,
    HdrRGB2RGBTypeSrgbToDciP3,
    HdrRGB2RGBTypeSrgbToBt601_525,
    HdrRGB2RGBTypeSrgbToBt601_625,

    HdrRGB2RGBTypeAdobeRgbToSrgb,
    HdrRGB2RGBTypeAdobeRgbToBt2020,
    HdrRGB2RGBTypeAdobeRgbToDciP3,
    HdrRGB2RGBTypeAdobeRgbToBt601_525,
    HdrRGB2RGBTypeAdobeRgbToBt601_625,

    HdrRGB2RGBTypeBt2020ToSrgb,
    HdrRGB2RGBTypeBt2020ToAdobeRgb,
    HdrRGB2RGBTypeBt2020ToDciP3,
    HdrRGB2RGBTypeBt2020ToBt601_525,
    HdrRGB2RGBTypeBt2020ToBt601_625,

    HdrRGB2RGBTypeDciP3ToSrgb,
    HdrRGB2RGBTypeDciP3ToAdobeRgb,
    HdrRGB2RGBTypeDciP3ToBt2020,
    HdrRGB2RGBTypeDciP3ToBt601_525,
    HdrRGB2RGBTypeDciP3ToBt601_625,

    HdrRGB2RGBTypeBt601_525ToSrgb,
    HdrRGB2RGBTypeBt601_525ToAdobeRgb,
    HdrRGB2RGBTypeBt601_525ToBt2020,
    HdrRGB2RGBTypeBt601_525ToDciP3,
    HdrRGB2RGBTypeBt601_525ToBt601_625,

    HdrRGB2RGBTypeBt601_625ToSrgb,
    HdrRGB2RGBTypeBt601_625ToAdobeRgb,
    HdrRGB2RGBTypeBt601_625ToBt2020,
    HdrRGB2RGBTypeBt601_625ToDciP3,
    HdrRGB2RGBTypeBt601_625ToBt601_525,

    HdrRGB2RGBTypeInvalid,
} HdrRGB2RGBType_e;

typedef enum _HdrTfInOutType_e // PQ <--> HLG invalid
{
    HdrTfInOutTypeSDR2SDR = 0,
    HdrTfInOutTypeSDR2PQ,
    HdrTfInOutTypeSDR2HLG,
    HdrTfInOutTypePQ2SDR,
    HdrTfInOutTypePQ2PQ,
    HdrTfInOutTypeHLG2SDR,
    HdrTfInOutTypeHLG2HLG,

    HdrTfInOutTypeInvalid,
} HdrTfInOutType_e;

typedef struct _HdrColorPosition_t
{
    struct
    {
        unsigned int x;
        unsigned int y;
    }rgb[3];
    struct
    {
        unsigned int x;
        unsigned int y;
    }white;
}HdrColorPosition_t;

typedef struct _HdrMgr_t {
    unsigned char hdrEnable;
    HdrSaturationType_e satType;
    hdr_tf_type_e tfIn;
    hdr_tf_type_e tfOut;
    hdr_cs_type_e csIn;
    hdr_cs_type_e csOut;

    int maxLin;
    int minLout_1_10000;    // 1/10000 nits
    int maxLout;
    int maxFall;

    unsigned int degammaLutY[HDR_DEGAMMA_LUT_LENGTH];
    unsigned int degammaLutS[HDR_DEGAMMA_LUT_LENGTH - 1];
} HdrMgr_t;

void HdrDegamma(HdrMgr_t* pHdrMgr, void* pDispHdr);
void HdrRgb2Y(hdr_cs_type_e csInType, void* pDispHdr);
void HdrSaturation(HdrSaturationType_e satType, void* pDispHdr);
void HdrDegammaStretch(hdr_tf_type_e tfInType, hdr_tf_type_e tfOutType, unsigned int maxlin, unsigned int maxlout, unsigned int* pDgStretchA, int* pDStretchB);
void HdrColorSpaceConversion(hdr_cs_type_e csInType, hdr_cs_type_e csOutType, void* pDispHdr);
void HdrGamma(hdr_tf_type_e tfOutType, void* pDispHdr);
void HdrGammaStretch(unsigned int* pGStretchA,  int* pGStretchB);
void HdrUpdateOutMetadata(HdrMgr_t* pHdrMgr, void* pDispHdr);
int HdrUpdate(HdrMgr_t* pHdrMgr, void* pDispHdr);
void HdrDump(HdrMgr_t* pHdrMgr, void* pDispHdr);

#endif
