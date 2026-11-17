# Screen Content Coding

OpenH264 emits standard H.264/AVC. AVC does not define the HEVC-SCC
`palette` or `intra_block_copy` syntax, so this implementation does not add
non-standard slice syntax. Instead it adds screen-content detection and maps
palette/block-copy decisions to coding tools that every AVC decoder can
consume:

- low-color blocks are normalized to a stable palette before residual coding
  and are then coded with the normal intra path;
- static blocks can copy the collocated reference block, after which the
  existing SCC mode decision turns them into P-skip/LTR or 16x16 inter blocks;
- identical left/top blocks are detected as same-frame copy candidates for
  statistics and mode-decision hints.

This keeps the output bitstream interoperable with standard H.264 decoders.

## Enabling The Tools

`SEncParamExt` has three new opt-in fields:

| Field | Default | Description |
| --- | --- | --- |
| `bEnableScreenContentDetection` | `false` | Classify each frame as camera or screen content before enabling screen tools. |
| `bEnablePalette` | `false` | Normalize low-color I/IDR blocks to a stable palette. |
| `bEnableIntraBlockCopy` | `false` | Enable block-copy decisions and static reference-block reuse. |

`INPUT_CONTENT_TYPE_ALL` is the convenience mode. It normalizes to the
screen-content path at initialization, enables detection and block copy, and
keeps palette preprocessing opt-in.

The console encoder exposes the same controls:

```text
-scdetect 1
-palette 1
-ibc 1
```

Configuration-file keys are:

```text
EnableScreenContentDetection 1
EnablePalette                0
EnableIntraBlockCopy         1
```

Typical screen-sharing configuration:

```text
UsageType                        4
EnableScreenContentDetection     1
EnableIntraBlockCopy             1
EnableLongTermReference          1
```

## Detection

The detector measures two signals over 16x16 blocks:

- the fraction of blocks whose YUV colors fit into at most 16 stable bins;
- the fraction of blocks whose collocated luma SAD against the reference is
  at most two samples per pixel.

A frame is classified as screen content when the low-color ratio is high, or
when a moderate low-color ratio is combined with a high static ratio. Frames
that are classified as camera content use the normal camera mode-decision
path when auto-detection is enabled.

## Palette And Block Copy

Palette preprocessing only runs on I/IDR low-color blocks. A block is skipped
if it needs more than 16 colors or if any pixel is farther than the configured
safe error from its palette entry. Stable bin centers are used so the same UI
colors map to the same entries across frames.

For P frames, `bEnableIntraBlockCopy` marks static collocated blocks and copies
the reference block before encoding. This removes repeated screen-capture
noise and lets the existing static/scroll SCC decision produce skip or integer
block-copy blocks. Same-frame left/top copies are also detected and exposed in
the per-macroblock analysis flags.

## Rate Control

The tools do not bypass or replace the existing rate controller. They run
before `pfWelsRcPictureInit`, consume the normal SCC complexity input, and
produce standard modes for `WelsRcMbInitScc`. Target bitrate, QP range, frame
skipping, LTR, and buffer management remain under the existing SCC rate-control
logic.

## Tests

Unit tests cover classification, palette bounds, static block copy, and encoder
initialization through the auto mode:

```shell
make test
```

The focused tests are in `test/encoder/EncUT_ScreenContent.cpp`.

With all three new options disabled, the existing screen-content configuration
was also compared byte-for-byte against the same OpenH264 revision before the
change; the generated bitstreams were identical.

The reproducible comparison script generates a synthetic desktop sequence with
UI panels, text-like edges, small capture noise, and a moving region:

```shell
python3 test/screen_content_compare.py
```

Example result on Linux x86-64, 320x192, 60 frames, quality mode,
1 Mbit/s target:

| Case | Bytes | kbps | PSNR (YUV) |
| --- | ---: | ---: | ---: |
| Camera path | 11029 | 44.1 | 44.09 dB |
| Screen baseline | 10422 | 41.7 | 44.20 dB |
| Screen + block copy | 10312 | 41.2 | 44.19 dB |
| Screen + palette + block copy | 10503 | 42.0 | 43.62 dB |
| Auto detection + block copy | 10312 | 41.2 | 44.19 dB |

For this sequence, auto detection plus block copy reduces the bitstream by
about 6.5% versus the camera path at essentially the same PSNR. Palette
preprocessing is content-dependent and is therefore opt-in; on very clean,
already-compressible UI it can cost bytes, while on noisy or lossy screen
captures it can remove color noise before residual coding.

## Limitations

- Arbitrary same-frame block copies cannot be signaled in AVC. The encoder
  detects them, but only block copies expressible through reference pictures,
  skip modes, or standard intra predictors can be emitted.
- A true HEVC-SCC palette map requires an HEVC bitstream and decoder. This
  implementation provides an AVC-compatible palette normalization path, not a
  new macroblock syntax.
- Screen detection uses content statistics, so deliberate scene transitions
  can briefly be classified as camera content. The encoder falls back to the
  normal path for those frames.
