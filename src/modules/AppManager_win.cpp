// src/modules/AppManager.cpp
#include "AppManager.hpp"
#include <unordered_map>

// tiện ích: WCHAR -> UTF-8 (giống ProcessManager, có thể tách ra file chung)
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

// ========== LIST_APPS: gom process theo exe name ==========
json AppManager::list_apps() {
    json apps = json::array();

    std::unordered_map<std::string, json> map; // exe -> info

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return {
            {"status","error"},
            {"module", get_module_name()},
            {"message","CreateToolhelp32Snapshot failed"}
        };
    }

    PROCESSENTRY32W pe32{};
    pe32.dwSize = sizeof(pe32);

    if (Process32FirstW(snapshot, &pe32)) {
        do {
            std::string exe = to_utf8(std::wstring(pe32.szExeFile));

            auto& app_entry = map[exe];
            if (app_entry.is_null()) {
                app_entry["exe"]      = exe;
                app_entry["count"]    = 0;
                app_entry["processes"] = json::array();
            }

            app_entry["count"] = app_entry["count"].get<int>() + 1;

            json p;
            p["pid"]     = static_cast<unsigned long>(pe32.th32ProcessID);
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
        {"command","LIST_APPS"},
        {"apps", apps}
    };
}

// ========== KILL_APP: kill tất cả process có cùng exe name ==========
json AppManager::kill_app_by_name(const std::string& exe_name) {
    int killed = 0;

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return {
            {"status","error"},
            {"module", get_module_name()},
            {"message","CreateToolhelp32Snapshot failed"}
        };
    }

    PROCESSENTRY32W pe32{};
    pe32.dwSize = sizeof(pe32);

    if (Process32FirstW(snapshot, &pe32)) {
        do {
            std::string exe = to_utf8(std::wstring(pe32.szExeFile));

            // so sánh không phân biệt hoa thường
            if (_stricmp(exe.c_str(), exe_name.c_str()) == 0) {
                HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pe32.th32ProcessID);
                if (hProcess) {
                    TerminateProcess(hProcess, 1);
                    CloseHandle(hProcess);
                    ++killed;
                }
            }
        } while (Process32NextW(snapshot, &pe32));
    }

    CloseHandle(snapshot);

    return {
        {"status","success"},
        {"module", get_module_name()},
        {"command","KILL_APP"},
        {"exe", exe_name},
        {"killed", killed}
    };
}

// ========== START_APP: giống start_process ==========
json AppManager::start_app(const std::string& path) {
    STARTUPINFOA si{};
    PROCESS_INFORMATION pi{};
    si.cb = sizeof(si);

    if (!CreateProcessA(
            nullptr,
            const_cast<char*>(path.c_str()),
            nullptr, nullptr, FALSE, 0, nullptr, nullptr,
            &si, &pi))
    {
        const DWORD gle = GetLastError();
        return {
            {"status","error"},
            {"module", get_module_name()},
            {"message","Failed to start app"},
            {"command","START_APP"},
            {"path", path},
            {"last_error", static_cast<int>(gle)}
        };
    }

    const DWORD pid = pi.dwProcessId;
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return {
            {"status","success"},
            {"module", get_module_name()},
            {"command","START_APP"},
            {"pid", static_cast<unsigned long>(pid)},
            {"path", path}
    };
}

// ========== DISPATCH CHÍNH ==========
json AppManager::handle_command(const json& request) {
    const std::string command = request.value("command", "");

    if (command == "LIST_APPS") {
        return list_apps();
    }

    if (command == "KILL_APP") {
        const std::string exe_name = request.value("exe", "");
        if (exe_name.empty()) {
            return {
                {"status","error"},
                {"module", get_module_name()},
                {"message","Missing 'exe' parameter"}
            };
        }
        return kill_app_by_name(exe_name);
    }

    if (command == "START_APP") {
        const std::string path = request.value("path", "");
        if (path.empty()) {
            return {
                {"status","error"},
                {"module", get_module_name()},
                {"message","Missing 'path' parameter"}
            };
        }
        return start_app(path);
    }

    return {
        {"status","error"},
        {"module", get_module_name()},
        {"message","Unknown APP command"}
    };
}
