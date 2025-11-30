CRYPTOGRAPHER

Convert any file into a video and back again without losing data. Perfect for archiving, large file transport, or creative storage in a video format.

Features

Encode files of any size into a video (FFV1 lossless codec)

Decode videos back into the original file, bit-perfect

Progress bars with tqdm for easy tracking

Works on macOS, Linux, Colab (CPU-only)

Streams frames efficiently — no huge memory usage

Optional: can be adapted for YouTube-safe videos with error-tolerant encoding

Dependencies

Python 3

FFmpeg (brew install ffmpeg on macOS, sudo apt install ffmpeg on Linux)

tqdm (for progress bars): pip install tqdm

No other libraries are required.

Usage
Encode a file into a video

Run the script: python main.py

Choose e for encode

Enter the input file path

Enter the output video path (e.g., output.avi)

The script will show a progress bar for encoding frames. The output is a lossless video containing your file data.

Decode a video back into a file

Run the script: python main.py

Choose d for decode

Enter the input video path

Enter the output file path

The script will reconstruct the original file exactly, verified via hash.

Notes & Tips

Large files: encoding/decoding may take minutes to hours depending on file size and CPU.

CPU-bound: this version uses CPU only for lossless encoding; no GPU is required.

YouTube-safe videos: to share encoded videos on platforms that recompress, map pixel values to safe ranges to reduce compression artifacts.

Colab-friendly: works perfectly on Google Colab; just install FFmpeg and optionally mount Google Drive for large files.

License

This project is open-source and free to use under the MIT License.

Example Workflow

Encode a file: run the script, choose e, input file path, output video path

Decode a file: run the script, choose d, input video path, output file path

Verify integrity with SHA256: compare the hash of the original file and the restored file. ✅ They should match exactly.
