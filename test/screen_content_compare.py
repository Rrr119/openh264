#!/usr/bin/env python3
"""Reproducible screen-content coding comparison for OpenH264.

The script generates a small synthetic desktop sequence, encodes it with
several AVC-compatible screen-content configurations, decodes it, and reports
bitrate plus PSNR against the generated source.
"""

import argparse
import math
import os
import random
import subprocess
import tempfile


WIDTH = 320
HEIGHT = 192
FRAMES = 60


def generate_sequence(path):
    random.seed(7)
    colors = [
        (24, 110, 170),
        (230, 230, 230),
        (30, 30, 30),
        (60, 160, 80),
        (190, 40, 40),
        (90, 90, 200),
        (240, 200, 40),
        (128, 128, 128),
    ]
    with open(path, "wb") as output:
        for frame in range(FRAMES):
            y_plane = bytearray(WIDTH * HEIGHT)
            u_plane = bytearray(WIDTH * HEIGHT // 4)
            v_plane = bytearray(WIDTH * HEIGHT // 4)
            for y in range(HEIGHT):
                for x in range(WIDTH):
                    color = 0
                    if 16 <= x < 304 and 16 <= y < 176:
                        color = 1
                    if 16 <= x < 304 and 16 <= y < 40:
                        color = 0
                    if 40 <= y < 160 and 40 <= x < 280 and ((y - 40) // 20) % 2 == 0 and (x // 7) % 3:
                        color = 2
                    if 60 <= y < 160 and 40 <= x < 280 and ((y - 60) // 32) % 2 == 0:
                        color = 3
                    if 80 <= y < 160 and 50 <= x < 270 and ((x - 50) // 48) % 3 == 0:
                        color = 4
                    if 100 <= y < 140 and 50 <= x < 270 and ((x + 40 - frame * 2) // 50) % 4 == 1:
                        color = 5
                    yy, uu, vv = colors[color]
                    y_plane[y * WIDTH + x] = max(0, min(255, yy + random.randint(-2, 2)))
                    u_plane[(y // 2) * (WIDTH // 2) + x // 2] = max(
                        0, min(255, uu + random.randint(-1, 1))
                    )
                    v_plane[(y // 2) * (WIDTH // 2) + x // 2] = max(
                        0, min(255, vv + random.randint(-1, 1))
                    )
            output.write(y_plane)
            output.write(u_plane)
            output.write(v_plane)


def write_config(path, source, output):
    with open(path, "w", encoding="ascii") as config:
        config.write(
            f"""UsageType                        1
SourceWidth                      {WIDTH}
SourceHeight                     {HEIGHT}
InputFile                        {source}
OutputFile                       {output}
MaxFrameRate                     30
FramesToBeEncoded                {FRAMES}
TemporalLayerNum                 1
IntraPeriod                      0
SpsPpsIDStrategy                 1
EnableFrameCropping              1
EntropyCodingModeFlag            0
MaxNalSize                       0
ComplexityMode                   0
LoopFilterDisableIDC             0
LoopFilterAlphaC0Offset          0
LoopFilterBetaOffset             0
MultipleThreadIdc                1
UseLoadBalancing                 1
RCMode                           0
TargetBitrate                    1000
MaxOverallBitrate                0
EnableFrameSkip                  1
MaxQp                            35
MinQp                            26
EnableDenoise                    0
EnableSceneChangeDetection       1
EnableBackgroundDetection        0
EnableAdaptiveQuantization       0
EnableLongTermReference          1
LtrMarkPeriod                    30
PrefixNALAddingCtrl              0
NumLayers                        1
LayerCfg                         testbin/layer2.cfg
"""
        )


def run(command, cwd):
    subprocess.run(command, cwd=cwd, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT)


def psnr(source_path, decoded_path):
    with open(source_path, "rb") as source_file:
        source = source_file.read()
    with open(decoded_path, "rb") as decoded_file:
        decoded = decoded_file.read()
    size = min(len(source), len(decoded))
    if size == 0:
        return 0.0
    squared_error = 0
    for index in range(size):
        difference = source[index] - decoded[index]
        squared_error += difference * difference
    mse = squared_error / size
    return 99.0 if mse == 0 else 10.0 * math.log10(255.0 * 255.0 / mse)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", default=os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    parser.add_argument("--encoder", default="./h264enc")
    parser.add_argument("--decoder", default="./h264dec")
    args = parser.parse_args()

    root = os.path.abspath(args.root)
    with tempfile.TemporaryDirectory(prefix="openh264_scc_") as temp:
        source = os.path.join(temp, "screen.yuv")
        config = os.path.join(temp, "screen.cfg")
        generate_sequence(source)
        write_config(config, source, os.path.join(temp, "base.264"))

        cases = [
            ("camera", ["-utype", "0", "-scdetect", "0", "-palette", "0", "-ibc", "0"]),
            ("screen-base", ["-utype", "1", "-scdetect", "0", "-palette", "0", "-ibc", "0"]),
            ("screen-copy", ["-utype", "1", "-scdetect", "0", "-palette", "0", "-ibc", "1"]),
            ("screen-palette", ["-utype", "1", "-scdetect", "0", "-palette", "1", "-ibc", "1"]),
            ("auto", ["-utype", "4", "-scdetect", "1", "-palette", "0", "-ibc", "1"]),
        ]

        print(f"{'case':16} {'bytes':>8} {'kbps':>8} {'psnr-yuv':>9}")
        for name, options in cases:
            bitstream = os.path.join(temp, name + ".264")
            decoded = os.path.join(temp, name + ".yuv")
            run([args.encoder, config, "-bf", bitstream, *options], root)
            run([args.decoder, bitstream, decoded], root)
            size = os.path.getsize(bitstream)
            kbps = size * 8 * 30.0 / FRAMES / 1000.0
            print(f"{name:16} {size:8d} {kbps:8.1f} {psnr(source, decoded):9.2f}")


if __name__ == "__main__":
    main()
