#pragma once
#include "../interfaces/IRemoteModule.hpp"
#include <string>
#include <iostream>
#include <nlohmann/json.hpp>
#include <fstream>

#if _WIN32
#include <windows.h>
#include <tlhelp32.h>
#else
#include <signal.h>
#include <unistd.h>
#include <cstdlib>
#include <unistd.h> // unix system api
#include <signal.h> // for killing apps, processes
#include <fstream> 
#include <wait.h> // wait for children processes
#include <pwd.h>
#include <iostream>
#include <sys/types.h>
#include <grp.h> // for using initgroups
#include <dirent.h>
#endif

using json = nlohmann::json;

class ProcessManager : public IRemoteModule {
public:
    const std::string& get_module_name() const override;
    json handle_command(const json& request) override;

private:
    json list_processes();
    bool kill_process(int pid);
    
    // [MỚI] Hàm khởi tạo tiến trình trả về PID
    bool start_process(const std::string& path, int& out_pid);
};
