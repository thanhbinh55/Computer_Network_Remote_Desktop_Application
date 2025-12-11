#include "InputManager.hpp"
#include <iostream>

// Helper lấy độ phân giải (chỉ dùng để tham khảo debug nếu cần, không dùng để tính toán toạ độ)
static int GetScreenWidth() { return GetSystemMetrics(SM_CXSCREEN); }
static int GetScreenHeight() { return GetSystemMetrics(SM_CYSCREEN); }

json InputManager::handle_command(const json& request) {
    std::string cmd = request.value("command", "");
    json p = request.value("payload", json::object());

    if (cmd == "MOUSE_MOVE") {
        // Nhận toạ độ tỉ lệ từ 0.0 đến 1.0 từ Client
        move_mouse(p.value("x", 0.0), p.value("y", 0.0));
    }
    else if (cmd == "MOUSE_BTN") {
        mouse_btn(p.value("btn", "left"), p.value("type", "down"));
    }
    else if (cmd == "MOUSE_WHEEL") {
        mouse_scroll(p.value("delta", 0));
    }
    else if (cmd == "KEY_EVENT") {
        bool is_down = (p.value("type", "up") == "down");
        int key = p.value("key", 0);
        key_event(key, is_down);
    }
    return {{"status", "ok"}};
}

// 1. DI CHUYỂN CHUỘT (SỬA LẠI ĐỂ KHẮC PHỤC LỆCH DPI)
void InputManager::move_mouse(double x, double y) {
    // Windows SendInput sử dụng hệ toạ độ tuyệt đối từ 0 đến 65535 cho toàn bộ màn hình
    // Bất kể độ phân giải là 1920x1080 hay 4K, toạ độ 65535,65535 luôn là góc dưới phải.
    // Việc này giúp tránh sai số do DPI Scaling (Zoom 125%, 150%) của Windows.
    
    int absX = static_cast<int>(x * 65535);
    int absY = static_cast<int>(y * 65535);

    INPUT input = {0};
    input.type = INPUT_MOUSE;
    // MOUSEEVENTF_ABSOLUTE: Chỉ định dùng toạ độ tuyệt đối (0-65535)
    // MOUSEEVENTF_VIRTUALDESK: Hỗ trợ đa màn hình (nếu có)
    input.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK;
    input.mi.dx = absX;
    input.mi.dy = absY;
    
    SendInput(1, &input, sizeof(INPUT));
}

// 2. CLICK CHUỘT
void InputManager::mouse_btn(const std::string& btn, const std::string& action) {
    INPUT input = {0};
    input.type = INPUT_MOUSE;
    
    if (btn == "left") {
        input.mi.dwFlags = (action == "down") ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP;
    } else if (btn == "right") {
        input.mi.dwFlags = (action == "down") ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP;
    } else if (btn == "middle") {
        input.mi.dwFlags = (action == "down") ? MOUSEEVENTF_MIDDLEDOWN : MOUSEEVENTF_MIDDLEUP;
    }
    SendInput(1, &input, sizeof(INPUT));
}

// 3. LĂN CHUỘT
void InputManager::mouse_scroll(int delta) {
    INPUT input = {0};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = MOUSEEVENTF_WHEEL;
    input.mi.mouseData = (DWORD)delta;
    SendInput(1, &input, sizeof(INPUT));
}

// 4. BÀN PHÍM
void InputManager::key_event(int vk_code, bool is_down) {
    INPUT input = {0};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = (WORD)vk_code;
    
    if (!is_down) {
        input.ki.dwFlags = KEYEVENTF_KEYUP;
    }
    SendInput(1, &input, sizeof(INPUT));
}
