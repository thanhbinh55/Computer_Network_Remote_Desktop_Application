// CommandDispatcher.hpp
#pragma once
// #include "./interfaces/IRemoteModule.hpp"
#include "interfaces/IRemoteModule.hpp"
#include <unordered_map>
#include <memory>
#include <string>
#include <iostream>
#include "modules/ProcessManager.hpp"
#include "modules/SystemManager.hpp"
#include "modules/ScreenManager.hpp"
#include "modules/AppManager.hpp"

class CommandDispatcher {
private:
    std::unordered_map<std::string, std::unique_ptr<IRemoteModule>> modules_;

public: 
    CommandDispatcher() {
        // Đăng ký PROCESS
        register_module(std::make_unique<ProcessManager>());

        // Đăng ký SYSTEM
        register_module(std::make_unique<SystemManager>());
    
        // Đăng ký SCREEN
        register_module(std::make_unique<ScreenManager>());

        // Đăng ký APP
        register_module(std::make_unique<AppManager>());
    }

    void register_module(std::unique_ptr<IRemoteModule> module) {
        if (!module) return;
        const std::string name = module->get_module_name();   // lấy tên TRƯỚC khi move

        auto [it, inserted] = modules_.emplace(name, std::move(module)); // move 1 lần
        if (!inserted) {
            std::cerr << "[Dispatcher] Duplicate module name: " << name << "\n";
            return;
        }
        std::cout << "Module registered: " << name << "\n";
    }

    json dispatch(const json& request) {
        if (!request.contains("module")) {
            return { {"status","error"},{"message","Malformed request (missing module)"} };
        }
        const std::string module_name = request["module"].get<std::string>();
        auto it = modules_.find(module_name);
        if (it == modules_.end()) {
            return { {"status","error"},{"message","Unknown module: " + module_name} };
        }
        return it->second->handle_command(request);
    }
};
