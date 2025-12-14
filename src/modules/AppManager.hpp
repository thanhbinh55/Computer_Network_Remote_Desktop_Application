// src/modules/AppManager.hpp
#pragma once
#include "../interfaces/IRemoteModule.hpp"
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cctype>

#ifdef _WIN32
#include <windows.h> 
#include <tlhelp32.h>
#else
#include <dirent.h> // for listing apps, processes
#include <unistd.h> // unix system api
#include <signal.h> // for killing apps, processes
#include <fstream> 
#include <wait.h> // wait for children processes
#include <pwd.h>
#include <iostream>
#include <sys/types.h>
#include <grp.h> // for using initgroups
#endif

using nlohmann::json;

class AppManager : public IRemoteModule {
private:
    std::string module_name_ = "APP";

    // implement
    json list_apps();
    json kill_app_by_name(const std::string& exe_name);
    json start_app(const std::string& path_or_exe);

public:
    AppManager() = default;

    const std::string& get_module_name() const override {
        return module_name_;
    }

    json handle_command(const json& request) override;
};
