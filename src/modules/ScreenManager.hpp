// src/modules/ScreenManager.hpp
#pragma once
#include "../interfaces/IRemoteModule.hpp"   // chỉnh lại path nếu khác
#include <string>
#include <vector>

class ScreenManager : public IRemoteModule {
    
private:
    std::string module_name_ = "SCREEN";

    // Chụp màn hình, trả về buffer BMP hoàn chỉnh (giống file .bmp),
    // đồng thời trả width/height (pixel) và thông báo lỗi nếu có.
    static bool capture_screen_bmp(std::vector<uint8_t>& out_bmp,
                                   int& width,
                                   int& height,
                                   std::string& error_msg);

    // Base64 encode tiện gửi qua WebSocket
    static std::string base64_encode(const std::vector<uint8_t>& data);

public:
    ScreenManager() = default;
    ~ScreenManager() override = default;

    // Xử lý lệnh JSON
    json handle_command(const json& request) override;

    // Tên module để Dispatcher phân loại
    const std::string& get_module_name() const override{ return module_name_; };
};
