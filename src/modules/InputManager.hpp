#pragma once
#include "../interfaces/IRemoteModule.hpp"

// --- PHÂN CHIA HỆ ĐIỀU HÀNH ---
#if defined(_WIN32)
    #include <windows.h>
#elif defined(__linux__)
    #include <X11/Xlib.h>
    #include <X11/extensions/XTest.h> // Thư viện giả lập Input trên Linux
    #include <X11/keysym.h>
    #include <iostream>
#endif
// ------------------------------

class InputManager : public IRemoteModule {
public:
    const std::string& get_module_name() const override {
        static const std::string name = "INPUT";
        return name;
    }

    json handle_command(const json& request) override;

private:
    void move_mouse(double x, double y); // Nhận toạ độ tỉ lệ 0.0 -> 1.0
    void mouse_btn(const std::string& btn, const std::string& action); // left/right, down/up
    void mouse_scroll(int delta);
    void key_event(int key_code, bool is_down);
};
