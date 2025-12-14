#include "ProcessManager.hpp"

const std::string& ProcessManager::get_module_name() const {
    static const std::string name = "PROCESS";
    return name;
}

json ProcessManager::handle_command(const json& request) {
    std::string cmd = request.value("command", "");

    // --- 1. LIST ---
    if (cmd == "LIST") {
        return list_processes();
    }
    
    // --- 2. KILL ---
    else if (cmd == "KILL") {
        int pid = 0;
        if (request.contains("payload") && request["payload"].contains("pid")) {
            pid = request["payload"]["pid"].get<int>();
        } else if (request.contains("pid")) {
            pid = request.value("pid", 0);
        }

        if (pid > 0 && kill_process(pid)) {
            return {
                {"status", "success"},
                {"module", get_module_name()},
                {"command", "KILL"},
                {"pid", pid},
                {"data", "Process terminated"}
            };
        } else {
            return {
                {"status", "error"}, 
                {"module", get_module_name()},
                {"error", "Failed to kill process"}};
        }
    }

    // --- 3. START (MỚI) ---
    else if (cmd == "START") {
        std::string path;
        // Lấy đường dẫn từ payload
        if (request.contains("payload") && request["payload"].contains("path")) {
            path = request["payload"]["path"].get<std::string>();
        } 
        // Fallback
        else if (request.contains("path")) {
            path = request.value("path", "");
        }

        if (path.empty()) {
            return {
                {"status", "error"}, 
                {"module", get_module_name()},
                {"error", "Missing 'path' in payload"}};
        }

        int new_pid = 0;
        if (start_process(path, new_pid)) {
            return {
                {"status", "success"},
                {"module", "PROCESS"},
                {"command", "START"},
                {"pid", new_pid},
                {"data", "Process started successfully"}
            };
        } else {
            return {
                {"status", "error"}, 
                {"module", get_module_name()},
                {"error", "Failed to create process. Check path."}
            };
        }
    }

    return {
        {"status", "error"}, 
        {"module", get_module_name()},
        {"error", "Unknown command"}
    };
}

json ProcessManager::list_processes() {
    json process_list = json::array();

    DIR* proc_dir = opendir("/proc");
    if (!proc_dir) {
        return {
            {"status", "error"},
            {"module", get_module_name()},
            {"command", "LIST"},
            {"message", "Could not open /proc directory"}
        };
    }

    struct dirent* entry;
    while ((entry = readdir(proc_dir)) != NULL) {
        
        if (entry->d_type != DT_DIR) {
            continue;
        }

        const char* d_name = entry->d_name;
        bool is_pid = true;
        if (*d_name == '\0') is_pid = false; // Tên rỗng

        while (*d_name) {
            if (!std::isdigit(*d_name)) {
                is_pid = false;
                break;
            }
            d_name++;
        }

        if (is_pid) {
            std::string pid_str(entry->d_name);
            unsigned long pid = 0;
            unsigned long threads = 0;
            std::string name;

            try {
                pid = std::stoul(pid_str);
            } catch (...) {
                continue; // Bỏ qua nếu không phải số hợp lệ
            }

            std::ifstream comm_file("/proc/" + pid_str + "/comm");
            if (comm_file.is_open()) {
                std::getline(comm_file, name);
            } else {
                continue; 
            }

            std::ifstream status_file("/proc/" + pid_str + "/status");
            std::string line;
            if (status_file.is_open()) {
                while (std::getline(status_file, line)) {
                    if (line.rfind("Threads:", 0) == 0) {
                        std::stringstream ss(line);
                        std::string key; // Đọc chữ "Threads:"
                        ss >> key >> threads; // Đọc số lượng luồng
                        break;
                    }
                }
            }
            process_list.push_back({
                {"pid",     pid},
                {"name",    name}, 
                {"threads", threads}
            });
        }
    }

    closedir(proc_dir);

    return {
        {"status", "success"},
        {"module", get_module_name()}, 
        {"command", "LIST"},
        {"data", {{"process_list", process_list}}}
    };
}

std::string get_active_user_process() {
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
    return "root";}

bool ProcessManager::start_process(const std::string& path, int& out_pid) {
    int pipe_fd[2];
    if (pipe(pipe_fd) == -1) return false;

    pid_t pid = fork();

    if (pid == -1) {
        close(pipe_fd[0]); close(pipe_fd[1]);
        return false;
    }

    // --- TIẾN TRÌNH CON ---
    if (pid == 0) {
        close(pipe_fd[0]);
        dup2(pipe_fd[1], STDOUT_FILENO);
        close(pipe_fd[1]);

        // 1. LẤY THÔNG TIN USER MỤC TIÊU TỰ ĐỘNG
        std::string target_username = get_active_user_process(); 
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
        std::string command = "setsid " + path + " > /dev/null 2>&1 & echo $!";
        
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

    out_pid = real_pid;
    return true;
}

bool ProcessManager::kill_process(int pid_ul) {
    pid_t pid = static_cast<pid_t>(pid_ul);

    const pid_t self = getpid();
    if (pid == self) {
        return false;
    }

    // if (kill(pid, 0) == -1) {
    //     return false;
    // }

    if (kill(pid, SIGKILL) == 0) {
        return true;
    }

    return false;
}    
     
     
     
     
