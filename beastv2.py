import os
import math
import subprocess
from tqdm import tqdm

# CONFIG
FRAME_WIDTH = 1920
FRAME_HEIGHT = 1080
PACK_PER_PIXEL = 3  # RGB
CHUNK_SIZE = FRAME_WIDTH * FRAME_HEIGHT * PACK_PER_PIXEL
FPS = 30
FFMPEG_CODEC = "ffv1"  # lossless

def encode_file_to_video(file_path, video_path):
    file_size = os.path.getsize(file_path)
    print(f"[INFO] Original file size: {file_size} bytes")

    # First chunk will store file size
    first_chunk_size = CHUNK_SIZE - 8

    ffmpeg_cmd = [
        "ffmpeg",
        "-y",  # overwrite
        "-f", "rawvideo",
        "-pix_fmt", "rgb24",
        "-s", f"{FRAME_WIDTH}x{FRAME_HEIGHT}",
        "-r", str(FPS),
        "-i", "-",  # input from stdin
        "-c:v", FFMPEG_CODEC,
        "-preset", "ultrafast",
        video_path
    ]

    proc = subprocess.Popen(ffmpeg_cmd, stdin=subprocess.PIPE)

    with open(file_path, "rb") as f:
        frame_index = 0
        while True:
            if frame_index == 0:
                chunk = f.read(first_chunk_size)
                chunk = file_size.to_bytes(8, "big") + chunk
            else:
                chunk = f.read(CHUNK_SIZE)

            if not chunk:
                break

            # Pad last chunk
            if len(chunk) < CHUNK_SIZE:
                chunk += b'\x00' * (CHUNK_SIZE - len(chunk))

            # Send to ffmpeg
            proc.stdin.write(chunk)
            frame_index += 1
            if frame_index % 10 == 0:
                print(f"[INFO] Encoded {frame_index} frames...")

    proc.stdin.close()
    proc.wait()
    print(f"[SUCCESS] Video saved: {video_path}, total frames: {frame_index}")

def decode_video_to_file(video_path, output_file):
    # FFmpeg command to output raw RGB frames
    ffmpeg_cmd = [
        "ffmpeg",
        "-i", video_path,
        "-f", "rawvideo",
        "-pix_fmt", "rgb24",
        "-"
    ]

    proc = subprocess.Popen(ffmpeg_cmd, stdout=subprocess.PIPE)

    # Read in chunks
    CHUNK_READ = FRAME_WIDTH * FRAME_HEIGHT * PACK_PER_PIXEL
    all_bytes = bytearray()
    frame_index = 0

    print("[INFO] Decoding frames...")
    while True:
        chunk = proc.stdout.read(CHUNK_READ)
        if not chunk:
            break
        all_bytes.extend(chunk)
        frame_index += 1
        if frame_index % 10 == 0:
            print(f"[INFO] Decoded {frame_index} frames...")

    proc.stdout.close()
    proc.wait()

    # Recover original file size
    original_size = int.from_bytes(all_bytes[:8], "big")
    file_data = all_bytes[8:8+original_size]

    with open(output_file, "wb") as f:
        f.write(file_data)

    print(f"[SUCCESS] File restored: {output_file}, size: {len(file_data)} bytes")

# MAIN
if __name__ == "__main__":
    print("=== LOSSLESS FILE <-> VIDEO (FFMPEG STREAM) ===")
    mode = input("Encode or Decode? (e/d): ").strip().lower()
    if mode == "e":
        file_path = input("Enter file path: ").strip()
        video_path = input("Enter output video path (e.g., output.avi): ").strip()
        encode_file_to_video(file_path, video_path)
    elif mode == "d":
        video_path = input("Enter video path: ").strip()
        output_file = input("Enter output file path: ").strip()
        decode_video_to_file(video_path, output_file)
    else:
        print("[ERROR] Invalid option.")

