#ifndef __REGISTER_BASE__HEADER__
#define __REGISTER_BASE__HEADER__

#include <cstdint>

namespace reg
{
    // only CPIA/MHB/MMU 3 component have MMIO, other compnent should be accessed via DMA path
    const static uint32_t CPIA_BASE = 0x10000; // 1_0000-1_1fff
    const static uint32_t CPIA_MASK = 0x01fff;
    const static uint32_t MHB_BASE = 0x18000; // 18000-18fff
    const static uint32_t MHB_MASK = 0x00fff;
    const static uint32_t MMU_BASE = 0x20000; // 2_0000-2_00ff
    const static uint32_t MMU_MASK = 0x000ff;

    const static uint32_t PMU_BASE = 0x220000; // 22_0000-22_ffff
    const static uint32_t PMU_MASK = 0x00ffff;
}

#endif
