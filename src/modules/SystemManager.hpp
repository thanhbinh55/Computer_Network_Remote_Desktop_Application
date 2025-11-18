// src/modules/ProcessManager.hpp
#pragma once
#include "../interfaces/IRemoteModule.hpp"
#include <string>
#include <windows.h> // Cho các kiểu dữ liệu Windows

class SystemManager : public IRemoteModule {
private:
    std::string module_name_ = "SYSTEM";
    
    // Các hàm thực thi Windows API (Triển khai trong .cpp)
    json shutdown_system() const;
    json restart_system() const;
    
public:
    SystemManager() = default;
    const std::string& get_module_name() const override { return module_name_; }
    json handle_command(const json& request) override; // Định nghĩa trong .cpp
};