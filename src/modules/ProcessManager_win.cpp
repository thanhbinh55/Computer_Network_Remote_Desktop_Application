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
                {"module", "PROCESS"},
                {"command", "KILL"},
                {"status", "success"},
                {"pid", pid},
                {"message", "Process terminated"}
            };
        } else {
            return {{"status", "error"}, {"message", "Failed to kill process"}};
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
            return {{"status", "error"}, {"message", "Missing 'path' in payload"}};
        }

        int new_pid = 0;
        if (start_process(path, new_pid)) {
            return {
                {"module", "PROCESS"},
                {"command", "START"},
                {"status", "success"},
                {"pid", new_pid},
                {"message", "Process started successfully"}
            };
        } else {
            return {{"status", "error"}, {"message", "Failed to create process. Check path."}};
        }
    }

    return {{"status", "error"}, {"message", "Unknown command"}};
}

// ... (Hàm list_processes giữ nguyên) ...
json ProcessManager::list_processes() {
    json process_list = json::array();
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) return {{"status", "error"}};

    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);

    if (Process32First(hSnapshot, &pe32)) {
        do {
            process_list.push_back({
                {"pid", (int)pe32.th32ProcessID},
                {"name", std::string(pe32.szExeFile)},
                {"threads", (int)pe32.cntThreads}
            });
        } while (Process32Next(hSnapshot, &pe32));
    }
    CloseHandle(hSnapshot);
    return {{"module", "PROCESS"}, {"command", "LIST"}, {"status", "success"}, {"data", {{"process_list", process_list}}}};
}

// ... (Hàm kill_process giữ nguyên) ...
bool ProcessManager::kill_process(int pid) {
    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
    if (!hProcess) return false;
    bool res = TerminateProcess(hProcess, 1);
    CloseHandle(hProcess);
    return res;
}

// [MỚI] Cài đặt hàm Start Process
bool ProcessManager::start_process(const std::string& path, int& out_pid) {
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    // CreateProcess yêu cầu chuỗi command line có thể ghi được (mutable), nên ta copy ra buffer
    std::vector<char> cmdData(path.begin(), path.end());
    cmdData.push_back('\0');

    // API tạo tiến trình
    if (CreateProcessA(
        NULL,           // No module name (use command line)
        cmdData.data(), // Command line
        NULL,           // Process handle not inheritable
        NULL,           // Thread handle not inheritable
        FALSE,          // Set handle inheritance to FALSE
        0,              // No creation flags
        NULL,           // Use parent's environment block
        NULL,           // Use parent's starting directory 
        &si,            // Pointer to STARTUPINFO structure
        &pi             // Pointer to PROCESS_INFORMATION structure
    )) {
        out_pid = (int)pi.dwProcessId;
        
        // Quan trọng: Đóng handle để tránh rò rỉ bộ nhớ (Process vẫn chạy bình thường)
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return true;
    }
    
    return false;
}
