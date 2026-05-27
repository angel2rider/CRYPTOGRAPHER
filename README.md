# CRYPTOGRAPHER

Convert any file into a **YouTube-safe video** and back again without losing data. Uses lossless FFV1 encoding with noise padding to survive platform re-compression.

## Features

- **YouTube-safe encoding** — dynamic frame resolution with random noise padding so streaming platforms' re-encoders don't corrupt your data
- **Output size ≈ input size** — frame dimensions are calculated per-file so the video file size closely matches the original (as low as 1.13× overhead)
- **Bit-perfect recovery** — original file is restored exactly byte-for-byte
- **Fast C++ implementation** — compiled binary (`cryptographer`), no runtime dependencies beyond FFmpeg
- **Companion tools** — encode/decode files to/from monochrome images (`f2i.py` / `i2f.py`)

## Files

| File | Purpose |
|------|---------|
| `cryptographer` | YouTube-safe file↔video encoder/decoder (compiled from `yt-safe.cpp`) |
| `yt-safe.cpp` | Source code for the `cryptographer` binary |
| `f2i.py` | Convert a file into a monochrome PNG image |
| `i2f.py` | Recover a file from a monochrome PNG image |

## Dependencies

- **FFmpeg** — `brew install ffmpeg` (macOS) or `apt install ffmpeg` (Linux)
- C++17 compiler — only needed if rebuilding from source

## Usage

### Encode a file into a YouTube-safe video

```bash
./cryptographer -e myfile.zip output_video.avi
```

### Decode a video back into the original file

```bash
./cryptographer -d output_video.avi myfile_restored.zip
```

Flags:
- `-e <input_file> <output_video>` — encode
- `-d <input_video> <output_file>` — decode

### File ↔ Image tools

```bash
python f2i.py      # prompts for file → saves monochrome PNG
python i2f.py      # prompts for PNG → recovers original file
```

## How it works

1. Frame resolution is calculated dynamically per file so the raw pixel data ≈ file size (minimizing wasted space)
2. Each frame has a 16-byte header with width, height, frame index, and payload size
3. Unused pixels are filled with random noise so YouTube's encoder can't compress them away
4. On decode, `ffprobe` reads the video dimensions and the binary extracts the exact payload from each frame

## License

Apache License 2.0
