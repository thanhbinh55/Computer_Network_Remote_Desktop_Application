#include "InputManager.hpp"

// --- HELPER: X11 Display Management ---
// Mở kết nối đến X Server (Màn hình)
// Để tối ưu, nên lưu biến này trong class thay vì mở/đóng liên tục,
static Display* GetXDisplay() {
    Display* display = XOpenDisplay(NULL);
    if (display == NULL) {
        std::cerr << "[INPUT] Cannot open X Display!\n";
        return NULL;
    }
    return display;
}

// Lấy độ phân giải màn hình thực tế 
static int GetScreenWidth(Display* display) {
    return DisplayWidth(display, DefaultScreen(display));
}
static int GetScreenHeight(Display* display) {
    return DisplayHeight(display, DefaultScreen(display));
}

// Hàm này rất quan trọng vì mã phím (VK_CODE) của Windows KHÁC với Linux
KeySym WinVK2X11(int vk_code) {
    // Mapping cơ bản các phím thông dụng
    // Bạn cần mở rộng danh sách này nếu muốn hỗ trợ full bàn phím
    if (vk_code >= 0x30 && vk_code <= 0x39) return vk_code; // 0-9
    if (vk_code >= 0x41 && vk_code <= 0x5A) return vk_code + 32; // A-Z (Linux dùng chữ thường cho KeySym cơ bản)
    
    switch (vk_code) {
        case 0x08: return XK_BackSpace;
        case 0x09: return XK_Tab;
        case 0x0D: return XK_Return;
        case 0x10: return XK_Shift_L;
        case 0x11: return XK_Control_L;
        case 0x12: return XK_Alt_L;
        case 0x1B: return XK_Escape;
        case 0x20: return XK_space;
        case 0x25: return XK_Left;
        case 0x26: return XK_Up;
        case 0x27: return XK_Right;
        case 0x28: return XK_Down;
        case 0x2E: return XK_Delete;
        // Thêm các phím khác nếu cần...
        default: return 0;
    }
}

// ==================== IMPLEMENTATION ====================

json InputManager::handle_command(const json& request) {
    std::string cmd = request.value("command", "");
    json p = request.value("payload", json::object());

    if (cmd == "MOUSE_MOVE") {
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

// 1. DI CHUYỂN CHUỘT
void InputManager::move_mouse(double x, double y) {
    Display* display = GetXDisplay();
    if (!display) return;

    // Linux X11 dùng toạ độ pixel thực tế (không phải 0-65535)
    int width = GetScreenWidth(display);
    int height = GetScreenHeight(display);

    int targetX = static_cast<int>(x * width);
    int targetY = static_cast<int>(y * height);

    // Di chuyển chuột ảo
    // tham số -1 nghĩa là screen mặc định
    XTestFakeMotionEvent(display, -1, targetX, targetY, CurrentTime);
    
    // Bắt buộc gọi XFlush để lệnh được gửi đi ngay lập tức
    XFlush(display);
    XCloseDisplay(display);
}

// 2. CLICK CHUỘT
void InputManager::mouse_btn(const std::string& btn, const std::string& action) {
    Display* display = GetXDisplay();
    if (!display) return;

    // X11 Mapping: 1=Left, 2=Middle, 3=Right
    unsigned int button = 1;
    if (btn == "left") button = 1;
    else if (btn == "middle") button = 2;
    else if (btn == "right") button = 3;

    bool is_press = (action == "down");

    XTestFakeButtonEvent(display, button, is_press, CurrentTime);
    XFlush(display);
    XCloseDisplay(display);
}

// 3. LĂN CHUỘT
void InputManager::mouse_scroll(int delta) {
    Display* display = GetXDisplay();
    if (!display) return;

    // X11 không có sự kiện "Scroll" riêng, nó coi scroll là nút bấm
    // Button 4: Scroll Up
    // Button 5: Scroll Down
    unsigned int button = (delta > 0) ? 4 : 5;

    // Mô phỏng nhấn xuống rồi nhả ra ngay lập tức để cuộn 1 nấc
    XTestFakeButtonEvent(display, button, True, CurrentTime);
    XTestFakeButtonEvent(display, button, False, CurrentTime);
    
    XFlush(display);
    XCloseDisplay(display);
}

// 4. BÀN PHÍM
void InputManager::key_event(int vk_code, bool is_down) {
    Display* display = GetXDisplay();
    if (!display) return;

    // 1. Chuyển đổi mã phím Windows (VK) sang X11 KeySym
    KeySym keysym = WinVK2X11(vk_code);
    
    // 2. Chuyển đổi KeySym sang KeyCode (Mã phần cứng Linux)
    KeyCode keycode = XKeysymToKeycode(display, keysym);

    if (keycode != 0) {
        XTestFakeKeyEvent(display, keycode, is_down, CurrentTime);
    }

    XFlush(display);
    XCloseDisplay(display);
}
