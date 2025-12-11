#include "AppManager.hpp"
#include <unordered_map>
#include <algorithm> // cho std::transform
#include <cctype>    // cho ::tolower
#include <vector>

#if _WIN32
#include "AppManager.hpp"
#include <unordered_map>

// Helper: Chuyển chuỗi về chữ thường
static std::string to_lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c){ return std::tolower(c); });
    return s;
}

// Helper: WCHAR -> UTF-8
static std::string to_utf8(const std::wstring& wide_str) {
    if (wide_str.empty()) return {};
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, wide_str.c_str(),
                                          static_cast<int>(wide_str.size()),
                                          nullptr, 0, nullptr, nullptr);
    std::string out(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, wide_str.c_str(),
                        static_cast<int>(wide_str.size()),
                        out.data(), size_needed, nullptr, nullptr);
    return out;
}

// ========== 1. LIST_APPS ==========
json AppManager::list_apps() {
    json apps = json::array();
    std::unordered_map<std::string, json> map;

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return {{"status","error"}, {"message","CreateToolhelp32Snapshot failed"}};
    }

    PROCESSENTRY32W pe32{};
    pe32.dwSize = sizeof(pe32);

    if (Process32FirstW(snapshot, &pe32)) {
        do {
            std::string exe = to_utf8(std::wstring(pe32.szExeFile));
            auto& app_entry = map[exe];
            
            if (app_entry.is_null()) {
                app_entry["exe"] = exe;
                app_entry["count"] = 0;
                app_entry["processes"] = json::array();
            }
            app_entry["count"] = app_entry["count"].get<int>() + 1;
            
            json p;
            p["pid"] = static_cast<unsigned long>(pe32.th32ProcessID);
            p["threads"] = static_cast<unsigned long>(pe32.cntThreads);
            app_entry["processes"].push_back(p);

        } while (Process32NextW(snapshot, &pe32));
    }
    CloseHandle(snapshot);

    for (auto& kv : map) {
        apps.push_back(kv.second);
    }

    return {
        {"status","success"},
        {"module", get_module_name()},
        {"command","LIST"},
        {"apps", apps}
    };
}

// ========== 2. KILL_APP_BY_NAME ==========
json AppManager::kill_app_by_name(const std::string& keyword) {
    int killed = 0;
    std::vector<std::string> killed_details;
    std::string search_term = to_lower(keyword);

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return {{"status", "error"}};

    PROCESSENTRY32W pe32{};
    pe32.dwSize = sizeof(pe32);

    if (Process32FirstW(snapshot, &pe32)) {
        do {
            std::string exe_name = to_utf8(std::wstring(pe32.szExeFile));
            std::string exe_lower = to_lower(exe_name);

            if (exe_lower.find(search_term) != std::string::npos) {
                HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pe32.th32ProcessID);
                if (hProcess) {
                    if (TerminateProcess(hProcess, 1)) {
                        killed++;
                        killed_details.push_back(exe_name + " (" + std::to_string(pe32.th32ProcessID) + ")");
                    }
                    CloseHandle(hProcess);
                }
            }
        } while (Process32NextW(snapshot, &pe32));
    }
    CloseHandle(snapshot);

    return {
        {"status", "success"},
        {"command", "KILL"},
        {"data", {
            {"keyword", keyword},
            {"killed_count", killed},
            {"details", killed_details}
        }}
    };
}

// ========== 3. START_APP (Phần bị thiếu gây lỗi) ==========
json AppManager::start_app(const std::string& path) {
    STARTUPINFOA si{};
    PROCESS_INFORMATION pi{};
    si.cb = sizeof(si);
    std::string cmd_line = path;

    if (!CreateProcessA(nullptr, &cmd_line[0], nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
        return {
            {"status", "error"},
            {"message", "Failed to start app"},
            {"last_error", (int)GetLastError()}
        };
    }
    
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return {
        {"status", "success"},
        {"command", "START"},
        {"pid", (unsigned long)pi.dwProcessId},
        {"path", path}
    };
}

// ========== 4. DISPATCHER ==========
json AppManager::handle_command(const json& request) {
    const std::string command = request.value("command", "");
    json payload;
    if (request.contains("payload")) payload = request["payload"];

    if (command == "LIST") return list_apps();
    
    if (command == "KILL") {
        std::string name = payload.value("name", "");
        if (name.empty()) return {{"status", "error"}, {"message", "Missing name"}};
        return kill_app_by_name(name);
    }

    if (command == "START") {
        std::string path = payload.value("path", "");
        if (path.empty()) return {{"status", "error"}, {"message", "Missing path"}};
        return start_app(path);
    }

    return {{"status", "error"}, {"message", "Unknown APP command"}};
}
#endif
