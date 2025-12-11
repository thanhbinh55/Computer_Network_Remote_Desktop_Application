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

bool ProcessManager::start_process(const std::string& path, int& out_pid) {
    int pipe_fd[2];
    if (pipe(pipe_fd) == -1) 
        return false;

    pid_t pid = fork();

    if (pid == -1) {
        close(pipe_fd[0]);
        close(pipe_fd[1]);
        return false;
    }

    if (pid == 0) {
        // --- TIẾN TRÌNH CON ---
        
        // 1. Chuyển stdout sang đầu 'write' của pipe
        close(pipe_fd[0]); // Con không đọc, đóng đầu đọc
        dup2(pipe_fd[1], STDOUT_FILENO); // Chuyển hướng stdout
        close(pipe_fd[1]); // Đóng fd gốc

        // 2. Chạy lệnh wrapper
        // Ví dụ: "alacritty & echo $!"
        std::string command = path + " & echo $!";
        
        execl("/bin/sh", "sh", "-c", command.c_str(), (char*) NULL);
        
        // Nếu execl thất bại
        _exit(errno); 
    }

    // --- TIẾN TRÌNH CHA ---
    close(pipe_fd[1]); // Cha không viết, đóng đầu viết

    int status;
    waitpid(pid, &status, 0); 

    std::array<char, 32> buffer;
    std::string real_pid_str;
    ssize_t bytes_read = read(pipe_fd[0], buffer.data(), buffer.size() - 1);
    
    close(pipe_fd[0]); // Đọc xong, đóng đầu đọc

    unsigned long real_pid = 0;

    if (bytes_read > 0) {
        buffer[bytes_read] = '\0'; // Đảm bảo kết thúc chuỗi
        real_pid_str = buffer.data();
        
        // Xóa ký tự newline '\n' ở cuối
        real_pid_str.erase(
            std::remove(real_pid_str.begin(), real_pid_str.end(), '\n'), 
            real_pid_str.end()
        );

        try {
            real_pid = std::stoul(real_pid_str);
        } catch (...) {
             return false; 
        }
    } else {
         return false;
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
     
     
     
     
