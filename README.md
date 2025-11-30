Lossless File ↔ Video Encoder/Decoder

Convert any file into a video and back again without losing data. Perfect for archiving, large file transport, or creative storage in a video format.

Features

Encode files of any size into a video (FFV1 lossless codec).

Decode videos back into the original file, bit-perfect.

Progress bars with tqdm for easy tracking.

Works on macOS, Linux, Colab (CPU-only).

Streams frames efficiently — no huge memory usage.

Optional: can be adapted for YouTube-safe videos with error-tolerant encoding.

Dependencies

Python 3

FFmpeg (brew install ffmpeg on macOS, sudo apt install ffmpeg on Linux)

tqdm (for progress bars):

pip install tqdm


No other libraries are required.

Usage
1. Encode a file into a video
python beast.py


Then select e (encode), and follow prompts:

Encode or Decode? (e/d): e
Enter file path: myfile.zip
Enter output video path (e.g., output.avi): myfile_video.avi


The script will show a progress bar for frame encoding.

The output is a lossless video containing your file data.

2. Decode a video back into a file
python beast.py


Then select d (decode), and follow prompts:

Encode or Decode? (e/d): d
Enter video path: myfile_video.avi
Enter output file path: restored.zip


The script will reconstruct the original file exactly, verified via hash.

Notes & Tips

Large files: encoding/decoding may take minutes to hours depending on file size and CPU.

CPU-bound: this version uses CPU only for lossless encoding; no GPU is required.

YouTube-safe videos: if you want to share encoded videos on platforms that recompress (YouTube, etc.), map pixel values to safe ranges to reduce compression artifacts.

Colab-friendly: works perfectly on Google Colab; just install FFmpeg and optionally mount Google Drive for large files.

License

This project is open-source and free to use under the MIT License.

Example Workflow
# Encode a file
python beast.py
# e → input file → output video

# Decode the video
python beast.py
# d → input video → output file


Verify integrity with SHA256:

shasum -a 256 original.zip
shasum -a 256 restored.zip


✅ Hashes should match exactly.
