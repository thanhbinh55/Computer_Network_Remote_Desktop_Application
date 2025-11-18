// src/modules/AppManager.hpp
#pragma once
#include "../interfaces/IRemoteModule.hpp"
#include <string>
#include <vector>
#include <windows.h>
#include <tlhelp32.h>
#include <nlohmann/json.hpp>

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
