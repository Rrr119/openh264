/*!
 * \copy
 *     Copyright (c)  2025, Cisco Systems
 *     All rights reserved.
 *
 *     Redistribution and use in source and binary forms, with or without
 *     modification, are permitted provided that the following conditions
 *     are met:
 *
 *        * Redistributions of source code must retain the above copyright
 *          notice, this list of conditions and the following disclaimer.
 *
 *        * Redistributions in binary form must reproduce the above copyright
 *          notice, this list of conditions and the following disclaimer in
 *          the documentation and/or other materials provided with the
 *          distribution.
 *
 *     THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *     "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *     LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *     FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 *     COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *     INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *     BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *     LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *     CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *     LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *     ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *     POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef WELS_SCREEN_CONTENT_H__
#define WELS_SCREEN_CONTENT_H__

#include "typedefs.h"

namespace WelsEnc {

#define SCREEN_CONTENT_MAX_PALETTE_COLORS 16

enum EScreenContentMbFlag {
  SC_MB_LOW_COLOR   = 1 << 0,
  SC_MB_PALETTE     = 1 << 1,
  SC_MB_STATIC      = 1 << 2,
  SC_MB_COPY_LEFT   = 1 << 3,
  SC_MB_COPY_TOP    = 1 << 4
};

typedef struct TagScreenContentPaletteEntry {
  uint8_t uiY;
  uint8_t uiU;
  uint8_t uiV;
  uint16_t uiCount;
} SScreenContentPaletteEntry;

typedef struct TagScreenContentPalette {
  SScreenContentPaletteEntry sEntry[SCREEN_CONTENT_MAX_PALETTE_COLORS];
  uint8_t uiSize;
  uint8_t uiReserved[3];
} SScreenContentPalette;

typedef struct TagScreenContentMbInfo {
  uint8_t uiFlags;
  uint8_t uiPaletteSize;
  uint8_t uiReserved[2];
} SScreenContentMbInfo;

typedef struct TagScreenContentFrameStats {
  uint32_t uiMbCount;
  uint32_t uiLowColorMbCount;
  uint32_t uiPaletteMbCount;
  uint32_t uiStaticMbCount;
  uint32_t uiCopyMbCount;
  float fScreenContentScore;
  bool bScreenContent;
} SScreenContentFrameStats;

bool WelsScreenContentExtractPalette (const uint8_t* pY, const uint8_t* pU, const uint8_t* pV,
                                      int32_t iStrideY, int32_t iStrideUV,
                                      int32_t iWidth, int32_t iHeight,
                                      SScreenContentPalette* pPalette);

bool WelsScreenContentAnalyzeFrame (uint8_t* pY, uint8_t* pU, uint8_t* pV,
                                    int32_t iStrideY, int32_t iStrideUV,
                                    const uint8_t* pRefY, const uint8_t* pRefU, const uint8_t* pRefV,
                                    int32_t iRefStrideY, int32_t iRefStrideUV, bool bHasReference,
                                    int32_t iWidth, int32_t iHeight,
                                    bool bEnablePalette, bool bEnableBlockCopy, bool bForceScreenContent,
                                    SScreenContentFrameStats* pStats,
                                    SScreenContentMbInfo* pMbInfo, int32_t iMbInfoCount);

}

#endif//WELS_SCREEN_CONTENT_H__
