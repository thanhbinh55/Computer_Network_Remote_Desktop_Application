#include "AppManager.hpp"

static std::string to_lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c){ return std::tolower(c); });
    return s;
}

json AppManager::list_apps() {
    // Dùng map để gom nhóm: Key là tên file (exe), Value là Json Object của nhóm đó
    std::map<std::string, json> app_map; 

    DIR* proc_dir = opendir("/proc");
    if (!proc_dir) {
        return {
            {"status", "error"},
            {"module", get_module_name()},
            {"command", "LIST"},
            {"error", "Could not open /proc directory"}
        };
    }

    struct dirent* entry;
    while ((entry = readdir(proc_dir)) != NULL) {
        
        // 1. Kiểm tra xem có phải thư mục PID không (chỉ chứa số)
        if (entry->d_type != DT_DIR) continue;
        
        std::string pid_str = entry->d_name;
        
        // Kiểm tra nhanh xem tên thư mục có phải toàn số không
        if (!std::all_of(pid_str.begin(), pid_str.end(), ::isdigit)) {
            continue; 
        }

        unsigned long pid = 0;
        unsigned long threads = 0;
        std::string name;

        try {
            pid = std::stoul(pid_str);
        } catch (...) { continue; }

        // 2. Lấy tên tiến trình (exe)
        std::ifstream comm_file("/proc/" + pid_str + "/comm");
        if (comm_file.is_open()) {
            std::getline(comm_file, name);
            // [QUAN TRỌNG] File comm trong Linux thường có ký tự xuống dòng ở cuối, cần xóa đi
            if (!name.empty() && name.back() == '\n') {
                name.pop_back();
            }
        } else {
            continue; 
        }

        // 3. Lấy số lượng luồng (threads)
        std::ifstream status_file("/proc/" + pid_str + "/status");
        std::string line;
        if (status_file.is_open()) {
            while (std::getline(status_file, line)) {
                if (line.rfind("Threads:", 0) == 0) {
                    std::stringstream ss(line);
                    std::string key; 
                    ss >> key >> threads; 
                    break;
                }
            }
        }

        // 4. Logic Gom Nhóm (Grouping)
        // Nếu tên app chưa có trong map, tạo mới cấu trúc
        if (app_map.find(name) == app_map.end()) {
            app_map[name] = {
                {"exe", name},
                {"count", 0},
                {"processes", json::array()}
            };
        }

        // Cập nhật thông tin vào nhóm
        app_map[name]["count"] = app_map[name]["count"].get<int>() + 1;
        
        // Thêm chi tiết PID vào mảng processes
        app_map[name]["processes"].push_back({
            {"pid", pid},
            {"threads", threads}
        });
    }

    closedir(proc_dir);

    // 5. Chuyển từ Map sang Array kết quả cuối cùng
    json app_list = json::array();
    for (auto const& [key, val] : app_map) {
        app_list.push_back(val);
    }

    return {
        {"status", "success"},
        {"module", get_module_name()},
        {"command", "LIST"},
        {"apps", app_list}
    }; 
}

std::string get_active_user_app() { // get user's name
// 1. Kiểm tra xem có đang chạy qua sudo không?
    const char* sudo_user = std::getenv("SUDO_USER");
    if (sudo_user != nullptr) {
        return std::string(sudo_user);
    }

    // 2. Thử lấy login name từ phiên làm việc
    const char* login_user = getlogin();
    // getlogin() có thể trả về "root" nếu đang su, nên cần kiểm tra
    if (login_user != nullptr && strcmp(login_user, "root") != 0) {
        return std::string(login_user);
    }

    // 3. Fallback: Lấy user mặc định của Ubuntu (UID 1000)
    // Hầu hết máy cá nhân, user chính luôn là 1000
    struct passwd* pw = getpwuid(1000);
    if (pw != nullptr) {
        return std::string(pw->pw_name);
    }

    // 4. Bất lực: Trả về root (nhưng trường hợp này GUI app sẽ lỗi)
    return "root";
}

json AppManager::start_app(const std::string& app_name) {
    int pipe_fd[2];
    if (pipe(pipe_fd) == -1) return {{"status", "error"}};

    pid_t pid = fork();

    if (pid == -1) {
        close(pipe_fd[0]); close(pipe_fd[1]);
        return {{"status", "error"}};
    }

    // --- TIẾN TRÌNH CON ---
    if (pid == 0) {
        close(pipe_fd[0]);
        dup2(pipe_fd[1], STDOUT_FILENO);
        close(pipe_fd[1]);

        // 1. LẤY THÔNG TIN USER MỤC TIÊU TỰ ĐỘNG
        std::string target_username = get_active_user_app(); 
        struct passwd *pw = getpwnam(target_username.c_str());

        if (pw == NULL) {
            std::cerr << "Error: User not found!" << std::endl;
            _exit(1);
        }

        // Tự động lấy các thông số từ hệ thống
        uid_t target_uid = pw->pw_uid;   // VD: 1000
        gid_t target_gid = pw->pw_gid;   // VD: 1000
        std::string home_dir = pw->pw_dir; // VD: /home/lam
        std::string shell = pw->pw_shell; // VD: /bin/zsh

        // 2. THIẾT LẬP MÔI TRƯỜNG CHUẨN CHO MỌI APP
        // Dọn sạch môi trường cũ của Root
        clearenv(); 

        // Set lại môi trường của User
        setenv("HOME", home_dir.c_str(), 1);
        setenv("USER", target_username.c_str(), 1);
        setenv("LOGNAME", target_username.c_str(), 1);
        setenv("SHELL", shell.c_str(), 1);
        setenv("PATH", "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin", 1);
        
        // Cấu hình hiển thị (Quan trọng cho GUI)
        setenv("DISPLAY", ":0", 1); 
        
        // Tự động tính đường dẫn Wayland/PulseAudio socket
        std::string xdg_runtime = "/run/user/" + std::to_string(target_uid);
        setenv("XDG_RUNTIME_DIR", xdg_runtime.c_str(), 1);
        
        // Nếu dùng Wayland (Ubuntu 22.04+ mặc định dùng wayland-0)
        setenv("WAYLAND_DISPLAY", "wayland-0", 1);

        // 3. HẠ QUYỀN (QUAN TRỌNG NHẤT)
        // Phải set Group trước, sau đó mới set User
        // initgroups để set thêm các group phụ (như sudo, video, audio...)
        initgroups(target_username.c_str(), target_gid); 
        setgid(target_gid);
        setuid(target_uid);

        // Lúc này tiến trình đã hoàn toàn là User thường
        // Mở bất kỳ app nào cũng được (VLC, Chrome, VSCode...)

        // 4. CHẠY LỆNH
        // Dùng "setsid" để tách app ra khỏi terminal của server (tránh bị đóng khi server tắt)
        std::string command = "setsid " + app_name + " > /dev/null 2>&1 & echo $!";
        
        execl(shell.c_str(), shell.c_str(), "-c", command.c_str(), (char*) NULL);
        
        _exit(127);
    }

    // --- TIẾN TRÌNH CHA ---
    close(pipe_fd[1]);
    int status;
    waitpid(pid, &status, 0);

    // ... (Code đọc PID giữ nguyên) ...
    // Demo nhanh:
    std::array<char, 64> buffer;
    ssize_t bytes = read(pipe_fd[0], buffer.data(), buffer.size()-1);
    close(pipe_fd[0]);
    unsigned long real_pid = 0;
    if(bytes > 0) {
        buffer[bytes] = 0;
        try { real_pid = std::stoul(buffer.data()); } catch(...) {}
    }

    return {
        {"status", "success"},
        {"module", get_module_name()},
        {"command", "START"},
        {"data", {{"name", app_name}, {"pid", real_pid}}}
    };
}

json AppManager::kill_app_by_name(const std::string& keyword) {
    int killed = 0;
    std::vector<std::string> killed_details;
    
    std::string search_term = to_lower(keyword);
    if (search_term.empty()) {
         return {
            {"status", "error"},
            {"module", get_module_name()},
            {"command", "KILL"},
            {"error", "Keyword cannot be empty"}
        };
    }

    DIR* proc_dir = opendir("/proc");
    if (!proc_dir) {
        return {
            {"status", "error"}, 
            {"module", get_module_name()},
            {"command", "KILL"},
            {"error", "Cannot open /proc directory"}
        };
    }

    pid_t my_pid = getpid();

    struct dirent* entry;
    
    while ((entry = readdir(proc_dir)) != NULL) {
        
        if (entry->d_type != DT_DIR) continue;
        if (!std::isdigit(entry->d_name[0])) continue;

        std::string pid_str = entry->d_name;
        int pid = std::stoi(pid_str);

        if (pid == my_pid) continue;

        std::string comm_path = "/proc/" + pid_str + "/comm";
        std::ifstream cmd_file(comm_path);
        std::string exe_name;

        if (cmd_file.is_open()) {
            std::getline(cmd_file, exe_name);
            
            if (!exe_name.empty() && exe_name.back() == '\n') {
                exe_name.pop_back();
            }

            std::string exe_lower = to_lower(exe_name);
            
            if (exe_lower.find(search_term) != std::string::npos) {
                
                if (kill(pid, SIGKILL) == 0) {
                    killed++;
                    killed_details.push_back(exe_name + " (" + pid_str + ")");
                } else {
                }
            }
        }
        cmd_file.close();
    }

    closedir(proc_dir);

    return {
        {"status", "success"},
        {"module", get_module_name()},
        {"command", "KILL"},
        {"data", {
            {"keyword", keyword},
            {"killed_count", killed},
            {"details", killed_details}
        }}
    };
}

json AppManager::handle_command(const json& request) {
    const std::string command = request.value("command", "");
    json payload;
    if (request.contains("payload")) payload = request["payload"];

    if (command == "LIST") return list_apps();
    
    if (command == "KILL") {
        std::string name = payload.value("name", "");
        if (name.empty()) return {
            {"status", "error"}, 
            {"module", get_module_name()},
            {"error", "Missing name"}
        };
        return kill_app_by_name(name);
    }

    if (command == "START") {
        std::string path = payload.value("path", "");
        std::cout << "Path: " << path << "\n";
        if (path.empty()) return {
            {"status", "error"}, 
            {"module", get_module_name()},
            {"error", "Missing path"}};
        return start_app(path);
    }

    return {
        {"status", "error"}, 
        {"module", get_module_name()},
        {"error", "Unknown APP command"}};
}