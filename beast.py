import os
import math
import numpy as np
import cv2
from tqdm import tqdm

# CONFIGURATION
FRAME_WIDTH = 1920
FRAME_HEIGHT = 1080
FPS = 30
PACK_PER_PIXEL = 3  # bytes per pixel (RGB)
VIDEO_CODEC = "FFV1"  # lossless for bit-perfect storage
CHUNK_BYTES = FRAME_WIDTH * FRAME_HEIGHT * PACK_PER_PIXEL

def encode_file_to_video(file_path, video_path):
    file_size = os.path.getsize(file_path)
    print(f"[INFO] Original file size: {file_size} bytes")

    fourcc = cv2.VideoWriter_fourcc(*VIDEO_CODEC)
    out = cv2.VideoWriter(video_path, fourcc, FPS, (FRAME_WIDTH, FRAME_HEIGHT))

    # Capacity per frame
    FRAME_CAPACITY = FRAME_WIDTH * FRAME_HEIGHT * 3
    first_frame_capacity = FRAME_CAPACITY - 8  # reserve 8 bytes for original size

    with open(file_path, "rb") as f:
        frame_index = 0
        while True:
            if frame_index == 0:
                chunk = f.read(first_frame_capacity)
                chunk = file_size.to_bytes(8, "big") + chunk
            else:
                chunk = f.read(FRAME_CAPACITY)

            if not chunk:
                break

            # Pad chunk if needed
            if len(chunk) < FRAME_CAPACITY:
                chunk += b'\x00' * (FRAME_CAPACITY - len(chunk))

            # Convert to frame
            frame = np.frombuffer(chunk, dtype=np.uint8).reshape((FRAME_HEIGHT, FRAME_WIDTH, 3))
            frame_bgr = cv2.cvtColor(frame, cv2.COLOR_RGB2BGR)
            out.write(frame_bgr)

            frame_index += 1
            if frame_index % 10 == 0:
                print(f"[INFO] Encoded {frame_index} frames...")

    out.release()
    print(f"[SUCCESS] Video saved: {video_path}, total frames: {frame_index}")

def decode_video_to_file(video_path, output_file):
    cap = cv2.VideoCapture(video_path)
    if not cap.isOpened():
        print("[ERROR] Cannot open video.")
        return

    all_bytes = bytearray()
    total_frames = int(cap.get(cv2.CAP_PROP_FRAME_COUNT))
    print(f"[INFO] Total frames: {total_frames}")

    for _ in tqdm(range(total_frames), desc="Decoding frames"):
        ret, frame = cap.read()
        if not ret:
            break
        rgb_frame = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
        all_bytes.extend(rgb_frame.flatten())

    cap.release()

    # Recover original file size from first 8 bytes
    original_size = int.from_bytes(all_bytes[:8], "big")
    file_data = all_bytes[8:8 + original_size]

    with open(output_file, "wb") as f:
        f.write(file_data)

    print(f"[SUCCESS] File restored: {output_file}, size: {len(file_data)} bytes")

# MAIN
if __name__ == "__main__":
    print("=== LOSSLESS FILE <-> VIDEO ENCODER/DECODER ===")
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
