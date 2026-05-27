# KNOWLEDGE.md — CRYPTOGRAPHER

## Overview

**CRYPTOGRAPHER** converts arbitrary binary files into **YouTube-safe lossless videos** (and back). The primary implementation is `yt-safe.cpp`, compiled to the `cryptographer` binary. Companion Python scripts (`f2i.py` / `i2f.py`) provide file-to-image encoding as a secondary feature.

---

## Project Structure

| File | Language | Purpose |
|------|----------|---------|
| `cryptographer` | Binary | Compiled YouTube-safe encoder/decoder |
| `yt-safe.cpp` | C++ | Source — YouTube-safe encoding with frame indexing, headers, and noise padding |
| `f2i.py` | Python | Encode a file into a monochrome PNG image |
| `i2f.py` | Python | Decode a monochrome PNG image back into a file |
| `README.md` | Docs | User-facing documentation |
| `LICENSE` | Text | Apache License 2.0 |
| `KNOWLEDGE.md` | Docs | This file — project knowledge base |

---

## yt-safe.cpp — YouTube-Safe Encoder/Decoder

The main tool. Compiled to the `cryptographer` binary.

### Configuration

| Parameter | Value |
|-----------|-------|
| Resolution | Dynamic — calculated per file to minimize wasted pixels (max 1920×1080) |
| FPS | 30 |
| Codec | FFV1 |
| Frame size (bytes) | `width × height × 3` — varies per file |
| Header per frame | 16 bytes (width, height, frame index, payload size; little-endian) |
| Padding | Random noise (`rand() % 256`) — prevents compression artifacts |

### How it works

- **Dimension calculation:** For each file, the optimal resolution is computed so the frame holds the file data with minimal wasted space. For files ≤ ~6MB, a single frame with custom dimensions is used. Larger files use multiple 1920×1080 frames.
- **Encode:** Reads file in chunks, packs each chunk into a frame with a header, fills remaining bytes with random noise, pipes raw RGB24 frames to `ffmpeg -c:v ffv1`.
- **Decode:** Uses `ffprobe` to detect video dimensions, then reads frames from `ffmpeg` pipe, extracts the 16-byte header, writes payload bytes to the output file.
- **Noise padding:** Unused pixel channels are filled with random values so streaming platforms' re-encoders can't use compression optimization to alter the data.
- **Output size:** The video file on disk is typically only 1.1–1.3× the input file size (the overhead is the AVI container + FFV1 codec headers, ~15KB).

### CLI

```
./cryptographer -e input output_video    # encode
./cryptographer -d input_video output    # decode
```

### Dependencies

- FFmpeg (`brew install ffmpeg`)
- C++17 compiler (only for rebuilding)

---

## Python Companion Scripts

### f2i.py — File to Image

- Reads binary file, converts each byte to 8 bits.
- Arranges bits into a square 1-bit monochrome PNG.
- Calculates `side = ceil(sqrt(total_bits))`.
- **Use case:** Visualizing binary data, QR-code-like representations.

### i2f.py — Image to File

- Reads a monochrome (1-bit) PNG using PIL.
- Converts pixel data (0 or 255) into bits, assembles bytes, writes binary file.
- **Use case:** Recovering data from bit-level encoded PNGs.

---

## Key Concepts

1. **Byte-to-pixel mapping:** Each byte becomes one RGB channel value (0–255). Three bytes fill one RGB pixel.
2. **Dynamic resolution:** Frame dimensions are calculated per-file so raw pixel data ≈ file size. This keeps the output video size close to the input while minimizing wasted noise padding.
3. **YouTube safety:** Dynamic resolution + random noise padding mean re-encoders can't corrupt data via compression optimization.
4. **Frame-level headers:** Each frame has a 16-byte header with width, height, frame index, and payload size for robustness.
5. **Lossless guarantee:** FFV1 codec + raw RGB24 ensures bit-perfect roundtrip when not re-encoded by a third party.
