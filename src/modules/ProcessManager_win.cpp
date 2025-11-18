#include "ProcessManager.hpp"
#include <windows.h>
#include <tlhelp32.h>
#include <sstream>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

using nlohmann::json;

// Tiện ích: WCHAR -> UTF-8
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

// ==== WINDOWS IMPL ====

json ProcessManager::list_processes() {
    json process_list = json::array();
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

    if (snapshot != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32W pe32{};
        pe32.dwSize = sizeof(pe32);

        if (Process32FirstW(snapshot, &pe32)) {
            do {
                process_list.push_back({
                    {"pid",      static_cast<unsigned long>(pe32.th32ProcessID)},
                    {"name",     to_utf8(std::wstring(pe32.szExeFile))},
                    {"threads",  static_cast<unsigned long>(pe32.cntThreads)}
                });
            } while (Process32NextW(snapshot, &pe32));
        }
        CloseHandle(snapshot);
    }

    return {
        {"status", "success"},
        {"module", get_module_name()},
        {"data",   process_list}
    };
}

json ProcessManager::kill_process(unsigned long pid) {
    // Không cho tự kill chính server
    const DWORD self = GetCurrentProcessId();
    if (static_cast<DWORD>(pid) == self) {
        return {
            {"status","error"},
            {"module", get_module_name()},
            {"message","Refuse to terminate own server process"},
            {"pid", pid}
        };
    }

    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, static_cast<DWORD>(pid));
    if (!hProcess) {
        const DWORD gle = GetLastError();
        return {
            {"status","error"},
            {"module", get_module_name()},
            {"message","OpenProcess failed"},
            {"pid", pid},
            {"last_error", static_cast<int>(gle)}
        };
    }

    const BOOL ok = TerminateProcess(hProcess, 1 /* exit code khác 0 */);
    const DWORD gle = ok ? 0 : GetLastError();
    CloseHandle(hProcess);

    if (!ok) {
        return {
            {"status","error"},
            {"module", get_module_name()},
            {"message","TerminateProcess failed"},
            {"pid", pid},
            {"last_error", static_cast<int>(gle)}
        };
    }

    return {
        {"status","success"},
        {"module", get_module_name()},
        {"command","KILL"},
        {"pid", pid}
    };
}

json ProcessManager::start_process(const std::string& path) {
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
            {"message","Failed to start process"},
            {"path", path},
            {"last_error", static_cast<int>(gle)}
        };
    }

    // Lấy PID trước khi đóng handle
    const DWORD child_pid = pi.dwProcessId;
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return {
        {"status","success"},
        {"module", get_module_name()},
        {"command","START"},
        {"pid", static_cast<unsigned long>(child_pid)},
        {"path", path}
    };
}

// ==== DISPATCH CHÍNH ====

json ProcessManager::handle_command(const json& request) {
    const std::string command = request.value("command", "");
    if (command == "LIST") {
        return list_processes();
    }
    if (command == "KILL") {
        if (!request.contains("pid") || !request["pid"].is_number_unsigned()) {
            return {
                {"status","error"},
                {"module", get_module_name()},
                {"message","Missing or invalid 'pid' parameter"}
            };
        }
        const unsigned long pid = request["pid"].get<unsigned long>();
        return kill_process(pid);
    }
    if (command == "START") {
        const std::string path = request.value("path", "");
        if (path.empty()) {
            return {
                {"status","error"},
                {"module", get_module_name()},
                {"message","Missing 'path' parameter"}
            };
        }
        return start_process(path);
    }

    return {
        {"status","error"},
        {"module", get_module_name()},
        {"message","Unknown PROCESS command"}
    };
}
