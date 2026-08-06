/*
 * @Copyright   Copyright (c) Lisuan Tech. All Rights Reserved
 * @License     Dual MIT/GPLv2
 */

#include "gf_kms_hdr.h"
#include "gf_kms.h"
#include "gf_os_interface.h"

/* HDR Config Parameter: RGB -> Y */
static unsigned short satRgb2Y[hdr_cs_type_end][HDR_CSC_RGB2Y_LEN] = {   // hdr_cs_type_e, 6x3
    {0x367 /* 0.212646484375, UQ0.12 */, 0xb71 /* 0.715087890625, UQ0.12 */, 0x128 /* 0.072265625000, UQ0.12 */},
    {0x4c2 /* 0.297363281250, UQ0.12 */, 0xa0a /* 0.627441406250, UQ0.12 */, 0x134 /* 0.075195312500, UQ0.12 */},
    {0x434 /* 0.262695312500, UQ0.12 */, 0xad9 /* 0.677978515625, UQ0.12 */, 0xf3 /* 0.059326171875, UQ0.12 */},
    {0x3aa /* 0.229003906250, UQ0.12 */, 0xb11 /* 0.691650390625, UQ0.12 */, 0x145 /* 0.079345703125, UQ0.12 */},
    {0x366 /* 0.212402343750, UQ0.12 */, 0xb38 /* 0.701171875000, UQ0.12 */, 0x163 /* 0.086669921875, UQ0.12 */},
    {0x38d /* 0.221923828125, UQ0.12 */, 0xb4e /* 0.706542968750, UQ0.12 */, 0x124 /* 0.071289062500, UQ0.12 */}
};

/* HDR Config Parameter: Saturation LUT */
static unsigned int satLutY[HDR_SAT_TYPE_COUNT][DISP_HDR_REG_SAT_LUT_LEN] = { // HdrSaturationType_e - 1, 1x13
    {0x0 /* 0.000000, UQ1.6 */, 0x16 /* 0.343750, UQ1.6 */, 0x1a /* 0.406250, UQ1.6 */, 0x1f /* 0.484375, UQ1.6 */, 0x25 /* 0.578125, UQ1.6 */,
      0x2d /* 0.703125, UQ1.6 */, 0x35 /* 0.828125, UQ1.6 */, 0x3f /* 0.984375, UQ1.6 */, 0x4b /* 1.171875, UQ1.6 */,
      0x59 /* 1.390625, UQ3.6 */, 0x6a /* 1.656250, UQ3.6 */, 0x7e /* 1.968750, UQ3.6 */, 0x96 /* 2.343750, UQ3.6 */},
};

static unsigned int satLutS[HDR_SAT_TYPE_COUNT][DISP_HDR_REG_SAT_LUT_LEN - 1] = { // HdrSaturationType_e - 1, 1x12
    {0xbdf /* 23.7421875, UQ5.7 */, 0x23f /* 4.4921875, UQ5.7 */, 0x156 /* 2.6718750, UQ5.7 */, 0xcb /* 1.5859375, UQ5.7 */,
      0x79 /* 0.9453125, UQ5.7 */, 0x48 /* 0.5625000, UQ5.7 */, 0x2b /* 0.3359375, UQ5.7 */, 0x32d /* 0.198486328125, UQ0.12 */,
      0x1e4 /* 0.118164062500, UQ0.12 */, 0x120 /* 0.070312500000, UQ0.12 */, 0xab /* 0.041748046875, UQ0.12 */, 0x66 /* 0.024902343750, UQ0.12 */},
};

/* HDR Config Parameter: Color Space Conversion, RGB -> RGB */
static short cscMatrixRgb2Rgb[HdrRGB2RGBTypeInvalid][DISP_HDR_REG_RGB2RGB_LEN] = { // HdrRGB2RGBType_e, 31x9
    // bypass
    {0x1000, 0x0, 0x0, 0x0, 0x1000, 0x0, 0x0, 0x0, 0x1000},

    // sRGB -> AdobeRGB
    {0xb71 /* 0.715087890625 */, 0x48f /* 0.284912109375 */, 0x0 /* 0.000000000000 */,
      0x0 /* 0.000000000000 */, 0x1000 /* 1.000000000000 */, 0x0 /* 0.000000000000 */,
      0x0 /* 0.000000000000 */, 0xa9 /* 0.041259765625 */, 0xf57 /* 0.958740234375 */},
    // sRGB -> BT2020
    {0xa0a /* 0.627441406250 */, 0x545 /* 0.329345703125 */, 0xb1 /* 0.043212890625 */,
      0x11b /* 0.069091796875 */, 0xeb6 /* 0.919433593750 */, 0x2f /* 0.011474609375 */,
      0x43 /* 0.016357421875 */, 0x169 /* 0.088134765625 */, 0xe54 /* 0.895507812500 */},
    // sRGB -> DCIP3
    {0xd29 /* 0.822509765625 */, 0x2d7 /* 0.177490234375 */, 0x0 /* 0.000000000000 */,
      0x88 /* 0.033203125000 */, 0xf78 /* 0.966796875000 */, 0x0 /* 0.000000000000 */,
      0x46 /* 0.017089843750 */, 0x129 /* 0.072509765625 */, 0xe91 /* 0.910400390625 */},
    // sRGB -> BT601-525
    {0x110c /* 1.065429687500 */, 0xffffff1d /* -0.055419921875 */, 0xffffffd7 /* -0.010009765625 */,
      0xffffffb0 /* -0.019531250000 */, 0x1095 /* 1.036376953125 */, 0xffffffbb /* -0.016845703125 */,
      0x7 /* 0.001708984375 */, 0x12 /* 0.004394531250 */, 0xfe7 /* 0.993896484375 */},
    // sRGB -> BT601-625
    {0xf53 /* 0.957763671875 */, 0xad /* 0.042236328125 */, 0x0 /* 0.000000000000 */,
      0x0 /* 0.000000000000 */, 0x1000 /* 1.000000000000 */, 0x0 /* 0.000000000000 */,
      0x0 /* 0.000000000000 */, 0xffffffcf /* -0.011962890625 */, 0x1031 /* 1.011962890625 */},

    // AdobeRGB -> sRGB
    {0x1660 /* 1.398437500000 */, 0xfffff9a0 /* -0.398437500000 */, 0x0 /* 0.000000000000 */,
      0x0 /* -0.000000000000 */, 0x1000 /* 1.000000000000 */, 0x0 /* 0.000000000000 */,
      0x0 /* -0.000000000000 */, 0xffffff50 /* -0.042968750000 */, 0x10b0 /* 1.042968750000 */},
    // AdobeRGB -> BT2020
    {0xe0a /* 0.877441406250 */, 0x13d /* 0.077392578125 */, 0xb9 /* 0.045166015625 */,
      0x18c /* 0.096679687500 */, 0xe44 /* 0.891601562500 */, 0x31 /* 0.011962890625 */,
      0x5e /* 0.022949218750 */, 0xb0 /* 0.042968750000 */, 0xef2 /* 0.934082031250 */},
    // AdobeRGB -> DCIP3
    {0x1267 /* 1.150146484375 */, 0xfffffd99 /* -0.150146484375 */, 0x0 /* 0.000000000000 */,
      0xbe /* 0.046386718750 */, 0xf42 /* 0.953613281250 */, 0x0 /* 0.000000000000 */,
      0x62 /* 0.023925781250 */, 0x6d /* 0.026611328125 */, 0xf32 /* 0.949707031250 */},
    // AdobeRGB -> BT601-525
    {0x17d6 /* 1.489746093750 */, 0xfffff854 /* -0.479492187500 */, 0xffffffd5 /* -0.010498046875 */,
      0xffffff90 /* -0.027343750000 */, 0x10b8 /* 1.044921875000 */, 0xffffffb9 /* -0.017333984375 */,
      0x9 /* 0.002197265625 */, 0xffffff61 /* -0.038818359375 */, 0x1096 /* 1.036621093750 */},
    // AdobeRGB -> BT601-625
    {0x156e /* 1.339355468750 */, 0xfffffa92 /* -0.339355468750 */, 0x0 /* 0.000000000000 */,
      0x0 /* 0.000000000000 */, 0x1000 /* 1.000000000000 */, 0x0 /* 0.000000000000 */,
      0x0 /* 0.000000000000 */, 0xffffff1d /* -0.055419921875 */, 0x10e3 /* 1.055419921875 */},

    // BT2020 -> sRGB
    {0x1a91 /* 1.660400390625 */, 0xfffff699 /* -0.587646484375 */, 0xfffffed6 /* -0.072753906250 */,
      0xfffffe02 /* -0.124511718750 */, 0x1220 /* 1.132812500000 */, 0xffffffde /* -0.008300781250 */,
      0xffffffb6 /* -0.018066406250 */, 0xfffffe64 /* -0.100585937500 */, 0x11e6 /* 1.118652343750 */},
    // BT2020 -> AdobeRGB
    {0x126f /* 1.152099609375 */, 0xfffffe71 /* -0.097412109375 */, 0xffffff21 /* -0.054443359375 */,
      0xfffffe02 /* -0.124511718750 */, 0x1220 /* 1.132812500000 */, 0xffffffde /* -0.008300781250 */,
      0xffffffa4 /* -0.022460937500 */, 0xffffff34 /* -0.049804687500 */, 0x1128 /* 1.072265625000 */},
    // BT2020 -> DCIP3
    {0x157f /* 1.343505859375 */, 0xfffffb7c /* -0.282226562500 */, 0xffffff05 /* -0.061279296875 */,
      0xfffffef5 /* -0.065185546875 */, 0x1136 /* 1.075683593750 */, 0xffffffd5 /* -0.010498046875 */,
      0xc /* 0.002929687500 */, 0xffffffb0 /* -0.019531250000 */, 0x1045 /* 1.016845703125 */},
    // BT2020 -> BT601-525
    {0x1c6b /* 1.776123046875 */, 0xfffff4ff /* -0.687744140625 */, 0xfffffe96 /* -0.088378906250 */,
      0xfffffd6b /* -0.161376953125 */, 0x12ff /* 1.187255859375 */, 0xffffff96 /* -0.025878906250 */,
      0xffffffbf /* -0.015869140625 */, 0xfffffe77 /* -0.095947265625 */, 0x11ca /* 1.111816406250 */},
    // BT2020 -> BT601-625
    {0x195d /* 1.585205078125 */, 0xfffff7c2 /* -0.515136718750 */, 0xfffffee1 /* -0.070068359375 */,
      0xfffffe02 /* -0.124511718750 */, 0x1220 /* 1.132812500000 */, 0xffffffde /* -0.008300781250 */,
      0xffffffbb /* -0.016845703125 */, 0xfffffe28 /* -0.115234375000 */, 0x121d /* 1.132080078125 */},

    // DCIP3 -> sRGB
    {0x1399 /* 1.224853515625 */, 0xfffffc67 /* -0.224853515625 */, 0x0 /* -0.000000000000 */,
      0xffffff54 /* -0.041992187500 */, 0x10ac /* 1.041992187500 */, 0x0 /* 0.000000000000 */,
      0xffffffb0 /* -0.019531250000 */, 0xfffffebe /* -0.078613281250 */, 0x1193 /* 1.098388671875 */},
    // DCIP3 -> AdobeRGB
    {0xdd3 /* 0.864013671875 */, 0x22d /* 0.135986328125 */, 0x0 /* 0.000000000000 */,
      0xffffff54 /* -0.041992187500 */, 0x10ac /* 1.041992187500 */, 0x0 /* -0.000000000000 */,
      0xffffffac /* -0.020507812500 */, 0xffffff7b /* -0.032470703125 */, 0x10d9 /* 1.052978515625 */},
    // DCIP3 -> BT2020
    {0xc10 /* 0.753906250000 */, 0x32d /* 0.198486328125 */, 0xc3 /* 0.047607421875 */,
      0xbb /* 0.045654296875 */, 0xf12 /* 0.941894531250 */, 0x33 /* 0.012451171875 */,
      0xfffffffb /* -0.001220703125 */, 0x48 /* 0.017578125000 */, 0xfbd /* 0.983642578125 */},
    // DCIP3 -> BT601-525
    {0x14ec /* 1.307617187500 */, 0xfffffb41 /* -0.296630859375 */, 0xffffffd3 /* -0.010986328125 */,
      0xfffffeec /* -0.067382812500 */, 0x115f /* 1.085693359375 */, 0xffffffb5 /* -0.018310546875 */,
      0xffffffb7 /* -0.017822265625 */, 0xfffffed1 /* -0.073974609375 */, 0x1177 /* 1.091552734375 */},
    // DCIP3 -> BT601-625
    {0x12be /* 1.171386718750 */, 0xfffffd42 /* -0.171386718750 */, 0x0 /* 0.000000000000 */,
      0xffffff54 /* -0.041992187500 */, 0x10ac /* 1.041992187500 */, 0x0 /* 0.000000000000 */,
      0xffffffb1 /* -0.019287109375 */, 0xfffffe87 /* -0.092041015625 */, 0x11c8 /* 1.111328125000 */},

    // BT601-525 -> sRGB
    {0xf08 /* 0.939453125000 */, 0xce /* 0.050292968750 */, 0x2a /* 0.010253906250 */,
      0x49 /* 0.017822265625 */, 0xf74 /* 0.965820312500 */, 0x43 /* 0.016357421875 */,
      0xfffffff9 /* -0.001708984375 */, 0xffffffee /* -0.004394531250 */, 0x1019 /* 1.006103515625 */},
    // BT601-525 -> AdobeRGB
    {0xad5 /* 0.677001953125 */, 0x4fa /* 0.311035156250 */, 0x31 /* 0.011962890625 */,
      0x49 /* 0.017822265625 */, 0xf74 /* 0.965820312500 */, 0x43 /* 0.016357421875 */,
      0xfffffffd /* -0.000732421875 */, 0x92 /* 0.035644531250 */, 0xf72 /* 0.965332031250 */},
    // BT601-525 -> BT2020
    {0x986 /* 0.595214843750 */, 0x597 /* 0.349365234375 */, 0xe3 /* 0.055419921875 */,
      0x14d /* 0.081298828125 */, 0xe44 /* 0.891601562500 */, 0x70 /* 0.027343750000 */,
      0x40 /* 0.015625000000 */, 0x150 /* 0.082031250000 */, 0xe71 /* 0.902587890625 */},
    // BT601-525 -> DCIP3
    {0xc6a /* 0.775878906250 */, 0x367 /* 0.212646484375 */, 0x2f /* 0.011474609375 */,
      0xc6 /* 0.048339843750 */, 0xef7 /* 0.935302734375 */, 0x42 /* 0.016113281250 */,
      0x41 /* 0.015869140625 */, 0x112 /* 0.066894531250 */, 0xead /* 0.917236328125 */},
    // BT601-525 -> BT601-625
    {0xe69 /* 0.900634765625 */, 0x16c /* 0.088867187500 */, 0x2b /* 0.010498046875 */,
      0x49 /* 0.017822265625 */, 0xf74 /* 0.965820312500 */, 0x43 /* 0.016357421875 */,
      0xfffffff8 /* -0.001953125000 */, 0xffffffbf /* -0.015869140625 */, 0x1049 /* 1.017822265625 */},

    // BT601-625 -> sRGB
    {0x10b4 /* 1.043945312500 */, 0xffffff4c /* -0.043945312500 */, 0x0 /* 0.000000000000 */,
      0x0 /* -0.000000000000 */, 0x1000 /* 1.000000000000 */, 0x0 /* -0.000000000000 */,
      0x0 /* -0.000000000000 */, 0x30 /* 0.011718750000 */, 0xfd0 /* 0.988281250000 */},
    // BT601-625 -> AdobeRGB
    {0xbf2 /* 0.746582031250 */, 0x40e /* 0.253417968750 */, 0x0 /* 0.000000000000 */,
      0x0 /* -0.000000000000 */, 0x1000 /* 1.000000000000 */, 0x0 /* -0.000000000000 */,
      0x0 /* 0.000000000000 */, 0xd7 /* 0.052490234375 */, 0xf29 /* 0.947509765625 */},
    // BT601-625 -> BT2020
    {0xa7b /* 0.655029296875 */, 0x4d6 /* 0.302246093750 */, 0xaf /* 0.042724609375 */,
      0x127 /* 0.072021484375 */, 0xeab /* 0.916748046875 */, 0x2e /* 0.011230468750 */,
      0x46 /* 0.017089843750 */, 0x191 /* 0.097900390625 */, 0xe29 /* 0.885009765625 */},
    // BT601-625 -> DCIP3
    {0xdbd /* 0.858642578125 */, 0x243 /* 0.141357421875 */, 0x0 /* 0.000000000000 */,
      0x8e /* 0.034667968750 */, 0xf72 /* 0.965332031250 */, 0x0 /* -0.000000000000 */,
      0x49 /* 0.017822265625 */, 0x151 /* 0.082275390625 */, 0xe66 /* 0.899902343750 */},
    // BT601-625 -> BT601-525
    {0x11cc /* 1.112304687500 */, 0xfffffe5c /* -0.102539062500 */, 0xffffffd8 /* -0.009765625000 */,
      0xffffffac /* -0.020507812500 */, 0x1098 /* 1.037109375000 */, 0xffffffbc /* -0.016601562500 */,
      0x7 /* 0.001708984375 */, 0x42 /* 0.016113281250 */, 0xfb7 /* 0.982177734375 */}
};

/* HDR Config Parameter: gamma LUT */
static unsigned int gammaLutY[hdr_tf_type_end][DISP_HDR_REG_G_LUT_LEN] = { // hdr_tf_type_e, 7x33
    // sRGB
    {0x0 /* 0.0000, UQ6.4 */, 0x0 /* 0.0000, UQ6.4 */, 0x0 /* 0.0000, UQ6.4 */, 0x0 /* 0.0000, UQ6.4 */, 0x0 /* 0.0000, UQ6.4 */,
      0x0 /* 0.0000, UQ6.4 */, 0x0 /* 0.0000, UQ6.4 */, 0x0 /* 0.0000, UQ6.4 */, 0x0 /* 0.0000, UQ6.4 */, 0x0 /* 0.0000, UQ6.4 */,
      0x1 /* 0.0625, UQ8.4 */, 0x1 /* 0.0625, UQ8.4 */, 0x2 /* 0.1250, UQ8.4 */, 0x4 /* 0.2500, UQ8.4 */, 0x8 /* 0.5000, UQ8.4 */,
      0xe /* 0.8750, UQ8.4 */, 0x18 /* 1.5000, UQ8.4 */, 0x2c /* 2.7500, UQ8.4 */, 0x4f /* 4.9375, UQ8.4 */, 0x8d /* 8.8125, UQ10.4 */,
      0xfe /* 15.8750, UQ10.4 */, 0x1c8 /* 28.5000, UQ10.4 */, 0x326 /* 50.3750, UQ10.4 */, 0x4fc /* 79.7500, UQ10.4 */, 0x755 /* 117.3125, UQ10.4 */,
      0xa53 /* 165.1875, UQ10.4 */, 0xe25 /* 226.3125, UQ10.4 */, 0x1304 /* 304.2500, UQ10.4 */, 0x193c /* 403.7500, UQ10.4 */,
      0x212a /* 530.6250, UQ10.4 */, 0x2b49 /* 692.5625, UQ10.4 */, 0x3833 /* 899.1875, UQ10.4 */, 0x3ff0 /* 1023.0000, UQ10.4 */},
    // AdobeRGB
    {0x4 /* 0.2500, UQ6.4 */, 0x5 /* 0.3125, UQ6.4 */, 0x6 /* 0.3750, UQ6.4 */, 0x8 /* 0.5000, UQ6.4 */, 0xb /* 0.6875, UQ6.4 */,
      0xe /* 0.8750, UQ6.4 */, 0x13 /* 1.1875, UQ6.4 */, 0x18 /* 1.5000, UQ6.4 */, 0x20 /* 2.0000, UQ6.4 */, 0x29 /* 2.5625, UQ6.4 */,
      0x36 /* 3.3750, UQ8.4 */, 0x46 /* 4.3750, UQ8.4 */, 0x5c /* 5.7500, UQ8.4 */, 0x78 /* 7.5000, UQ8.4 */, 0x9c /* 9.7500, UQ8.4 */,
      0xcb /* 12.6875, UQ8.4 */, 0x109 /* 16.5625, UQ8.4 */, 0x15a /* 21.6250, UQ8.4 */, 0x1c4 /* 28.2500, UQ8.4 */, 0x24d /* 36.8125, UQ10.4 */,
      0x301 /* 48.0625, UQ10.4 */, 0x3eb /* 62.6875, UQ10.4 */, 0x51d /* 81.8125, UQ10.4 */, 0x6ac /* 106.7500, UQ10.4 */, 0x8b4 /* 139.2500, UQ10.4 */,
      0xb5a /* 181.6250, UQ10.4 */, 0xed0 /* 237.0000, UQ10.4 */, 0x1353 /* 309.1875, UQ10.4 */, 0x1936 /* 403.3750, UQ10.4 */,
      0x20e4 /* 526.2500, UQ10.4 */, 0x2ae8 /* 686.5000, UQ10.4 */, 0x37fa /* 895.6250, UQ10.4 */, 0x3ff0 /* 1023.0000, UQ10.4 */},
    // DCI-P3
    {0xe /* 0.8750, UQ6.4 */, 0x11 /* 1.0625, UQ6.4 */, 0x15 /* 1.3125, UQ6.4 */, 0x1b /* 1.6875, UQ6.4 */, 0x22 /* 2.1250, UQ6.4 */,
      0x2a /* 2.6250, UQ6.4 */, 0x35 /* 3.3125, UQ6.4 */, 0x42 /* 4.1250, UQ6.4 */, 0x53 /* 5.1875, UQ6.4 */, 0x68 /* 6.5000, UQ6.4 */,
      0x82 /* 8.1250, UQ8.4 */, 0xa3 /* 10.1875, UQ8.4 */, 0xcc /* 12.7500, UQ8.4 */, 0xff /* 15.9375, UQ8.4 */, 0x140 /* 20.0000, UQ8.4 */,
      0x190 /* 25.0000, UQ8.4 */, 0x1f5 /* 31.3125, UQ8.4 */, 0x274 /* 39.2500, UQ8.4 */, 0x312 /* 49.1250, UQ8.4 */, 0x3d8 /* 61.5000, UQ10.4 */,
      0x4d0 /* 77.0000, UQ10.4 */, 0x607 /* 96.4375, UQ10.4 */, 0x78c /* 120.7500, UQ10.4 */, 0x973 /* 151.1875, UQ10.4 */, 0xbd6 /* 189.3750, UQ10.4 */,
      0xed2 /* 237.1250, UQ10.4 */, 0x128f /* 296.9375, UQ10.4 */, 0x173d /* 371.8125, UQ10.4 */, 0x1d19 /* 465.5625, UQ10.4 */,
      0x2470 /* 583.0000, UQ10.4 */, 0x2da1 /* 730.0625, UQ10.4 */, 0x3923 /* 914.1875, UQ10.4 */, 0x3ff0 /* 1023.0000, UQ10.4 */},
    // ITU-OETF
    {0x0 /* 0.0000, UQ6.4 */, 0x0 /* 0.0000, UQ6.4 */, 0x0 /* 0.0000, UQ6.4 */, 0x0 /* 0.0000, UQ6.4 */, 0x0 /* 0.0000, UQ6.4 */,
      0x0 /* 0.0000, UQ6.4 */, 0x0 /* 0.0000, UQ6.4 */, 0x0 /* 0.0000, UQ6.4 */, 0x0 /* 0.0000, UQ6.4 */, 0x0 /* 0.0000, UQ6.4 */,
      0x0 /* 0.0000, UQ8.4 */, 0x0 /* 0.0000, UQ8.4 */, 0x1 /* 0.0625, UQ8.4 */, 0x1 /* 0.0625, UQ8.4 */, 0x3 /* 0.1875, UQ8.4 */,
      0x5 /* 0.3125, UQ8.4 */, 0x9 /* 0.5625, UQ8.4 */, 0xf /* 0.9375, UQ8.4 */, 0x1b /* 1.6875, UQ8.4 */, 0x31 /* 3.0625, UQ10.4 */,
      0x58 /* 5.5000, UQ10.4 */, 0x9f /* 9.9375, UQ10.4 */, 0x11d /* 17.8125, UQ10.4 */, 0x1ff /* 31.9375, UQ10.4 */, 0x395 /* 57.3125, UQ10.4 */,
      0x65f /* 101.9375, UQ10.4 */, 0xa32 /* 163.1250, UQ10.4 */, 0xf2c /* 242.7500, UQ10.4 */, 0x15a5 /* 346.3125, UQ10.4 */,
      0x1e11 /* 481.0625, UQ10.4 */, 0x2905 /* 656.3125, UQ10.4 */, 0x3746 /* 884.3750, UQ10.4 */, 0x3ff0 /* 1023.0000, UQ10.4 */},
    // BT1886
    {0x8 /* 0.5000, UQ6.4 */, 0xa /* 0.6250, UQ6.4 */, 0xc /* 0.7500, UQ6.4 */, 0x10 /* 1.0000, UQ6.4 */, 0x14 /* 1.2500, UQ6.4 */,
      0x1a /* 1.6250, UQ6.4 */, 0x21 /* 2.0625, UQ6.4 */, 0x2a /* 2.6250, UQ6.4 */, 0x35 /* 3.3125, UQ6.4 */, 0x44 /* 4.2500, UQ6.4 */,
      0x57 /* 5.4375, UQ8.4 */, 0x6f /* 6.9375, UQ8.4 */, 0x8d /* 8.8125, UQ8.4 */, 0xb4 /* 11.2500, UQ8.4 */, 0xe6 /* 14.3750, UQ8.4 */,
      0x126 /* 18.3750, UQ8.4 */, 0x177 /* 23.4375, UQ8.4 */, 0x1de /* 29.8750, UQ8.4 */, 0x262 /* 38.1250, UQ8.4 */, 0x30a /* 48.6250, UQ10.4 */,
      0x3e1 /* 62.0625, UQ10.4 */, 0x4f3 /* 79.1875, UQ10.4 */, 0x651 /* 101.0625, UQ10.4 */, 0x80f /* 128.9375, UQ10.4 */, 0xa48 /* 164.5000, UQ10.4 */,
      0xd1f /* 209.9375, UQ10.4 */, 0x10bd /* 267.8125, UQ10.4 */, 0x155c /* 341.7500, UQ10.4 */, 0x1b40 /* 436.0000, UQ10.4 */,
      0x22c5 /* 556.3125, UQ10.4 */, 0x2c5d /* 709.8125, UQ10.4 */, 0x389b /* 905.6875, UQ10.4 */, 0x3ff0 /* 1023.0000, UQ10.4 */},
    // PQ
    {0x1b /* 1.6875, UQ6.4 */, 0x26 /* 2.3750, UQ6.4 */, 0x36 /* 3.3750, UQ6.4 */, 0x4c /* 4.7500, UQ6.4 */, 0x69 /* 6.5625, UQ6.4 */,
      0x92 /* 9.1250, UQ6.4 */, 0xc8 /* 12.5000, UQ6.4 */, 0x10f /* 16.9375, UQ6.4 */, 0x16d /* 22.8125, UQ6.4 */, 0x1e6 /* 30.3750, UQ6.4 */,
      0x27f /* 39.9375, UQ8.4 */, 0x33f /* 51.9375, UQ8.4 */, 0x42c /* 66.7500, UQ8.4 */, 0x54d /* 84.8125, UQ8.4 */, 0x6a5 /* 106.3125, UQ8.4 */,
      0x83b /* 131.6875, UQ8.4 */, 0xa12 /* 161.1250, UQ8.4 */, 0xc2e /* 194.8750, UQ8.4 */, 0xe8e /* 232.8750, UQ8.4 */, 0x1132 /* 275.1250, UQ10.4 */,
      0x1418 /* 321.5000, UQ10.4 */, 0x173d /* 371.8125, UQ10.4 */, 0x1a9b /* 425.6875, UQ10.4 */, 0x1e2c /* 482.7500, UQ10.4 */, 0x21e7 /* 542.4375, UQ10.4 */,
      0x25c5 /* 604.3125, UQ10.4 */, 0x29be /* 667.8750, UQ10.4 */, 0x2dc8 /* 732.5000, UQ10.4 */, 0x31db /* 797.6875, UQ10.4 */,
      0x35ee /* 862.8750, UQ10.4 */, 0x39fa /* 927.6250, UQ10.4 */, 0x3df8 /* 991.5000, UQ10.4 */, 0x3ff0 /* 1023.0000, UQ10.4 */},
    // HLG
    {0x3 /* 0.1875, UQ6.4 */, 0x4 /* 0.2500, UQ6.4 */, 0x5 /* 0.3125, UQ6.4 */, 0x7 /* 0.4375, UQ6.4 */, 0x9 /* 0.5625, UQ6.4 */,
      0xc /* 0.7500, UQ6.4 */, 0x10 /* 1.0000, UQ6.4 */, 0x16 /* 1.3750, UQ6.4 */, 0x1d /* 1.8125, UQ6.4 */, 0x27 /* 2.4375, UQ6.4 */,
      0x35 /* 3.3125, UQ8.4 */, 0x47 /* 4.4375, UQ8.4 */, 0x5f /* 5.9375, UQ8.4 */, 0x7f /* 7.9375, UQ8.4 */, 0xaa /* 10.6250, UQ8.4 */,
      0xe4 /* 14.2500, UQ8.4 */, 0x131 /* 19.0625, UQ8.4 */, 0x199 /* 25.5625, UQ8.4 */, 0x223 /* 34.1875, UQ8.4 */, 0x2dd /* 45.8125, UQ10.4 */,
      0x3d6 /* 61.3750, UQ10.4 */, 0x524 /* 82.2500, UQ10.4 */, 0x6e3 /* 110.1875, UQ10.4 */, 0x93a /* 147.6250, UQ10.4 */, 0xc5c /* 197.7500, UQ10.4 */,
      0x108e /* 264.8750, UQ10.4 */, 0x162d /* 354.8125, UQ10.4 */, 0x1db5 /* 475.3125, UQ10.4 */, 0x267d /* 615.8125, UQ10.4 */,
      0x2e43 /* 740.1875, UQ10.4 */, 0x3583 /* 856.1875, UQ10.4 */, 0x3c80 /* 968.0000, UQ10.4 */, 0x3ff0 /* 1023.0000, UQ10.4 */}
};

static unsigned int gammaLutS[hdr_tf_type_end][DISP_HDR_REG_G_LUT_LEN - 1] = { // hdr_tf_type_e, 7x32
    // sRGB
    {0x6742 /* 13217.0, UQ27.1 */, 0x6742 /* 13217.0, UQ27.1 */, 0x6742 /* 13217.0, UQ27.1 */, 0x6742 /* 13217.0, UQ27.1 */,
      0x6742 /* 13217.0, UQ25.1 */, 0x6742 /* 13217.0, UQ25.1 */, 0x6742 /* 13217.0, UQ25.1 */, 0x6742 /* 13217.0, UQ25.1 */,
      0x6742 /* 13217.0, UQ25.1 */, 0x6742 /* 13217.0, UQ25.1 */, 0x6742 /* 13217.0, UQ25.1 */, 0x6742 /* 13217.0, UQ25.1 */,
      0x6742 /* 13217.0, UQ25.1 */, 0x6742 /* 13217.0, UQ25.1 */, 0x6742 /* 13217.0, UQ25.1 */, 0x6742 /* 13217.0, UQ25.1 */, 0x6742 /* 13217.0, UQ25.1 */,
      0x6742 /* 13217.0, UQ18.1 */, 0x6742 /* 13217.0, UQ18.1 */, 0x6742 /* 13217.0, UQ18.1 */, 0x6742 /* 13217.0, UQ18.1 */, 0x63d9 /* 12780.5, UQ18.1 */,
      0x4acf /* 9575.5, UQ18.1 */, 0x3530 /* 6808.0, UQ14.1 */, 0x25d0 /* 4840.0, UQ14.1 */, 0x1ae2 /* 3441.0, UQ14.1 */, 0x131d /* 2446.5, UQ14.1 */,
      0xd97 /* 1739.5, UQ14.1 */, 0x9a9 /* 1236.5, UQ14.1 */, 0x6df /* 879.5, UQ14.1 */, 0x4e2 /* 625.0, UQ14.1 */, 0x3d1 /* 488.5, UQ14.1 */},
    // AdobeRGB
    {0x113a7c2 /* 9032673.0, UQ27.1 */, 0xc86437 /* 6566427.5, UQ27.1 */, 0x91ad68 /* 4773556.0, UQ27.1 */, 0x69e6f6 /* 3470203.0, UQ27.1 */,
      0x4cfcb1 /* 2522712.5, UQ25.1 */, 0x37f782 /* 1833921.0, UQ25.1 */, 0x28af95 /* 1333194.5, UQ25.1 */, 0x1d93c1 /* 969184.5, UQ25.1 */,
      0x158064 /* 704562.0, UQ25.1 */, 0xfa17e /* 512191.0, UQ25.1 */, 0xb5cf1 /* 372344.5, UQ25.1 */, 0x842b2 /* 270681.0, UQ25.1 */,
      0x6014f /* 196775.5, UQ25.1 */, 0x45d91 /* 143048.5, UQ25.1 */, 0x32c6e /* 103991.0, UQ25.1 */, 0x24e9c /* 75598.0, UQ25.1 */, 0x1ad5a /* 54957.0, UQ25.1 */,
      0x1381f /* 39951.5, UQ18.1 */, 0xe2e7 /* 29043.5, UQ18.1 */, 0xa4f3 /* 21113.5, UQ18.1 */, 0x77ea /* 15349.0, UQ18.1 */, 0x572c /* 11158.0, UQ18.1 */,
      0x3f5f /* 8111.5, UQ18.1 */, 0x2e11 /* 5896.5, UQ14.1 */, 0x217d /* 4286.5, UQ14.1 */, 0x1859 /* 3116.5, UQ14.1 */, 0x11b3 /* 2265.5, UQ14.1 */,
      0xcde /* 1647.0, UQ14.1 */, 0x95a /* 1197.0, UQ14.1 */, 0x6cd /* 870.5, UQ14.1 */, 0x4f1 /* 632.5, UQ14.1 */, 0x3ed /* 502.5, UQ14.1 */},
    // DCI-P3
    {0x33e1931 /* 27200664.5, UQ27.1 */, 0x2433798 /* 18979788.0, UQ27.1 */, 0x19428f2 /* 13243513.0, UQ27.1 */, 0x11a02a9 /* 9240916.5, UQ27.1 */,
      0xc4c737 /* 6448027.5, UQ25.1 */, 0x894e47 /* 4499235.5, UQ25.1 */, 0x5fcec9 /* 3139428.5, UQ25.1 */, 0x42da09 /* 2190596.5, UQ25.1 */,
      0x2ea5a6 /* 1528531.0, UQ25.1 */, 0x208c84 /* 1066562.0, UQ25.1 */, 0x16b62c /* 744214.0, UQ25.1 */, 0xfd8f4 /* 519290.0, UQ25.1 */,
      0xb0ed1 /* 362344.5, UQ25.1 */, 0x7b742 /* 252833.0, UQ25.1 */, 0x56246 /* 176419.0, UQ25.1 */, 0x3c1b7 /* 123099.5, UQ25.1 */, 0x29f0e /* 85895.0, UQ25.1 */,
      0x1d43e /* 59935.0, UQ18.1 */, 0x146ba /* 41821.0, UQ18.1 */, 0xe3fb /* 29181.5, UQ18.1 */, 0x9f14 /* 20362.0, UQ18.1 */, 0x6f00 /* 14208.0, UQ18.1 */,
      0x4d74 /* 9914.0, UQ18.1 */, 0x360b /* 6917.5, UQ14.1 */, 0x25b6 /* 4827.0, UQ14.1 */, 0x1a50 /* 3368.0, UQ14.1 */, 0x125c /* 2350.0, UQ14.1 */,
      0xcd0 /* 1640.0, UQ14.1 */, 0x8f0 /* 1144.0, UQ14.1 */, 0x63d /* 798.5, UQ14.1 */, 0x45a /* 557.0, UQ14.1 */, 0x35a /* 429.0, UQ14.1 */},
    // ITU-OETF
    {0x23f7 /* 4603.5, UQ27.1 */, 0x23f7 /* 4603.5, UQ27.1 */, 0x23f7 /* 4603.5, UQ27.1 */, 0x23f7 /* 4603.5, UQ27.1 */,
      0x23f7 /* 4603.5, UQ25.1 */, 0x23f7 /* 4603.5, UQ25.1 */, 0x23f7 /* 4603.5, UQ25.1 */, 0x23f7 /* 4603.5, UQ25.1 */,
      0x23f7 /* 4603.5, UQ25.1 */, 0x23f7 /* 4603.5, UQ25.1 */, 0x23f7 /* 4603.5, UQ25.1 */, 0x23f7 /* 4603.5, UQ25.1 */,
      0x23f7 /* 4603.5, UQ25.1 */, 0x23f7 /* 4603.5, UQ25.1 */, 0x23f7 /* 4603.5, UQ25.1 */, 0x23f7 /* 4603.5, UQ25.1 */, 0x23f7 /* 4603.5, UQ25.1 */,
      0x23f7 /* 4603.5, UQ18.1 */, 0x23f7 /* 4603.5, UQ18.1 */, 0x23f7 /* 4603.5, UQ18.1 */, 0x23f7 /* 4603.5, UQ18.1 */, 0x23f7 /* 4603.5, UQ18.1 */,
      0x23f7 /* 4603.5, UQ18.1 */, 0x23f7 /* 4603.5, UQ14.1 */, 0x2340 /* 4512.0, UQ14.1 */, 0x1aeb /* 3445.5, UQ14.1 */, 0x1384 /* 2498.0, UQ14.1 */,
      0xe26 /* 1811.0, UQ14.1 */, 0xa42 /* 1313.0, UQ14.1 */, 0x770 /* 952.0, UQ14.1 */, 0x564 /* 690.0, UQ14.1 */, 0x445 /* 546.5, UQ14.1 */},
    // BT1886
    {0x1f72a2d /* 16487702.5, UQ27.1 */, 0x165bc4b /* 11722277.5, UQ27.1 */, 0xfe56ed /* 8334198.5, UQ27.1 */, 0xb4d3fa /* 5925373.0, UQ27.1 */,
      0x809040 /* 4212768.0, UQ25.1 */, 0x5b67a7 /* 2995155.5, UQ25.1 */, 0x40fc79 /* 2129468.5, UQ25.1 */, 0x2e340d /* 1513990.5, UQ25.1 */,
      0x20d966 /* 1076403.0, UQ25.1 */, 0x175ad7 /* 765291.5, UQ25.1 */, 0x109ac8 /* 544100.0, UQ25.1 */, 0xbce2e /* 386839.0, UQ25.1 */,
      0x864af /* 275031.5, UQ25.1 */, 0x5f7a7 /* 195539.5, UQ25.1 */, 0x43e1e /* 139023.0, UQ25.1 */, 0x30432 /* 98841.0, UQ25.1 */, 0x22502 /* 70273.0, UQ25.1 */,
      0x18654 /* 49962.0, UQ18.1 */, 0x11583 /* 35521.5, UQ18.1 */, 0xc54e /* 25255.0, UQ18.1 */, 0x8c47 /* 17955.5, UQ18.1 */, 0x63bc /* 12766.0, UQ18.1 */,
      0x46e8 /* 9076.0, UQ18.1 */, 0x326a /* 6453.0, UQ14.1 */, 0x23d8 /* 4588.0, UQ14.1 */, 0x197c /* 3262.0, UQ14.1 */, 0x121e /* 2319.0, UQ14.1 */,
      0xce2 /* 1649.0, UQ14.1 */, 0x928 /* 1172.0, UQ14.1 */, 0x683 /* 833.5, UQ14.1 */, 0x4a1 /* 592.5, UQ14.1 */, 0x39e /* 463.0, UQ14.1 */},
    // PQ
    {0xa7a7739 /* 87899036.5, UQ27.1 */, 0x8262af0 /* 68359544.0, UQ27.1 */, 0x649b249 /* 52746532.5, UQ27.1 */, 0x4cead65 /* 40326834.5, UQ27.1 */,
      0x3a325f1 /* 30511864.5, UQ25.1 */, 0x2b87234 /* 22821146.0, UQ25.1 */, 0x2026e3f /* 16856863.5, UQ25.1 */, 0x176f373 /* 12286393.5, UQ25.1 */,
      0x10d7bc0 /* 8830432.0, UQ25.1 */, 0xbee1e2 /* 6254833.0, UQ25.1 */, 0x85335f /* 4364719.5, UQ25.1 */, 0x5b8c2c /* 2999830.0, UQ25.1 */,
      0x3df6bf /* 2030431.5, UQ25.1 */, 0x294dbb /* 1353437.5, UQ25.1 */, 0x1b1e22 /* 888593.0, UQ25.1 */, 0x118a48 /* 574756.0, UQ25.1 */, 0xb2e44 /* 366370.0, UQ25.1 */,
      0x706c5 /* 230242.5, UQ18.1 */, 0x45afe /* 142719.0, UQ18.1 */, 0x2aa0e /* 87303.0, UQ18.1 */, 0x19bf7 /* 52731.5, UQ18.1 */, 0xf5d5 /* 31466.5, UQ18.1 */,
      0x9105 /* 18562.5, UQ18.1 */, 0x549f /* 10831.5, UQ14.1 */, 0x30df /* 6255.5, UQ14.1 */, 0x1bf4 /* 3578.0, UQ14.1 */, 0xfd8 /* 2028.0, UQ14.1 */,
      0x8e7 /* 1139.5, UQ14.1 */, 0x4f7 /* 635.5, UQ14.1 */, 0x2bf /* 351.5, UQ14.1 */, 0x183 /* 193.5, UQ14.1 */, 0xf8 /* 124.0, UQ14.1 */},
    // HLG
    {0xe71eff /* 7573375.5, UQ27.1 */, 0xac86bf /* 5653343.5, UQ27.1 */, 0x80c96b /* 4220085.5, UQ27.1 */, 0x6022e1 /* 3150192.5, UQ27.1 */,
      0x47c36f /* 2351543.5, UQ25.1 */, 0x3591d6 /* 1755371.0, UQ25.1 */, 0x27fd0c /* 1310342.0, UQ25.1 */, 0x1dd9b6 /* 978139.0, UQ25.1 */,
      0x16485b /* 730157.5, UQ25.1 */, 0x10a22a /* 545045.0, UQ25.1 */, 0xc6a9e /* 406863.0, UQ25.1 */, 0x944c3 /* 303713.5, UQ25.1 */,
      0x6eb36 /* 226715.0, UQ25.1 */, 0x52a2a /* 169237.0, UQ25.1 */, 0x3daf7 /* 126331.5, UQ25.1 */, 0x2e0bf /* 94303.5, UQ25.1 */, 0x225f7 /* 70395.5, UQ25.1 */,
      0x19a89 /* 52548.5, UQ18.1 */, 0x13274 /* 39226.0, UQ18.1 */, 0xe4c3 /* 29281.5, UQ18.1 */, 0xaac4 /* 21858.0, UQ18.1 */, 0x7f79 /* 16316.5, UQ18.1 */,
      0x5f28 /* 12180.0, UQ18.1 */, 0x4708 /* 9092.0, UQ14.1 */, 0x3506 /* 6787.0, UQ14.1 */, 0x2794 /* 5066.0, UQ14.1 */, 0x1d8c /* 3782.0, UQ14.1 */,
      0x1331 /* 2456.5, UQ14.1 */, 0x977 /* 1211.5, UQ14.1 */, 0x4ec /* 630.0, UQ14.1 */, 0x2a5 /* 338.5, UQ14.1 */, 0x1b2 /* 217.0, UQ14.1 */}
};

/* CIE colorSpace XY position */
static unsigned int colorRgbXy10Bit[hdr_cs_type_end][6] = { // hdr_cs_type_e, rx ry gx gy bx by, 6x6, accuracy: 2^(-10)
    // sRGB [0.640 0.330] [0.300 0.600] [0.150 0.060]
    {0x28F, 0x152, 0x133, 0x266, 0x9A, 0x3D},
    // AdobeRGB [0.6400 0.3300] [0.2100 0.7100] [0.1500 0.0600]
    {0x28F, 0x152, 0xD7,  0x2D7, 0x9A, 0x3D},
    // BT2020 [0.708 0.292] [0.170 0.797] [0.131 0.046]
    {0x2D5, 0x12B, 0xAE,  0x330, 0x86, 0x2F},
    // DCIP3 [0.6800 0.3200] [0.2650 0.6900] [0.1500 0.0600]
    {0x2B8, 0x148, 0x10F, 0x2C3, 0x9A, 0x3D},
    // BT601-525 [0.630 0.340] [0.310 0.595] [0.155 0.070]
    {0x285, 0x15C, 0x13D, 0x261, 0x9F, 0x48},
    // BT601-625 [0.640 0.330] [0.290 0.600] [0.150 0.060]
    {0x28F, 0x152, 0x129, 0x266, 0x9A, 0x3D}
};

static unsigned int colorRgbXy16Bit[hdr_cs_type_end][6] = { // accuracy: 1/50000
    // sRGB
    {32000, 16500, 15000, 30000, 7500, 3000},
    // AdobeRGB
    {32000, 16500, 10500, 35500, 7500, 3000},
    // BT2020
    {35400, 14600,  8500, 39850, 6550, 2300},
    // DCIP3
    {34000, 16000, 13250, 34500, 7500, 3000},
    // BT601-525
    {31500, 17000, 15500, 29750, 7750, 3500},
    // BT601-625
    {32000, 16500, 14500, 30000, 7500, 3000}
};

/* D65 [0.3127, 0.3290], accuracy: 1/50000 */
static unsigned int whitePointD65[2] = {15635, 16450};

/* metadata policy, ref G100 HW design: minLin(1/10000 nits), maxLin, maxFall, minLout(1/10000 nits), maxLout */
static unsigned int defaultMetadata[HdrTfInOutTypeInvalid][5] = {
    {0,  100,   80,  0,   100}, // SDR -> SDR
    {0,  100,   50,  0,   1000}, // SDR -> PQ
    {0,  100,   50,  0,   1000}, // SDR -> HLG
    {0,  10000, 180, 0,   300}, // PQ  -> SDR
    {50, 10000, 640, 100, 400}, // PQ  -> PQ
    {0,  1000,  180, 0,   1000}, // HLG  -> SDR
    {0,  1000,  800, 0,   1000}  // HLG  -> HLG
};

static HdrRGB2RGBType_e GetRgb2rgbType(hdr_cs_type_e csInType, hdr_cs_type_e csOutType)
{
    HdrRGB2RGBType_e rgb2rgbType = HdrRGB2RGBTypeBypass;
    switch(csInType)
    {
        case hdr_cs_type_srgb:
            switch(csOutType)
            {
                case hdr_cs_type_srgb:
                    rgb2rgbType = HdrRGB2RGBTypeSrgbToAdobeRgb;
                    break;
                case hdr_cs_type_bt2020:
                    rgb2rgbType = HdrRGB2RGBTypeSrgbToBt2020;
                    break;
                case hdr_cs_type_dciP3:
                    rgb2rgbType = HdrRGB2RGBTypeSrgbToDciP3;
                    break;
                case hdr_cs_type_bt601_525:
                    rgb2rgbType = HdrRGB2RGBTypeSrgbToBt601_525;
                    break;
                case hdr_cs_type_bt601_625:
                    rgb2rgbType = HdrRGB2RGBTypeSrgbToBt601_625;
                    break;
                default:
                    break;
            }
            break;
        case hdr_cs_type_adobeRgb:
            switch(csOutType)
            {
                case hdr_cs_type_srgb:
                    rgb2rgbType = HdrRGB2RGBTypeAdobeRgbToSrgb;
                    break;
                case hdr_cs_type_bt2020:
                    rgb2rgbType = HdrRGB2RGBTypeAdobeRgbToBt2020;
                    break;
                case hdr_cs_type_dciP3:
                    rgb2rgbType = HdrRGB2RGBTypeAdobeRgbToDciP3;
                    break;
                case hdr_cs_type_bt601_525:
                    rgb2rgbType = HdrRGB2RGBTypeAdobeRgbToBt601_525;
                    break;
                case hdr_cs_type_bt601_625:
                    rgb2rgbType = HdrRGB2RGBTypeAdobeRgbToBt601_625;
                    break;
                default:
                    break;
            }
            break;
        case hdr_cs_type_bt2020:
            switch(csOutType)
            {
                case hdr_cs_type_srgb:
                    rgb2rgbType = HdrRGB2RGBTypeBt2020ToSrgb;
                    break;
                case hdr_cs_type_adobeRgb:
                    rgb2rgbType = HdrRGB2RGBTypeBt2020ToAdobeRgb;
                    break;
                case hdr_cs_type_dciP3:
                    rgb2rgbType = HdrRGB2RGBTypeBt2020ToDciP3;
                    break;
                case hdr_cs_type_bt601_525:
                    rgb2rgbType = HdrRGB2RGBTypeBt2020ToBt601_525;
                    break;
                case hdr_cs_type_bt601_625:
                    rgb2rgbType = HdrRGB2RGBTypeBt2020ToBt601_625;
                    break;
                default:
                    break;
            }
            break;
        case hdr_cs_type_dciP3:
            switch(csOutType)
            {
                case hdr_cs_type_srgb:
                    rgb2rgbType = HdrRGB2RGBTypeDciP3ToSrgb;
                    break;
                case hdr_cs_type_adobeRgb:
                    rgb2rgbType = HdrRGB2RGBTypeDciP3ToAdobeRgb;
                    break;
                case hdr_cs_type_bt2020:
                    rgb2rgbType = HdrRGB2RGBTypeDciP3ToBt2020;
                    break;
                case hdr_cs_type_bt601_525:
                    rgb2rgbType = HdrRGB2RGBTypeDciP3ToBt601_525;
                    break;
                case hdr_cs_type_bt601_625:
                    rgb2rgbType = HdrRGB2RGBTypeDciP3ToBt601_625;
                    break;
                default:
                    break;
            }
            break;
        case hdr_cs_type_bt601_525:
            switch(csOutType)
            {
                case hdr_cs_type_srgb:
                    rgb2rgbType = HdrRGB2RGBTypeBt601_525ToSrgb;
                    break;
                case hdr_cs_type_adobeRgb:
                    rgb2rgbType = HdrRGB2RGBTypeBt601_525ToAdobeRgb;
                    break;
                case hdr_cs_type_bt2020:
                    rgb2rgbType = HdrRGB2RGBTypeBt601_525ToBt2020;
                    break;
                case hdr_cs_type_dciP3:
                    rgb2rgbType = HdrRGB2RGBTypeBt601_525ToDciP3;
                    break;
                case hdr_cs_type_bt601_625:
                    rgb2rgbType = HdrRGB2RGBTypeBt601_525ToBt601_625;
                    break;
                default:
                    break;
            }
            break;
        case hdr_cs_type_bt601_625:
            switch(csOutType)
            {
                case hdr_cs_type_srgb:
                    rgb2rgbType = HdrRGB2RGBTypeBt601_625ToSrgb;
                    break;
                case hdr_cs_type_adobeRgb:
                    rgb2rgbType = HdrRGB2RGBTypeBt601_625ToAdobeRgb;
                    break;
                case hdr_cs_type_bt2020:
                    rgb2rgbType = HdrRGB2RGBTypeBt601_625ToBt2020;
                    break;
                case hdr_cs_type_dciP3:
                    rgb2rgbType = HdrRGB2RGBTypeBt601_625ToDciP3;
                    break;
                case hdr_cs_type_bt601_525:
                    rgb2rgbType = HdrRGB2RGBTypeBt601_625ToBt601_525;
                    break;
                default:
                    break;
            }
            break;
        default:
            break;
    }

    return rgb2rgbType;
}

static HdrTfInOutType_e HdrGetTfInOutType(hdr_tf_type_e tfIn, hdr_tf_type_e tfOut)
{
    HdrTfInOutType_e type = HdrTfInOutTypeSDR2SDR;

    if( (hdr_tf_type_pq == tfIn && hdr_tf_type_hlg == tfOut)
        || (hdr_tf_type_hlg == tfIn && hdr_tf_type_pq == tfOut))
    {
        gf_printk(GF_DRV_WARNING, "%s, warning PQ <--> HLG not support\n", __FUNCTION__);
        return HdrTfInOutTypeInvalid;
    }

    switch(tfIn)
    {
        case hdr_tf_type_pq:
            type = (hdr_tf_type_pq == tfOut) ? HdrTfInOutTypePQ2PQ : HdrTfInOutTypePQ2SDR;
            break;
        case hdr_tf_type_hlg:
            type = (hdr_tf_type_hlg == tfOut) ? HdrTfInOutTypeHLG2HLG : HdrTfInOutTypeHLG2SDR;
            break;
        default:
            if(hdr_tf_type_pq == tfOut)
                type = HdrTfInOutTypeSDR2PQ;
            else if(hdr_tf_type_hlg == tfOut)
                type = HdrTfInOutTypeSDR2HLG;
            else
                type = HdrTfInOutTypeSDR2SDR;
            break;
    }
    return type;
}

static void HdrGetColorPosition(hdr_cs_type_e csType, disp_u16xy_t* prgb, disp_u16xy_t* pwhite)
{
    prgb[0].x = colorRgbXy16Bit[csType][0];
    prgb[0].y = colorRgbXy16Bit[csType][1];
    prgb[1].x = colorRgbXy16Bit[csType][2];
    prgb[1].y = colorRgbXy16Bit[csType][3];
    prgb[2].x = colorRgbXy16Bit[csType][4];
    prgb[2].y = colorRgbXy16Bit[csType][5];
    pwhite->x = whitePointD65[0];
    pwhite->y = whitePointD65[1];
}

void HdrDegamma(HdrMgr_t* pHdrMgr, void* pDispHdr)
{
    pdisp_hdr_module_t pHdr = (pdisp_hdr_module_t)pDispHdr;

    gf_memcpy(pHdr->dg_lut_y, &pHdrMgr->degammaLutY, sizeof(pHdr->dg_lut_y));
    gf_memcpy(pHdr->dg_lut_s, &pHdrMgr->degammaLutS, sizeof(pHdr->dg_lut_s));
}

void HdrRgb2Y(hdr_cs_type_e csInType, void* pDispHdr)
{
    pdisp_hdr_module_t pHdr = (pdisp_hdr_module_t)pDispHdr;

    gf_memcpy(pHdr->sat_rgb_2_y, &satRgb2Y[csInType][0], sizeof(pHdr->sat_rgb_2_y));
}

void HdrSaturation(HdrSaturationType_e satType, void*  pDispHdr)
{
    pdisp_hdr_module_t pHdr = (pdisp_hdr_module_t)pDispHdr;
    int index = 0;

    if(satType)
    {   // satgain valid
        gf_memcpy(pHdr->sat_lut_y, &satLutY[satType - 1][0], sizeof(pHdr->sat_lut_y));
        gf_memcpy(pHdr->sat_lut_s, &satLutS[satType - 1][0], sizeof(pHdr->sat_lut_s));
    }
    else
    {   // satgain = 0 invalid
        for(index = 0; index < DISP_HDR_REG_SAT_LUT_LEN; ++index)
            pHdr->sat_lut_y[index] = 0x40;  // 1.0
        gf_memset(pHdr->sat_lut_s, 0, sizeof(pHdr->sat_lut_s)); // 0.0
    }
}

void HdrDegammaStretch(hdr_tf_type_e tfInType, hdr_tf_type_e tfOutType, unsigned int maxlin, unsigned int maxlout, unsigned int* pDgStretchA, int* pDStretchB)
{
    unsigned long long stretchA_Uq14_12 = 0;

    *pDgStretchA = 0x1000;      // 1 - UQ14.12
    *pDStretchB = 0x0;          // 0 - Q14.12

    if(hdr_tf_type_pq == tfInType && hdr_tf_type_pq == tfOutType) {
        // a = maxlout / maxlin
        if(0 == maxlin) {
            gf_printk(GF_DRV_WARNING, "%s, invalid maxlin[%u], -> 100\n", __FUNCTION__, maxlin);
            maxlin = 100;
        }
        stretchA_Uq14_12 = maxlout;
        stretchA_Uq14_12 <<= 12;
        stretchA_Uq14_12 /= maxlin;
        *pDgStretchA = (unsigned long)stretchA_Uq14_12;
    }
}

void HdrColorSpaceConversion(hdr_cs_type_e csInType, hdr_cs_type_e csOutType, void*  pDispHdr)
{
    HdrRGB2RGBType_e rgb2rgbType;
    pdisp_hdr_module_t pHdr = (pdisp_hdr_module_t)pDispHdr;

    rgb2rgbType = GetRgb2rgbType(csInType, csOutType);
    gf_memcpy(pHdr->rgb_2_rgb_m, &cscMatrixRgb2Rgb[rgb2rgbType][0], sizeof(pHdr->rgb_2_rgb_m));
}

void HdrGamma(hdr_tf_type_e tfOutType, void*  pDispHdr)
{
    pdisp_hdr_module_t pHdr = (pdisp_hdr_module_t)pDispHdr;

    gf_memcpy(pHdr->g_lut_y, &gammaLutY[tfOutType][0], sizeof(pHdr->g_lut_y));
    gf_memcpy(pHdr->g_lut_s, &gammaLutS[tfOutType][0], sizeof(pHdr->g_lut_s));
}

void HdrGammaStretch(unsigned int* pGStretchA, int* pGStretchB)
{
    *pGStretchA = 0x1000;   // 1 - UQ14.12
    *pGStretchB = 0x0;      // 0 - Q14.12
}

void HdrUpdateOutMetadata(HdrMgr_t* pHdrMgr, void* pDispHdr)
{
    pdisp_hdr_module_t pHdr = (pdisp_hdr_module_t)pDispHdr;
    //disp_hdr_metadata_t pmetas
    // nothing todo now.
}

int HdrUpdate(HdrMgr_t* pHdrMgr, void* pDispHdr)
{
    pdisp_hdr_module_t pHdr = (pdisp_hdr_module_t)pDispHdr;

    if(!pHdr)
    {
        gf_printk(GF_DRV_WARNING, "%s, invalid param, pDispHdr NULL\n", __FUNCTION__);
        return -1;
    }

    pHdr->hdr_en = pHdrMgr->hdrEnable ? 1 : 0;
    pHdr->sat_en = pHdrMgr->satType ? 1 : 0;

    if(pHdrMgr->hdrEnable)
    {
        // update static metadata
        HdrUpdateOutMetadata(pHdrMgr, pDispHdr);

        // update HDR register
        HdrDegamma(pHdrMgr, pDispHdr);
        HdrRgb2Y(pHdrMgr->csIn, pDispHdr);
        HdrSaturation(pHdrMgr->satType, pDispHdr);
        HdrDegammaStretch(pHdrMgr->tfIn, pHdrMgr->tfOut, pHdrMgr->maxLin, pHdrMgr->maxLout, &pHdr->dg_stretch_a, &pHdr->dg_stretch_b);
        HdrColorSpaceConversion(pHdrMgr->csIn, pHdrMgr->csOut, pDispHdr);
        HdrGamma(pHdrMgr->tfOut, pDispHdr);
        HdrGammaStretch(&pHdr->g_stretch_a, &pHdr->g_stretch_b);
    }

    return 0;
}

void HdrDump(HdrMgr_t* pHdrMgr, void* pDispHdr)
{
    pdisp_hdr_module_t pHdr = (pdisp_hdr_module_t)pDispHdr;
    int i;

    if (pHdrMgr)
    {
        gf_printk(GF_DRV_DEBUG, "%s: hdr_en[%u], satType[%d] cs[%d -> %d] tf[%d -> %d] maxLin[%u], Out_min_max_fall[%u %u %u]\n",
            __FUNCTION__, pHdrMgr->hdrEnable, pHdrMgr->satType, pHdrMgr->csIn, pHdrMgr->csOut, pHdrMgr->tfIn,
            pHdrMgr->tfOut, pHdrMgr->maxLin, pHdrMgr->minLout_1_10000, pHdrMgr->maxLout, pHdrMgr->maxFall);
    }

    if (pHdr)
    {
        gf_printk(GF_DRV_DEBUG, "hdr reg:\n");
        printk(KERN_DEBUG KERN_CONT "\treg: hdr_en[%#x], sat_en[%#x], dg_stretch[%#x, %#x], g_stretch[%#x, %#x], rgb2Y[%#x, %#x, %#x]\n",
            pHdr->hdr_en, pHdr->sat_en, pHdr->dg_stretch_a, pHdr->dg_stretch_b, pHdr->g_stretch_a,
            pHdr->g_stretch_b, pHdr->sat_rgb_2_y[0], pHdr->sat_rgb_2_y[1], pHdr->sat_rgb_2_y[2]);
        printk(KERN_DEBUG KERN_CONT "\treg dg_lut_y: ");
        for(i = 0; i < 37; ++i)
            printk(KERN_DEBUG KERN_CONT "%#x, ", pHdr->dg_lut_y[i]);
        printk(KERN_DEBUG KERN_CONT "\n\treg dg_lut_s: ");
        for(i = 0; i < 36; ++i)
            printk(KERN_DEBUG KERN_CONT "%#x, ", pHdr->dg_lut_s[i]);
        printk(KERN_DEBUG KERN_CONT "\n\treg sat_lut_y: ");
        for(i = 0; i < 13; ++i)
            printk(KERN_DEBUG KERN_CONT "%#x, ", pHdr->sat_lut_y[i]);
        printk(KERN_DEBUG KERN_CONT "\n\treg sat_lut_s: ");
        for(i = 0; i < 12; ++i)
            printk(KERN_DEBUG KERN_CONT "%#x, ", pHdr->sat_lut_s[i]);
        printk(KERN_DEBUG KERN_CONT "\n\treg rgb_2_rgb_m: ");
        for(i = 0; i < 9; ++i)
            printk(KERN_DEBUG KERN_CONT "%#x, ", pHdr->rgb_2_rgb_m[i]);
        printk(KERN_DEBUG KERN_CONT "\n\treg g_lut_y: ");
        for(i = 0; i < 33; ++i)
            printk(KERN_DEBUG KERN_CONT "%#x, ", pHdr->g_lut_y[i]);
        printk(KERN_DEBUG KERN_CONT "\n\treg g_lut_s: ");
        for(i = 0; i < 32; ++i)
            printk(KERN_DEBUG KERN_CONT "%#x, ", pHdr->g_lut_s[i]);
        printk(KERN_DEBUG KERN_CONT "\n");
    }
}
