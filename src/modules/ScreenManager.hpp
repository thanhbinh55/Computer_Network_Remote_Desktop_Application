#pragma once
#include "../interfaces/IRemoteModule.hpp"
#include <vector>
#include <string>

// --- CẤU HÌNH CHO WINDOWS ---
#if defined(_WIN32)
    #include <windows.h>
    #include <objidl.h> 
    #include <gdiplus.h> // Thường cần thêm cái này cho chụp ảnh màn hình Windows

// --- CẤU HÌNH CHO LINUX ---
#elif defined(__linux__)
    #include <X11/Xlib.h>
    #include <X11/Xutil.h>
    #include <jpeglib.h>
    #include <iostream>
    #include <cstring>
    #include <iostream>
    #include <fstream>
    #include <ctime>
    #include <iomanip>
    #include <filesystem>
#endif
class ScreenManager : public IRemoteModule {
public:
    const std::string& get_module_name() const override { 
        static const std::string name = "SCREEN"; return name; 
    }
    
    json handle_command(const json& request) override;

    // Hàm public để Main gọi trực tiếp
    //static bool capture_screen_data(std::vector<uint8_t>& out_buffer, std::string& error_msg);
    static bool capture_screen_data(std::vector<uint8_t>& out_buffer, std::string& error_msg, bool save_to_disk = true); // Mặc định là lưu vào ổ đĩa, còn khi streaming thì không lưu
};
