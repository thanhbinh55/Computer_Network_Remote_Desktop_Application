#pragma once
#include "../interfaces/IRemoteModule.hpp"
#include <functional>
#include <atomic>
#include <vector>
#include <thread>

#if _WIN32
#include <windows.h>
#include <objidl.h> 
#include <gdiplus.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#else
#include <jpeglib.h>
#endif

class WebcamManager : public IRemoteModule {
public:
    using StreamCallback = std::function<void(const std::vector<uint8_t>&)>;

    const std::string& get_module_name() const override {
        static const std::string name = "WEBCAM";
        return name;
    }

    // Hàm xử lý lệnh JSON (Start/Stop từ client)
    json handle_command(const json& request) override {
        // Chúng ta xử lý logic stream ở Main thông qua hàm start_stream bên dưới
        // Hàm này chỉ để giữ đúng interface
        return {{"status", "ok"}}; 
    }

    // Hàm bắt đầu luồng video
    void start_stream(StreamCallback callback);
    
    // Hàm dừng luồng
    void stop_stream();

private:
    std::atomic<bool> running_{false};
    std::thread stream_thread_;
};
