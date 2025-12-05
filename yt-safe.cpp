#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include <algorithm>

static const size_t FRAME_WIDTH  = 320;
static const size_t FRAME_HEIGHT = 240;
static const size_t FRAME_PIXELS = FRAME_WIDTH * FRAME_HEIGHT;
static const size_t FRAME_SIZE   = FRAME_PIXELS * 3;  // RGB24 bytes
static const size_t HEADER_SIZE  = 16;                // 8-byte index + 8-byte payload size

// Writes uint64_t in little-endian
static void write_u64_le(uint8_t* dst, uint64_t v) {
    for (int i = 0; i < 8; i++) dst[i] = (v >> (i * 8)) & 0xFF;
}

// Reads uint64_t in little-endian
static uint64_t read_u64_le(const uint8_t* src) {
    uint64_t v = 0;
    for (int i = 0; i < 8; i++) v |= (uint64_t)src[i] << (i * 8);
    return v;
}

// -----------------------------------------------------------
// ENCODE: file → frames → piped to FFmpeg → video
// -----------------------------------------------------------
int encode_file(const std::string& in_file, const std::string& out_vid) {
    std::ifstream fin(in_file, std::ios::binary | std::ios::ate);
    if (!fin) {
        std::cerr << "Cannot open input file\n";
        return 1;
    }

    uint64_t file_size = fin.tellg();
    fin.seekg(0);

    uint64_t bytes_left = file_size;
    uint64_t frame_index = 0;

    // Start ffmpeg process
    char cmd[2048];
    snprintf(cmd, sizeof(cmd),
             "ffmpeg -y -f rawvideo -pix_fmt rgb24 -s %zux%zu -r 30 -i - "
             "-c:v ffv1 -preset ultrafast \"%s\"",
             FRAME_WIDTH, FRAME_HEIGHT, out_vid.c_str());

    FILE* pipe = popen(cmd, "w");
    if (!pipe) {
        std::cerr << "FFmpeg pipe failed.\n";
        return 1;
    }

    std::vector<uint8_t> frame(FRAME_SIZE, 0);

    while (bytes_left > 0) {
        size_t payload = std::min<uint64_t>(bytes_left, (uint64_t)(FRAME_SIZE - HEADER_SIZE));

        // Fill header
        write_u64_le(frame.data(), frame_index);
        write_u64_le(frame.data() + 8, payload);

        // Fill payload
        fin.read((char*)frame.data() + HEADER_SIZE, payload);

        // Fill excess pixels with noise to avoid YouTube artifact detection
        for (size_t i = HEADER_SIZE + payload; i < FRAME_SIZE; i++)
            frame[i] = (uint8_t)(rand() % 256);

        fwrite(frame.data(), 1, FRAME_SIZE, pipe);

        bytes_left -= payload;
        frame_index++;
    }

    pclose(pipe);
    return 0;
}

// -----------------------------------------------------------
// DECODE: video → frames → rebuild file
// -----------------------------------------------------------
int decode_video(const std::string& in_vid, const std::string& out_file) {
    // Start ffmpeg reader
    char cmd[2048];
    snprintf(cmd, sizeof(cmd),
             "ffmpeg -i \"%s\" -f rawvideo -pix_fmt rgb24 -",
             in_vid.c_str());

    FILE* pipe = popen(cmd, "r");
    if (!pipe) {
        std::cerr << "FFmpeg pipe failed.\n";
        return 1;
    }

    std::ofstream fout(out_file, std::ios::binary);
    if (!fout) {
        std::cerr << "Cannot open output file.\n";
        return 1;
    }

    std::vector<uint8_t> frame(FRAME_SIZE);

    while (true) {
        size_t n = fread(frame.data(), 1, FRAME_SIZE, pipe);
        if (n < FRAME_SIZE)
            break;

        uint64_t index = read_u64_le(frame.data());
        uint64_t payload = read_u64_le(frame.data() + 8);

        if (payload > FRAME_SIZE - HEADER_SIZE)
            payload = FRAME_SIZE - HEADER_SIZE;

        fout.write((char*)(frame.data() + HEADER_SIZE), payload);
    }

    pclose(pipe);
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

    std::cerr << "Invalid mode.\n";
    return 1;
}

