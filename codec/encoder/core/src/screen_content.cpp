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

#include <string.h>
#include <limits.h>
#include "screen_content.h"
#include "macros.h"

namespace WelsEnc {

namespace {

typedef struct TagColorBin {
  uint32_t uiKey;
  uint32_t uiCount;
  uint32_t uiY;
  uint32_t uiU;
  uint32_t uiV;
} SColorBin;

static inline uint32_t ScreenColorKey (const uint8_t uiY, const uint8_t uiU, const uint8_t uiV) {
  return ((static_cast<uint32_t> (uiY) >> 2) << 12) |
         ((static_cast<uint32_t> (uiU) >> 3) << 6) |
         (static_cast<uint32_t> (uiV) >> 3);
}

static inline int32_t ScreenAbsDiff (const int32_t iA, const int32_t iB) {
  return iA > iB ? iA - iB : iB - iA;
}

static void SortPaletteByCount (SScreenContentPalette* pPalette) {
  for (int32_t i = 1; i < pPalette->uiSize; ++i) {
    SScreenContentPaletteEntry sEntry = pPalette->sEntry[i];
    int32_t j = i - 1;
    while (j >= 0 && pPalette->sEntry[j].uiCount < sEntry.uiCount) {
      pPalette->sEntry[j + 1] = pPalette->sEntry[j];
      --j;
    }
    pPalette->sEntry[j + 1] = sEntry;
  }
}

static int32_t FindPaletteColor (const SScreenContentPalette& sPalette, const uint8_t uiY, const uint8_t uiU,
                                 const uint8_t uiV) {
  int32_t iBest = -1;
  uint32_t uiBestDistance = UINT_MAX;
  for (int32_t i = 0; i < sPalette.uiSize; ++i) {
    const int32_t iDy = static_cast<int32_t> (uiY) - sPalette.sEntry[i].uiY;
    const int32_t iDu = static_cast<int32_t> (uiU) - sPalette.sEntry[i].uiU;
    const int32_t iDv = static_cast<int32_t> (uiV) - sPalette.sEntry[i].uiV;
    const uint32_t uiDistance = static_cast<uint32_t> (iDy * iDy * 2 + iDu * iDu + iDv * iDv);
    if (uiDistance < uiBestDistance) {
      uiBestDistance = uiDistance;
      iBest = i;
    }
  }
  return iBest;
}

static bool QuantizeMbToPalette (uint8_t* pY, uint8_t* pU, uint8_t* pV,
                                 const int32_t iStrideY, const int32_t iStrideUV,
                                 const int32_t iWidth, const int32_t iHeight,
                                 const SScreenContentPalette& sPalette) {
  for (int32_t y = 0; y < iHeight; ++y) {
    for (int32_t x = 0; x < iWidth; ++x) {
      const uint8_t uiY = pY[y * iStrideY + x];
      const uint8_t uiU = pU[(y >> 1) * iStrideUV + (x >> 1)];
      const uint8_t uiV = pV[(y >> 1) * iStrideUV + (x >> 1)];
      const int32_t iColor = FindPaletteColor (sPalette, uiY, uiU, uiV);
      if (iColor < 0 || ScreenAbsDiff (uiY, sPalette.sEntry[iColor].uiY) > 5 ||
          ScreenAbsDiff (uiU, sPalette.sEntry[iColor].uiU) > 3 ||
          ScreenAbsDiff (uiV, sPalette.sEntry[iColor].uiV) > 3) {
        return false;
      }
    }
  }

  uint32_t uiChanged = 0;
  for (int32_t y = 0; y < iHeight; ++y) {
    for (int32_t x = 0; x < iWidth; ++x) {
      uint8_t* pPixelY = pY + y * iStrideY + x;
      uint8_t* pPixelU = pU + (y >> 1) * iStrideUV + (x >> 1);
      uint8_t* pPixelV = pV + (y >> 1) * iStrideUV + (x >> 1);
      const uint8_t uiU = *pPixelU;
      const uint8_t uiV = *pPixelV;
      const int32_t iColor = FindPaletteColor (sPalette, *pPixelY, uiU, uiV);
      if (iColor < 0) {
        continue;
      }
      if (*pPixelY != sPalette.sEntry[iColor].uiY) {
        ++uiChanged;
      }
      *pPixelY = sPalette.sEntry[iColor].uiY;
      if ((x & 1) == 0 && (y & 1) == 0) {
        if (uiU != sPalette.sEntry[iColor].uiU || uiV != sPalette.sEntry[iColor].uiV) {
          ++uiChanged;
        }
        *pPixelU = sPalette.sEntry[iColor].uiU;
        *pPixelV = sPalette.sEntry[iColor].uiV;
      }
    }
  }
  return uiChanged > 0;
}

static bool IsSameMb (const uint8_t* pY0, const uint8_t* pU0, const uint8_t* pV0,
                      const uint8_t* pY1, const uint8_t* pU1, const uint8_t* pV1,
                      const int32_t iStrideY, const int32_t iStrideUV,
                      const int32_t iWidth, const int32_t iHeight) {
  const int32_t iChromaWidth = (iWidth + 1) >> 1;
  const int32_t iChromaHeight = (iHeight + 1) >> 1;
  for (int32_t y = 0; y < iHeight; ++y) {
    if (memcmp (pY0 + y * iStrideY, pY1 + y * iStrideY, iWidth) != 0) {
      return false;
    }
  }
  for (int32_t y = 0; y < iChromaHeight; ++y) {
    if (memcmp (pU0 + y * iStrideUV, pU1 + y * iStrideUV, iChromaWidth) != 0 ||
        memcmp (pV0 + y * iStrideUV, pV1 + y * iStrideUV, iChromaWidth) != 0) {
      return false;
    }
  }
  return true;
}

static void CopyMb (uint8_t* pDstY, uint8_t* pDstU, uint8_t* pDstV,
                    const uint8_t* pSrcY, const uint8_t* pSrcU, const uint8_t* pSrcV,
                    const int32_t iDstStrideY, const int32_t iDstStrideUV,
                    const int32_t iSrcStrideY, const int32_t iSrcStrideUV,
                    const int32_t iWidth, const int32_t iHeight) {
  const int32_t iChromaWidth = (iWidth + 1) >> 1;
  const int32_t iChromaHeight = (iHeight + 1) >> 1;
  for (int32_t y = 0; y < iHeight; ++y) {
    memcpy (pDstY + y * iDstStrideY, pSrcY + y * iSrcStrideY, iWidth);
  }
  for (int32_t y = 0; y < iChromaHeight; ++y) {
    memcpy (pDstU + y * iDstStrideUV, pSrcU + y * iSrcStrideUV, iChromaWidth);
    memcpy (pDstV + y * iDstStrideUV, pSrcV + y * iSrcStrideUV, iChromaWidth);
  }
}

static uint32_t MbSad (const uint8_t* pY0, const uint8_t* pY1,
                       const int32_t iStrideY, const int32_t iRefStrideY,
                       const int32_t iWidth, const int32_t iHeight) {
  uint32_t uiSad = 0;
  for (int32_t y = 0; y < iHeight; ++y) {
    for (int32_t x = 0; x < iWidth; ++x) {
      uiSad += ScreenAbsDiff (pY0[y * iStrideY + x], pY1[y * iRefStrideY + x]);
    }
  }
  return uiSad;
}

static void AnalyzeMbPalette (const uint8_t* pY, const uint8_t* pU, const uint8_t* pV,
                              const int32_t iStrideY, const int32_t iStrideUV,
                              const int32_t iWidth, const int32_t iHeight,
                              SScreenContentPalette* pPalette) {
  SColorBin sBins[SCREEN_CONTENT_MAX_PALETTE_COLORS];
  int32_t iBinCount = 0;
  bool bOverflow = false;
  memset (sBins, 0, sizeof (sBins));

  for (int32_t y = 0; y < iHeight && !bOverflow; ++y) {
    for (int32_t x = 0; x < iWidth; ++x) {
      const uint8_t uiY = pY[y * iStrideY + x];
      const uint8_t uiU = pU[(y >> 1) * iStrideUV + (x >> 1)];
      const uint8_t uiV = pV[(y >> 1) * iStrideUV + (x >> 1)];
      const uint32_t uiKey = ScreenColorKey (uiY, uiU, uiV);
      int32_t i = 0;
      for (; i < iBinCount; ++i) {
        if (sBins[i].uiKey == uiKey) {
          ++sBins[i].uiCount;
          sBins[i].uiY += uiY;
          sBins[i].uiU += uiU;
          sBins[i].uiV += uiV;
          break;
        }
      }
      if (i == iBinCount) {
        if (iBinCount >= SCREEN_CONTENT_MAX_PALETTE_COLORS) {
          bOverflow = true;
          break;
        }
        sBins[i].uiKey = uiKey;
        sBins[i].uiCount = 1;
        sBins[i].uiY = uiY;
        sBins[i].uiU = uiU;
        sBins[i].uiV = uiV;
        ++iBinCount;
      }
    }
  }

  pPalette->uiSize = static_cast<uint8_t> (bOverflow ? 0 : iBinCount);
  for (int32_t i = 0; i < iBinCount; ++i) {
    // Use stable bin centers rather than averages. This keeps palette entries
    // identical across frames when the source only differs by sensor/UI noise,
    // which is important for the block-copy and P-skip paths.
    pPalette->sEntry[i].uiY = static_cast<uint8_t> (((sBins[i].uiKey >> 12) << 2) + 2);
    pPalette->sEntry[i].uiU = static_cast<uint8_t> (((sBins[i].uiKey >> 6) & 0x3f) << 3) + 4;
    pPalette->sEntry[i].uiV = static_cast<uint8_t> ((sBins[i].uiKey & 0x3f) << 3) + 4;
    pPalette->sEntry[i].uiCount = static_cast<uint16_t> (sBins[i].uiCount);
  }
  SortPaletteByCount (pPalette);
}

}

bool WelsScreenContentExtractPalette (const uint8_t* pY, const uint8_t* pU, const uint8_t* pV,
                                      const int32_t iStrideY, const int32_t iStrideUV,
                                      const int32_t iWidth, const int32_t iHeight,
                                      SScreenContentPalette* pPalette) {
  if (pY == NULL || pU == NULL || pV == NULL || pPalette == NULL || iWidth <= 0 || iHeight <= 0) {
    return false;
  }
  AnalyzeMbPalette (pY, pU, pV, iStrideY, iStrideUV, iWidth, iHeight, pPalette);
  return pPalette->uiSize > 0;
}

bool WelsScreenContentAnalyzeFrame (uint8_t* pY, uint8_t* pU, uint8_t* pV,
                                    const int32_t iStrideY, const int32_t iStrideUV,
                                    const uint8_t* pRefY, const uint8_t* pRefU, const uint8_t* pRefV,
                                    const int32_t iRefStrideY, const int32_t iRefStrideUV, const bool bHasReference,
                                    const int32_t iWidth, const int32_t iHeight,
                                    const bool bEnablePalette, const bool bEnableBlockCopy,
                                    const bool bForceScreenContent,
                                    SScreenContentFrameStats* pStats,
                                    SScreenContentMbInfo* pMbInfo, const int32_t iMbInfoCount) {
  if (pY == NULL || pU == NULL || pV == NULL || pStats == NULL || iWidth <= 0 || iHeight <= 0) {
    return false;
  }

  const int32_t iMbWidth = (iWidth + 15) >> 4;
  const int32_t iMbHeight = (iHeight + 15) >> 4;
  const int32_t iMbCount = iMbWidth * iMbHeight;
  uint32_t uiLowColorMbCount = 0;
  uint32_t uiStaticMbCount = 0;

  memset (pStats, 0, sizeof (*pStats));
  pStats->uiMbCount = static_cast<uint32_t> (iMbCount);
  if (pMbInfo != NULL) {
    memset (pMbInfo, 0, sizeof (SScreenContentMbInfo) * iMbInfoCount);
  }

  for (int32_t iMbY = 0; iMbY < iMbHeight; ++iMbY) {
    for (int32_t iMbX = 0; iMbX < iMbWidth; ++iMbX) {
      const int32_t iX = iMbX << 4;
      const int32_t iY = iMbY << 4;
      const int32_t iMbW = WELS_MIN (16, iWidth - iX);
      const int32_t iMbH = WELS_MIN (16, iHeight - iY);
      const int32_t iMbXY = iMbY * iMbWidth + iMbX;
      SScreenContentPalette sPalette;
      AnalyzeMbPalette (pY + iY * iStrideY + iX, pU + (iY >> 1) * iStrideUV + (iX >> 1),
                        pV + (iY >> 1) * iStrideUV + (iX >> 1), iStrideY, iStrideUV,
                        iMbW, iMbH, &sPalette);
      uint8_t uiFlags = 0;
      if (sPalette.uiSize > 0) {
        uiFlags |= SC_MB_LOW_COLOR;
        ++uiLowColorMbCount;
      }
      if (bHasReference && pRefY != NULL) {
        const uint32_t uiSad = MbSad (pY + iY * iStrideY + iX, pRefY + iY * iRefStrideY + iX,
                                      iStrideY, iRefStrideY, iMbW, iMbH);
        if (uiSad <= static_cast<uint32_t> (iMbW * iMbH * 2)) {
          uiFlags |= SC_MB_STATIC;
          ++uiStaticMbCount;
        }
      }
      if (pMbInfo != NULL && iMbXY < iMbInfoCount) {
        pMbInfo[iMbXY].uiFlags = uiFlags;
        pMbInfo[iMbXY].uiPaletteSize = sPalette.uiSize;
      }
    }
  }

  const float fLowColorRatio = static_cast<float> (uiLowColorMbCount) / iMbCount;
  const float fStaticRatio = bHasReference ? static_cast<float> (uiStaticMbCount) / iMbCount : 0.0f;
  pStats->fScreenContentScore = 0.7f * fLowColorRatio + (bHasReference ? 0.3f * fStaticRatio : 0.0f);
  if (bForceScreenContent) {
    pStats->bScreenContent = true;
  } else if (!bHasReference) {
    pStats->bScreenContent = fLowColorRatio >= 0.50f;
  } else {
    pStats->bScreenContent = (fLowColorRatio >= 0.55f) ||
                             (fLowColorRatio >= 0.35f && fStaticRatio >= 0.25f) ||
                             (fLowColorRatio >= 0.20f && fStaticRatio >= 0.85f);
  }

  if (!pStats->bScreenContent) {
    return false;
  }

  pStats->uiLowColorMbCount = uiLowColorMbCount;
  pStats->uiStaticMbCount = uiStaticMbCount;
  for (int32_t iMbY = 0; iMbY < iMbHeight; ++iMbY) {
    for (int32_t iMbX = 0; iMbX < iMbWidth; ++iMbX) {
      const int32_t iX = iMbX << 4;
      const int32_t iY = iMbY << 4;
      const int32_t iMbW = WELS_MIN (16, iWidth - iX);
      const int32_t iMbH = WELS_MIN (16, iHeight - iY);
      const int32_t iMbXY = iMbY * iMbWidth + iMbX;
      if (pMbInfo == NULL || iMbXY >= iMbInfoCount) {
        continue;
      }
      const bool bBlockCopy = bEnableBlockCopy && bHasReference && pRefY != NULL && pRefU != NULL && pRefV != NULL &&
                              (pMbInfo[iMbXY].uiFlags & SC_MB_STATIC);
      if (bBlockCopy) {
        CopyMb (pY + iY * iStrideY + iX, pU + (iY >> 1) * iStrideUV + (iX >> 1),
                pV + (iY >> 1) * iStrideUV + (iX >> 1),
                pRefY + iY * iRefStrideY + iX, pRefU + (iY >> 1) * iRefStrideUV + (iX >> 1),
                pRefV + (iY >> 1) * iRefStrideUV + (iX >> 1),
                iStrideY, iStrideUV, iRefStrideY, iRefStrideUV, iMbW, iMbH);
        ++pStats->uiCopyMbCount;
      } else if (bEnablePalette && !bHasReference && (pMbInfo[iMbXY].uiFlags & SC_MB_LOW_COLOR)) {
        SScreenContentPalette sPalette;
        AnalyzeMbPalette (pY + iY * iStrideY + iX, pU + (iY >> 1) * iStrideUV + (iX >> 1),
                          pV + (iY >> 1) * iStrideUV + (iX >> 1), iStrideY, iStrideUV,
                          iMbW, iMbH, &sPalette);
        if (sPalette.uiSize > 0) {
          const bool bChanged = QuantizeMbToPalette (pY + iY * iStrideY + iX,
                               pU + (iY >> 1) * iStrideUV + (iX >> 1),
                               pV + (iY >> 1) * iStrideUV + (iX >> 1),
                               iStrideY, iStrideUV, iMbW, iMbH, sPalette);
          if (bChanged) {
            pMbInfo[iMbXY].uiFlags |= SC_MB_PALETTE;
            ++pStats->uiPaletteMbCount;
          }
        }
      }

      if (iMbX > 0 && IsSameMb (pY + iY * iStrideY + iX, pU + (iY >> 1) * iStrideUV + (iX >> 1),
                                pV + (iY >> 1) * iStrideUV + (iX >> 1),
                                pY + iY * iStrideY + iX - 16, pU + (iY >> 1) * iStrideUV + ((iX - 16) >> 1),
                                pV + (iY >> 1) * iStrideUV + ((iX - 16) >> 1),
                                iStrideY, iStrideUV, iMbW, iMbH)) {
        pMbInfo[iMbXY].uiFlags |= SC_MB_COPY_LEFT;
        ++pStats->uiCopyMbCount;
      } else if (iMbY > 0 && IsSameMb (pY + iY * iStrideY + iX, pU + (iY >> 1) * iStrideUV + (iX >> 1),
                                       pV + (iY >> 1) * iStrideUV + (iX >> 1),
                                       pY + (iY - 16) * iStrideY + iX, pU + ((iY - 16) >> 1) * iStrideUV + (iX >> 1),
                                       pV + ((iY - 16) >> 1) * iStrideUV + (iX >> 1),
                                       iStrideY, iStrideUV, iMbW, iMbH)) {
        pMbInfo[iMbXY].uiFlags |= SC_MB_COPY_TOP;
        ++pStats->uiCopyMbCount;
      }
    }
  }
  return true;
}

}
