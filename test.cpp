// main.cpp
// High-performance CRYPTOGRAPHER encoder (file -> lossless video via ffmpeg).
// Build with: clang++ -std=c++17 -O3 -march=armv8-a -flto -o cryptographer main.cpp
// On macOS for Apple M2 you can use: clang++ -std=c++17 -O3 -march=arm64 -mcpu=apple-m2 -flto -o cryptographer main.cpp

#include <atomic>
#include <cassert>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <mutex>
#include <queue>
#include <string>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>
#include <vector>

using u8 = unsigned char;
using Buffer = std::vector<u8>;

// ---------------- CONFIG ----------------
static const int FRAME_WIDTH  = 1920;
static const int FRAME_HEIGHT = 1080;
static const int PACK_PER_PIXEL = 3; // rgb24
static const size_t FRAME_CAPACITY = (size_t)FRAME_WIDTH * FRAME_HEIGHT * PACK_PER_PIXEL; // bytes per frame
static const int FPS = 30;
static const char * FFMPEG_CODEC = "ffv1"; // lossless
static const size_t QUEUE_MAX_FRAMES = 64; // tuned: number of in-flight frames (increase if you have lots of RAM)
// ----------------------------------------

static inline long long get_file_size(const char* path) {
    struct stat st;
    if (stat(path, &st) != 0) return -1;
    return (long long)st.st_size;
}

// Thread-safe bounded queue of Buffers (frames)
class FrameQueue {
    std::deque<Buffer> q;
    std::mutex m;
    std::condition_variable cv_not_empty;
    std::condition_variable cv_not_full;
    bool closed = false;
    size_t max_frames;
public:
    FrameQueue(size_t max_frames_) : max_frames(max_frames_) {}
    // push frame (moves buffer)
    bool push(Buffer &&buf) {
        std::unique_lock<std::mutex> lk(m);
        cv_not_full.wait(lk, [&]{ return q.size() < max_frames || closed; });
        if (closed) return false;
        q.emplace_back(std::move(buf));
        cv_not_empty.notify_one();
        return true;
    }
    // pop frame (moves out)
    bool pop(Buffer &out) {
        std::unique_lock<std::mutex> lk(m);
        cv_not_empty.wait(lk, [&]{ return !q.empty() || closed; });
        if (q.empty()) return false;
        out = std::move(q.front());
        q.pop_front();
        cv_not_full.notify_one();
        return true;
    }
    void close() {
        std::unique_lock<std::mutex> lk(m);
        closed = true;
        cv_not_empty.notify_all();
        cv_not_full.notify_all();
    }
};

// write all bytes using write() loop
static ssize_t write_all(int fd, const void *buf, size_t size) {
    const u8 *p = (const u8*)buf;
    size_t remaining = size;
    while (remaining > 0) {
        ssize_t w = ::write(fd, p, remaining);
        if (w < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        remaining -= (size_t)w;
        p += w;
    }
    return (ssize_t)size;
}

// Starts ffmpeg process for writing rawvideo stdin pipeline. Returns FILE* (writable) or nullptr.
static FILE* start_ffmpeg_writer(const std::string &video_path) {
    std::string cmd = "ffmpeg -y -f rawvideo -pix_fmt rgb24 -s " +
                      std::to_string(FRAME_WIDTH) + "x" + std::to_string(FRAME_HEIGHT) +
                      " -r " + std::to_string(FPS) +
                      " -i - -c:v " + FFMPEG_CODEC + " -preset ultrafast \"" + video_path + "\"";
    // popen with "w": write to stdin of ffmpeg
    FILE* fp = popen(cmd.c_str(), "w");
    return fp;
}

// Starts ffmpeg reader (not used for this encode-focused fast pipeline, provided as stub)
static FILE* start_ffmpeg_reader(const std::string &video_path) {
    std::string cmd = "ffmpeg -i \"" + video_path + "\" -f rawvideo -pix_fmt rgb24 -";
    FILE* fp = popen(cmd.c_str(), "r");
    return fp;
}

int main(int argc, char** argv) {
    if (argc < 4) {
        std::cerr << "Usage:\n  " << argv[0] << " encode <input_file> <output_video>\n  " << argv[0] << " decode <input_video> <output_file>\n";
        return 1;
    }

    std::string mode = argv[1];
    std::string input = argv[2];
    std::string output = argv[3];

    if (mode == "encode") {
        long long file_size = get_file_size(input.c_str());
        if (file_size < 0) { std::cerr << "Cannot stat input file\n"; return 1; }
        std::cout << "[INFO] Input size: " << file_size << " bytes\n";

        FrameQueue queue(QUEUE_MAX_FRAMES);
        std::atomic<size_t> frames_produced{0};
        std::atomic<size_t> frames_consumed{0};
        std::atomic<bool> reader_done{false};

        // Writer thread: pops frames and writes to ffmpeg stdin
        FILE* ffmpeg_fp = start_ffmpeg_writer(output);
        if (!ffmpeg_fp) { std::cerr << "[ERROR] Failed to start ffmpeg\n"; return 1; }
        int ff_fd = fileno(ffmpeg_fp);

        std::thread writer([&](){
            Buffer buf;
            while (true) {
                bool ok = queue.pop(buf);
                if (!ok) break; // queue closed and empty
                // write buffer to ffmpeg
                ssize_t wrote = write_all(ff_fd, buf.data(), buf.size());
                if (wrote < 0) {
                    std::perror("[ERROR] write_all");
                    // try to continue; but break to be safe
                    break;
                }
                frames_consumed.fetch_add(1, std::memory_order_relaxed);
            }
            // close stdin to ffmpeg
            fflush(ffmpeg_fp);
            pclose(ffmpeg_fp);
        });

        // Reader thread: read file and produce frames
        std::thread reader([&](){
            FILE* f = fopen(input.c_str(), "rb");
            if (!f) { std::perror("[ERROR] fopen"); queue.close(); reader_done = true; return; }

            // We'll allocate a temporary aligned buffer for reading
            const size_t first_capacity = FRAME_CAPACITY - 8; // reserve 8 bytes for file size
            // Keep reusing the same buffer for each frame (move into queue)
            while (true) {
                // For first frame, read first_capacity bytes and prepend file_size
                Buffer frame;
                frame.resize(FRAME_CAPACITY);
                size_t read_here = 0;
                if (frames_produced.load() == 0) {
                    // read first_capacity
                    read_here = fread(frame.data() + 8, 1, first_capacity, f);
                    // write 8-byte big-endian size
                    unsigned long long fs = (unsigned long long)file_size;
                    for (int i = 7; i >= 0; --i) {
                        frame[7 - i] = (u8)((fs >> (i * 8)) & 0xFF);
                    }
                    // pad if needed
                    if (read_here < first_capacity) {
                        size_t pad = first_capacity - read_here;
                        memset(frame.data() + 8 + read_here, 0, pad);
                    }
                } else {
                    // subsequent frames: read FRAME_CAPACITY bytes
                    read_here = fread(frame.data(), 1, FRAME_CAPACITY, f);
                    if (read_here == 0) {
                        break; // eof
                    }
                    if (read_here < FRAME_CAPACITY) {
                        memset(frame.data() + read_here, 0, FRAME_CAPACITY - read_here);
                    }
                }

                // push to queue (move)
                bool pushed = queue.push(std::move(frame));
                if (!pushed) break;
                frames_produced.fetch_add(1, std::memory_order_relaxed);

                if ((frames_produced.load() % 10) == 0) {
                    std::cout << "[INFO] Produced frames: " << frames_produced.load() << "\n";
                }
            }

            fclose(f);
            // done producing
            queue.close();
            reader_done = true;
        });

        // Main thread: show progress
        while (!reader_done.load() || frames_consumed.load() < frames_produced.load()) {
            size_t prod = frames_produced.load();
            size_t cons = frames_consumed.load();
            std::cout << "\r[PROGRESS] produced: " << prod << "  written: " << cons << std::flush;
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
        std::cout << "\n[INFO] All done. total frames produced: " << frames_produced.load() << "\n";

        reader.join();
        writer.join();

        std::cout << "[SUCCESS] Video saved: " << output << "\n";
        return 0;
    }
    else if (mode == "decode") {
        // decode: read raw frames from ffmpeg stdout, stream them to memory file writing
        // We'll shell out ffmpeg -i input -f rawvideo -pix_fmt rgb24 -
        FILE* ff = start_ffmpeg_reader(input);
        if (!ff) { std::cerr << "[ERROR] Failed to start ffmpeg reader\n"; return 1; }
        int fd = fileno(ff);

        // read into vector in chunks
        Buffer all; // WARNING: for multi-GB files this will be large; for decode we can stream to file directly.
        // We'll stream to a temporary output file to avoid huge RAM usage.
        std::string tmpPath = output + ".tmp";
        FILE* out = fopen(tmpPath.c_str(), "wb");
        if (!out) { std::perror("[ERROR] fopen out"); pclose(ff); return 1; }

        Buffer buf;
        buf.resize(FRAME_CAPACITY);
        size_t frames = 0;
        std::cout << "[INFO] Decoding frames...\n";
        while (true) {
            size_t got = fread(buf.data(), 1, FRAME_CAPACITY, ff);
            if (got == 0) break;
            fwrite(buf.data(), 1, got, out);
            frames++;
            if ((frames % 10) == 0) std::cout << "[INFO] Decoded frames: " << frames << "\n";
        }
        fflush(out);
        fclose(out);
        pclose(ff);

        // Now open tmp, extract original size and write exact bytes to final file
        FILE* tmp = fopen(tmpPath.c_str(), "rb");
        if (!tmp) { std::perror("[ERROR] fopen tmp"); return 1; }
        // read first 8 bytes
        unsigned char size_bytes[8];
        if (fread(size_bytes, 1, 8, tmp) != 8) { std::cerr << "[ERROR] cannot read size header\n"; fclose(tmp); return 1; }
        unsigned long long orig_size = 0;
        for (int i = 0; i < 8; ++i) orig_size = (orig_size << 8) | size_bytes[i];

        // copy next orig_size bytes to output
        FILE* final_out = fopen(output.c_str(), "wb");
        if (!final_out) { std::perror("[ERROR] fopen final_out"); fclose(tmp); return 1; }

        const size_t COPY_BUF = 64 * 1024 * 1024; // 64MB buffer
        Buffer copybuf;
        copybuf.resize(COPY_BUF);
        size_t remaining = (size_t)orig_size;
        // Seek to byte 8 in tmp and then read
        fseek(tmp, 8, SEEK_SET);
        while (remaining > 0) {
            size_t toread = remaining < COPY_BUF ? remaining : COPY_BUF;
            size_t r = fread(copybuf.data(), 1, toread, tmp);
            if (r == 0) break;
            fwrite(copybuf.data(), 1, r, final_out);
            remaining -= r;
        }
        fclose(tmp);
        fclose(final_out);
        // remove tmp
        remove(tmpPath.c_str());
        std::cout << "[SUCCESS] Restored file: " << output << ", bytes: " << orig_size << "\n";
        return 0;
    }
    else {
        std::cerr << "Unknown mode\n";
        return 1;
    }
}

