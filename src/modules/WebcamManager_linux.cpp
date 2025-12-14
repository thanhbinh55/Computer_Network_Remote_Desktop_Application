#include "WebcamManager.hpp"
#include <iostream>
#include <cstdio>
#include <vector>
#include <algorithm> // std::search
#include <cstring>   // strerror
#include <cstdint>   // [FIX] Cần cho uint8_t

void WebcamManager::stop_stream() {
    running_ = false;
    if (stream_thread_.joinable()) {
        stream_thread_.join();
    }
}

void WebcamManager::start_stream(StreamCallback callback) {
    stop_stream();
    running_ = true;

    // VIẾT LOGIC FFMPEG TRONG LAMBDA 
    stream_thread_ = std::thread([this, callback]() {
        const char* cmd = "ffmpeg -f v4l2 -framerate 25 -video_size 640x480 -i /dev/video0 -f image2pipe -vcodec mjpeg -q:v 10 pipe:1 2>/dev/null";
        FILE* pipe = popen(cmd, "r");
        
        if (!pipe) return;

        std::vector<uint8_t> buffer;
        std::vector<uint8_t> read_chunk(4096);
        const std::vector<uint8_t> jpeg_start = {0xFF, 0xD8};
        const std::vector<uint8_t> jpeg_end   = {0xFF, 0xD9};

        while (running_) {
            size_t bytes_read = fread(read_chunk.data(), 1, read_chunk.size(), pipe);
            if (bytes_read <= 0) break;

            buffer.insert(buffer.end(), read_chunk.begin(), read_chunk.begin() + bytes_read);

            while (true) {
                auto start_it = std::search(buffer.begin(), buffer.end(), jpeg_start.begin(), jpeg_start.end());
                if (start_it == buffer.end()) {
                    if (buffer.size() > 65536) buffer.clear();
                    break;
                }

                auto end_it = std::search(start_it, buffer.end(), jpeg_end.begin(), jpeg_end.end());
                if (end_it == buffer.end()) break;

                if (end_it + 2 > buffer.end()) break; // Safety check

                std::vector<uint8_t> jpg_frame(start_it, end_it + 2);
                
                // Gọi callback trực tiếp (không cần biến thành viên callback_)
                callback(jpg_frame);

                buffer.erase(buffer.begin(), end_it + 2);
            }
        }
        pclose(pipe);
    });
}


