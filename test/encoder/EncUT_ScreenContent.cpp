#include <gtest/gtest.h>

#include <stdlib.h>
#include <string.h>
#include <vector>

#include "codec_api.h"
#include "screen_content.h"

using namespace WelsEnc;

namespace {

const int32_t kWidth = 64;
const int32_t kHeight = 64;

void FillScreenFrame (std::vector<uint8_t>* pFrame) {
  const int32_t iLumaSize = kWidth * kHeight;
  const int32_t iChromaSize = iLumaSize >> 2;
  pFrame->assign (iLumaSize + 2 * iChromaSize, 0);
  for (int32_t y = 0; y < kHeight; ++y) {
    for (int32_t x = 0; x < kWidth; ++x) {
      uint8_t uiValue = 32;
      if (x >= 8 && x < 56 && y >= 8 && y < 56) {
        uiValue = 220;
      }
      if (y >= 8 && y < 20) {
        uiValue = 64;
      }
      if ((y % 12) < 2 && x >= 12 && x < 52) {
        uiValue = 16;
      }
      (*pFrame)[y * kWidth + x] = uiValue;
    }
  }
  for (int32_t i = 0; i < iChromaSize; ++i) {
    (*pFrame)[iLumaSize + i] = 128;
    (*pFrame)[iLumaSize + iChromaSize + i] = 128;
  }
}

void FillNoiseFrame (std::vector<uint8_t>* pFrame) {
  pFrame->resize (kWidth * kHeight * 3 / 2);
  for (size_t i = 0; i < pFrame->size(); ++i) {
    (*pFrame)[i] = static_cast<uint8_t> (rand() & 0xff);
  }
}

void FillInputPicture (SSourcePicture* pPicture, std::vector<uint8_t>* pFrame) {
  memset (pPicture, 0, sizeof (*pPicture));
  pPicture->iPicWidth = kWidth;
  pPicture->iPicHeight = kHeight;
  pPicture->iColorFormat = videoFormatI420;
  pPicture->iStride[0] = kWidth;
  pPicture->iStride[1] = pPicture->iStride[2] = kWidth >> 1;
  pPicture->pData[0] = pFrame->data();
  pPicture->pData[1] = pPicture->pData[0] + kWidth * kHeight;
  pPicture->pData[2] = pPicture->pData[1] + (kWidth * kHeight >> 2);
}

}

TEST (ScreenContentTest, DetectsUiAndRejectsNoise) {
  std::vector<uint8_t> frame;
  FillScreenFrame (&frame);
  const int32_t iLumaSize = kWidth * kHeight;
  const int32_t iChromaSize = iLumaSize >> 2;
  const int32_t iMbCount = ((kWidth + 15) >> 4) * ((kHeight + 15) >> 4);
  std::vector<SScreenContentMbInfo> mbInfo (iMbCount);
  SScreenContentFrameStats stats;

  EXPECT_TRUE (WelsScreenContentAnalyzeFrame (frame.data(), frame.data() + iLumaSize,
               frame.data() + iLumaSize + iChromaSize,
               kWidth, kWidth >> 1, NULL, NULL, NULL, 0, 0, false,
               kWidth, kHeight, false, false, false, &stats, mbInfo.data(), iMbCount));
  EXPECT_TRUE (stats.bScreenContent);
  EXPECT_GT (stats.uiLowColorMbCount, 0u);

  FillNoiseFrame (&frame);
  EXPECT_FALSE (WelsScreenContentAnalyzeFrame (frame.data(), frame.data() + iLumaSize,
                frame.data() + iLumaSize + iChromaSize,
                kWidth, kWidth >> 1, NULL, NULL, NULL, 0, 0, false,
                kWidth, kHeight, false, false, false, &stats, mbInfo.data(), iMbCount));
  EXPECT_FALSE (stats.bScreenContent);
}

TEST (ScreenContentTest, ExtractsBoundedPalette) {
  std::vector<uint8_t> frame;
  FillScreenFrame (&frame);
  const int32_t iLumaSize = kWidth * kHeight;
  const int32_t iChromaSize = iLumaSize >> 2;
  SScreenContentPalette palette;

  EXPECT_TRUE (WelsScreenContentExtractPalette (frame.data(), frame.data() + iLumaSize,
               frame.data() + iLumaSize + iChromaSize,
               kWidth, kWidth >> 1, 16, 16, &palette));
  EXPECT_GT (palette.uiSize, 0);
  EXPECT_LE (palette.uiSize, SCREEN_CONTENT_MAX_PALETTE_COLORS);

  for (int32_t y = 0; y < 16; ++y) {
    for (int32_t x = 0; x < 16; ++x) {
      frame[y * kWidth + x] = static_cast<uint8_t> ((x + y * 16) * 4);
    }
  }
  EXPECT_FALSE (WelsScreenContentExtractPalette (frame.data(), frame.data() + iLumaSize,
                frame.data() + iLumaSize + iChromaSize,
                kWidth, kWidth >> 1, 16, 16, &palette));
}

TEST (ScreenContentTest, BlockCopyReusesStaticReferenceBlocks) {
  std::vector<uint8_t> first;
  std::vector<uint8_t> second;
  FillScreenFrame (&first);
  second = first;
  second[5] ^= 1;
  const int32_t iLumaSize = kWidth * kHeight;
  const int32_t iChromaSize = iLumaSize >> 2;
  const int32_t iMbCount = ((kWidth + 15) >> 4) * ((kHeight + 15) >> 4);
  std::vector<SScreenContentMbInfo> mbInfo (iMbCount);
  SScreenContentFrameStats stats;

  ASSERT_TRUE (WelsScreenContentAnalyzeFrame (first.data(), first.data() + iLumaSize,
               first.data() + iLumaSize + iChromaSize,
               kWidth, kWidth >> 1, NULL, NULL, NULL, 0, 0, false,
               kWidth, kHeight, false, true, false, &stats, mbInfo.data(), iMbCount));
  EXPECT_TRUE (WelsScreenContentAnalyzeFrame (second.data(), second.data() + iLumaSize,
               second.data() + iLumaSize + iChromaSize,
               kWidth, kWidth >> 1, first.data(), first.data() + iLumaSize,
               first.data() + iLumaSize + iChromaSize, kWidth, kWidth >> 1, true,
               kWidth, kHeight, false, true, false, &stats, mbInfo.data(), iMbCount));
  EXPECT_GT (stats.uiStaticMbCount, 0u);
  EXPECT_GT (stats.uiCopyMbCount, 0u);
  EXPECT_EQ (first[5], second[5]);
}

TEST (ScreenContentTest, HandlesPartialMacroblocks) {
  const int32_t iWidth = 50;
  const int32_t iHeight = 34;
  const int32_t iLumaSize = iWidth * iHeight;
  const int32_t iChromaSize = ((iWidth + 1) >> 1) * ((iHeight + 1) >> 1);
  std::vector<uint8_t> frame (iLumaSize + 2 * iChromaSize, 48);
  for (int32_t y = 0; y < iHeight; ++y) {
    for (int32_t x = 0; x < iWidth; ++x) {
      frame[y * iWidth + x] = ((x >> 3) + (y >> 3)) & 1 ? 192 : 32;
    }
  }
  const int32_t iMbCount = ((iWidth + 15) >> 4) * ((iHeight + 15) >> 4);
  std::vector<SScreenContentMbInfo> mbInfo (iMbCount);
  SScreenContentFrameStats stats;

  EXPECT_TRUE (WelsScreenContentAnalyzeFrame (frame.data(), frame.data() + iLumaSize,
               frame.data() + iLumaSize + iChromaSize, iWidth, (iWidth + 1) >> 1,
               NULL, NULL, NULL, 0, 0, false, iWidth, iHeight,
               false, false, false, &stats, mbInfo.data(), iMbCount));
  EXPECT_TRUE (stats.bScreenContent);
}

TEST (ScreenContentTest, EncoderAcceptsAutoScreenOptions) {
  ISVCEncoder* pEncoder = NULL;
  ASSERT_EQ (0, WelsCreateSVCEncoder (&pEncoder));
  ASSERT_TRUE (pEncoder != NULL);

  SEncParamExt param;
  ASSERT_EQ (0, pEncoder->GetDefaultParams (&param));
  param.iUsageType = INPUT_CONTENT_TYPE_ALL;
  param.iPicWidth = kWidth;
  param.iPicHeight = kHeight;
  param.fMaxFrameRate = 30.0f;
  param.iTargetBitrate = 200000;
  param.iSpatialLayerNum = 1;
  param.sSpatialLayers[0].iVideoWidth = kWidth;
  param.sSpatialLayers[0].iVideoHeight = kHeight;
  param.sSpatialLayers[0].fFrameRate = 30.0f;
  param.sSpatialLayers[0].iSpatialBitrate = 200000;
  param.sSpatialLayers[0].sSliceArgument.uiSliceMode = SM_SINGLE_SLICE;
  param.bEnableSceneChangeDetect = true;
  param.bEnableLongTermReference = true;
  param.bIsLosslessLink = true;

  ASSERT_EQ (0, pEncoder->InitializeExt (&param));

  std::vector<uint8_t> frame;
  FillScreenFrame (&frame);
  SSourcePicture picture;
  FillInputPicture (&picture, &frame);
  SFrameBSInfo frameInfo;
  memset (&frameInfo, 0, sizeof (frameInfo));
  EXPECT_EQ (0, pEncoder->EncodeFrame (&picture, &frameInfo));
  EXPECT_NE (videoFrameTypeSkip, frameInfo.eFrameType);

  pEncoder->Uninitialize();
  WelsDestroySVCEncoder (pEncoder);
}
