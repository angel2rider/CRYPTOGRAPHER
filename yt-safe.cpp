// yt-safe.cpp — YouTube-safe file↔video encoder/decoder
// Dynamically chooses optimal frame resolution so output video ≈ input file size.
//
// Build: clang++ -std=c++17 -O3 -o cryptographer yt-safe.cpp
// Usage:
//   Encode: ./cryptographer -e input_file output_video
//   Decode: ./cryptographer -d input_video output_file

#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <cmath>
#include <climits>

static const size_t HEADER_SIZE = 16;       // 16-byte frame header
static const size_t MAX_WIDTH  = 1920;
static const size_t MAX_HEIGHT = 1080;

// -----------------------------------------------------------
// Little-endian read/write helpers
// -----------------------------------------------------------
static void write_u16_le(uint8_t* dst, uint16_t v) {
    dst[0] = (uint8_t)( v        & 0xFF);
    dst[1] = (uint8_t)((v >> 8)  & 0xFF);
}

static uint16_t read_u16_le(const uint8_t* src) {
    return (uint16_t)src[0] | ((uint16_t)src[1] << 8);
}

static void write_u32_le(uint8_t* dst, uint32_t v) {
    for (int i = 0; i < 4; i++)
        dst[i] = (uint8_t)((v >> (i * 8)) & 0xFF);
}

static uint32_t read_u32_le(const uint8_t* src) {
    uint32_t v = 0;
    for (int i = 0; i < 4; i++)
        v |= (uint32_t)src[i] << (i * 8);
    return v;
}

static void write_u64_le(uint8_t* dst, uint64_t v) {
    for (int i = 0; i < 8; i++)
        dst[i] = (uint8_t)((v >> (i * 8)) & 0xFF);
}

static uint64_t read_u64_le(const uint8_t* src) {
    uint64_t v = 0;
    for (int i = 0; i < 8; i++)
        v |= (uint64_t)src[i] << (i * 8);
    return v;
}

// -----------------------------------------------------------
// Frame header layout (16 bytes per frame):
//   [0-1]   width        (uint16 LE)
//   [2-3]   height       (uint16 LE)
//   [4-7]   frame_index  (uint32 LE)
//   [8-15]  payload_size (uint64 LE)
// -----------------------------------------------------------

// Calculate optimal frame dimensions for a given file size.
// Returns width, height, and number of frames needed.
static void calc_dimensions(uint64_t file_size,
                            uint16_t &out_width,
                            uint16_t &out_height,
                            uint64_t &out_frames)
{
    if (file_size == 0) {
        out_width = 1; out_height = 1; out_frames = 1;
        return;
    }

    // Minimum pixels needed for a single frame:
    //   w * h * 3 >= file_size + HEADER_SIZE
    //   w * h >= ceil((file_size + HEADER_SIZE) / 3)
    uint64_t min_pixels = (file_size + HEADER_SIZE + 2) / 3;

    // If it fits in one 1920×1080 frame, find optimal dimensions
    if (min_pixels <= (uint64_t)MAX_WIDTH * MAX_HEIGHT) {
        out_frames = 1;

        uint64_t best_w = 1, best_h = 1, best_waste = UINT64_MAX;
        uint64_t max_w = std::min<uint64_t>(MAX_WIDTH, min_pixels);

        for (uint64_t w = 1; w <= max_w; w++) {
            uint64_t h = (min_pixels + w - 1) / w;  // ceil division
            if (h > MAX_HEIGHT) continue;
            uint64_t waste = w * h - min_pixels;
            if (waste < best_waste) {
                best_waste = waste;
                best_w = w;
                best_h = h;
            }
        }

        out_width  = (uint16_t)best_w;
        out_height = (uint16_t)best_h;
        return;
    }

    // Large file: use maximum resolution across multiple frames
    out_width  = MAX_WIDTH;
    out_height = MAX_HEIGHT;

    uint64_t cap_per_frame = (uint64_t)MAX_WIDTH * MAX_HEIGHT * 3 - HEADER_SIZE;
    out_frames = (file_size + cap_per_frame - 1) / cap_per_frame;
}

// -----------------------------------------------------------
// ENCODE: file → video
// -----------------------------------------------------------
static int encode_file(const std::string& in_file, const std::string& out_vid) {
    std::ifstream fin(in_file, std::ios::binary | std::ios::ate);
    if (!fin) {
        std::cerr << "Cannot open input file\n";
        return 1;
    }

    uint64_t file_size = (uint64_t)fin.tellg();
    fin.seekg(0);

    // Calculate optimal dimensions
    uint16_t width, height;
    uint64_t frames_needed;
    calc_dimensions(file_size, width, height, frames_needed);

    size_t frame_size = (size_t)width * height * 3;
    size_t payload_capacity = frame_size - HEADER_SIZE;

    std::cout << "[INFO] File size: " << file_size << " bytes\n";
    std::cout << "[INFO] Resolution: " << width << "x" << height
              << " (" << frame_size << " bytes per frame)\n";
    std::cout << "[INFO] Frames needed: " << frames_needed << "\n";

    // Start FFmpeg process with dynamic resolution
    char cmd[2048];
    int n = snprintf(cmd, sizeof(cmd),
        "ffmpeg -y -f rawvideo -pix_fmt rgb24 -s %ux%u -r 30 -i - "
        "-c:v ffv1 \"%s\"",
        (unsigned)width, (unsigned)height, out_vid.c_str());
    if (n < 0 || (size_t)n >= sizeof(cmd)) {
        std::cerr << "FFmpeg command too long\n";
        return 1;
    }

    FILE* pipe = popen(cmd, "w");
    if (!pipe) {
        std::cerr << "FFmpeg pipe failed\n";
        return 1;
    }

    uint64_t bytes_left = file_size;
    uint64_t frame_index = 0;
    std::vector<uint8_t> frame(frame_size, 0);

    while (bytes_left > 0) {
        size_t payload = (size_t)std::min<uint64_t>(bytes_left, (uint64_t)payload_capacity);

        // Fill header
        write_u16_le(frame.data(),      width);
        write_u16_le(frame.data() + 2,  height);
        write_u32_le(frame.data() + 4,  (uint32_t)frame_index);
        write_u64_le(frame.data() + 8,  (uint64_t)payload);

        // Fill payload with file data
        bool ok = (bool)fin.read((char*)frame.data() + HEADER_SIZE, payload);
        if (!ok && payload > 0) {
            std::cerr << "Read error at frame " << frame_index << "\n";
            pclose(pipe);
            return 1;
        }

        // Fill excess bytes with noise (YouTube-safe)
        size_t noise_start = HEADER_SIZE + payload;
        for (size_t i = noise_start; i < frame_size; i++)
            frame[i] = (uint8_t)(rand() % 256);

        // Write frame to FFmpeg
        size_t written = fwrite(frame.data(), 1, frame_size, pipe);
        if (written != frame_size) {
            std::cerr << "Write error at frame " << frame_index << "\n";
            pclose(pipe);
            return 1;
        }

        bytes_left -= payload;
        frame_index++;

        if (frame_index % 100 == 0)
            std::cout << "[INFO] Encoded " << frame_index << " frames...\n";
    }

    pclose(pipe);
    std::cout << "[SUCCESS] Video saved: " << out_vid
              << ", frames: " << frame_index << "\n";
    return 0;
}

// -----------------------------------------------------------
// DECODE: video → file
// -----------------------------------------------------------
static int decode_video(const std::string& in_vid, const std::string& out_file) {
    // Use ffprobe to get video dimensions
    char probe_cmd[2048];
    int n = snprintf(probe_cmd, sizeof(probe_cmd),
        "ffprobe -v error -select_streams v:0 -show_entries stream=width,height "
        "-of csv=p=0 \"%s\"", in_vid.c_str());
    if (n < 0 || (size_t)n >= sizeof(probe_cmd)) {
        std::cerr << "FFprobe command too long\n";
        return 1;
    }

    FILE* probe_pipe = popen(probe_cmd, "r");
    if (!probe_pipe) {
        std::cerr << "FFprobe pipe failed\n";
        return 1;
    }

    unsigned w = 0, h = 0;
    if (fscanf(probe_pipe, "%u,%u", &w, &h) != 2 || w == 0 || h == 0) {
        std::cerr << "Failed to parse video dimensions from ffprobe\n";
        pclose(probe_pipe);
        return 1;
    }
    pclose(probe_pipe);

    size_t frame_size = (size_t)w * h * 3;
    std::cout << "[INFO] Video dimensions: " << w << "x" << h
              << " (" << frame_size << " bytes per frame)\n";

    // Start FFmpeg reader
    char read_cmd[2048];
    n = snprintf(read_cmd, sizeof(read_cmd),
        "ffmpeg -i \"%s\" -f rawvideo -pix_fmt rgb24 -", in_vid.c_str());
    if (n < 0 || (size_t)n >= sizeof(read_cmd)) {
        std::cerr << "FFmpeg command too long\n";
        return 1;
    }

    FILE* pipe = popen(read_cmd, "r");
    if (!pipe) {
        std::cerr << "FFmpeg pipe failed\n";
        return 1;
    }

    std::ofstream fout(out_file, std::ios::binary);
    if (!fout) {
        std::cerr << "Cannot open output file\n";
        pclose(pipe);
        return 1;
    }

    std::vector<uint8_t> frame(frame_size);
    uint64_t total_written = 0;
    uint64_t frame_index = 0;

    while (true) {
        size_t n_read = fread(frame.data(), 1, frame_size, pipe);
        if (n_read < frame_size) {
            if (n_read > 0) {
                std::cerr << "Warning: truncated frame " << frame_index
                          << " (got " << n_read << " bytes, expected "
                          << frame_size << ")\n";
            }
            break;
        }

        uint64_t payload = read_u64_le(frame.data() + 8);
        if (payload > frame_size - HEADER_SIZE) {
            std::cerr << "Warning: invalid payload size " << payload
                      << " at frame " << frame_index << ", clamping\n";
            payload = frame_size - HEADER_SIZE;
        }

        fout.write((char*)(frame.data() + HEADER_SIZE), (std::streamsize)payload);
        total_written += payload;
        frame_index++;

        if (frame_index % 100 == 0)
            std::cout << "[INFO] Decoded " << frame_index << " frames...\n";
    }

    pclose(pipe);
    fout.close();

    std::cout << "[SUCCESS] File restored: " << out_file
              << ", size: " << total_written << " bytes, frames: " << frame_index << "\n";
    return 0;
}

// -----------------------------------------------------------
// MAIN
// -----------------------------------------------------------
int main(int argc, char** argv) {
    if (argc < 4) {
        std::cout << "Usage:\n";
        std::cout << "  Encode: " << argv[0] << " -e input_file output_video\n";
        std::cout << "  Decode: " << argv[0] << " -d input_video output_file\n";
        return 0;
    }

    std::string mode = argv[1];
    if (mode == "-e") {
        return encode_file(argv[2], argv[3]);
    } else if (mode == "-d") {
        return decode_video(argv[2], argv[3]);
    }

    std::cerr << "Invalid mode. Use -e for encode, -d for decode.\n";
    return 1;
}
