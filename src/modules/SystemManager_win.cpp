#include "SystemManager.hpp"

// Shutdown system
json SystemManager::shutdown_system() const {
    // Yeu cau tat may
    HANDLE hToken = nullptr; // Handle cho token, co tac dung cap quyen
    TOKEN_PRIVILEGES tkp{}; // Chua thong tin ve quyen 

    // Mo token cua quy trinh hien tai
    if (!OpenProcessToken(GetCurrentProcess(), 
                          TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, 
                          &hToken)) 
    {
        return {
            {"status","error"},
            {"module", get_module_name()},
            {"message","OpenProcessToken failed"},
            {"last_error", static_cast<int>(GetLastError())}
        };
    }

    // Lay quyen shutdown
    LookupPrivilegeValue(nullptr, SE_SHUTDOWN_NAME, &tkp.Privileges[0].Luid);
    tkp.PrivilegeCount = 1; // Mot quyen
    tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    AdjustTokenPrivileges(hToken, FALSE, &tkp, 0, nullptr, nullptr);

    // Thuc hien shutdown
    const BOOL res = ExitWindowsEx(EWX_SHUTDOWN | EWX_FORCE,
                                    SHTDN_REASON_MAJOR_OTHER | SHTDN_REASON_MINOR_OTHER);       
                                    const DWORD gle = res ? 0 : GetLastError();
                                    CloseHandle(hToken);
    if (!res) {
        return {
        {"status","error"},
        {"module", get_module_name()},
        {"message","ExitWindowsEx failed"},
        {"last_error", static_cast<int>(gle)}
        };
    }

    return {
        {"status","success"},
        {"module", get_module_name()},
        {"command","SHUTDOWN"},
        {"message", "System is shutting down"}
    };           
}

// Restart system

json SystemManager::restart_system() const {
    HANDLE hToken = nullptr;
    TOKEN_PRIVILEGES tkp{};

    // 1. Mở token của process
    if (!OpenProcessToken(GetCurrentProcess(),
                          TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY,
                          &hToken))
    {
        return {
            {"status","error"},
            {"module", get_module_name()},
            {"message","OpenProcessToken failed"},
            {"last_error", static_cast<int>(GetLastError())}
        };
    }

    // 2. Lấy quyền shutdown
    if (!LookupPrivilegeValue(nullptr, SE_SHUTDOWN_NAME, &tkp.Privileges[0].Luid)) {
        DWORD gle = GetLastError();
        CloseHandle(hToken);
        return {
            {"status","error"},
            {"module", get_module_name()},
            {"message","LookupPrivilegeValue failed"},
            {"last_error", static_cast<int>(gle)}
        };
    }

    tkp.PrivilegeCount = 1;
    tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    // 3. Gán quyền vào token
    AdjustTokenPrivileges(hToken, FALSE, &tkp, 0, nullptr, nullptr);
    DWORD adjustGLE = GetLastError();

    if (adjustGLE == ERROR_NOT_ALL_ASSIGNED) {
        CloseHandle(hToken);
        return {
            {"status","error"},
            {"module", get_module_name()},
            {"message","Privilege SE_SHUTDOWN_NAME not assigned"},
            {"last_error", static_cast<int>(adjustGLE)}
        };
    }

    // 4. Thực hiện restart
    const BOOL res = ExitWindowsEx(
        EWX_REBOOT | EWX_FORCE,
        SHTDN_REASON_MAJOR_OTHER | SHTDN_REASON_MINOR_OTHER
    );

    DWORD gle = res ? 0 : GetLastError();
    CloseHandle(hToken);

    if (!res) {
        return {
            {"status","error"},
            {"module", get_module_name()},
            {"message","ExitWindowsEx failed"},
            {"last_error", static_cast<int>(gle)}
        };
    }

    return {
        {"status","success"},
        {"module", get_module_name()},
        {"command","RESTART"},
        {"message","System is restarting"}
    };
}

// DISPATCHER

json SystemManager::handle_command(const json& request) {
    const std::string command = request.value("command", "");

    if (command == "SHUTDOWN") {
        return shutdown_system();
    }
    if (command == "RESTART") {
        return restart_system();
    }
    
    return {
        {"status","error"},
        {"module", get_module_name()},
        {"message","Unknown SYSTEM command"},
        {"received_command", command}
    };
}
