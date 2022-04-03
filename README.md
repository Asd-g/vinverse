## Description

Vinverse is a simple filter to remove residual combing, based on an AviSynth script by Didée and originally written by tritical.

This plugin also includes a fast implementation of [Vinverse2 function](https://forum.doom9.org/showthread.php?p=1584186#post1584186).

### Requirements:

- AviSynth 2.60 / AviSynth+ 3.4 or later

- Microsoft VisualC++ Redistributable Package 2022 (can be downloaded from [here](https://github.com/abbodi1406/vcredist/releases)) (Windows only)

### Usage:

```
vinverse (clip input, float "sstr", int "amnt", int "uv", float "scl", int "opt", clip "clip2", int "thr")
```
```
vinverse2 (clip input, float "sstr", int "amnt", int "uv", float "scl", int "opt")
```

### Parameters:

- input\
    A clip to process.\
    Must be in YUV 8..16-bit planar format.

- sstr\
    Strength of contra sharpening.\
    Default: 2.7.

- amnt\
    Change no pixel by more than this.\
    Must be between 0 and range_max.\
    Default: range_max ((2 ^ bit_depth) - 1).

- uv\
    Chroma mode.\
    1: Return garbage.\
    2: Copy plane.\
    3: Process plane.\
    Default: 3.

- scl\
    Scale factor for `VshrpD*VblurD < 0`.\
    Default: 0.25.

- opt\
    Sets which cpu optimizations to use.\
    -1: Auto-detect.\
    0: Use C++ code.\
    1: Use SSE2 code.\
    2: Use AVX2 code.\
    3: Use AVX512 code.\
    Default: -1.

- clip2 (vinverse only)\
    External clip from which to take contra sharpening.\
    This clip must be the same length, width, height, and colorspace as the input clip.

- thr (vinverse only)\
    Threshold.\
    If the absolute difference between the source and the blurred pixel is below this threshold, the pixel is just copied otherwirse it's processed.\
    Must be between 0 and range_max.\
    Default: 0 (every pixel is processed).

### Building:

- Windows\
    Use solution files.

- Linux
    ```
    Requirements:
        - Git
        - C++17 compiler
        - CMake >= 3.16
    ```
    ```
    git clone https://github.com/Asd-g/vinverse && \
    cd vinverse && \
    mkdir build && \
    cd build && \

    cmake ..
    make -j$(nproc)
    sudo make install
    ```
