#pragma once
#include "../interfaces/IRemoteModule.hpp"
#include <unordered_map>
#include <memory>
#include <string>
#include <iostream>
#include "../modules/ProcessManager.hpp"
#include "../modules/SystemManager.hpp"
#include "../modules/ScreenManager.hpp"
#include "../modules/AppManager.hpp"
#include "../modules/WebcamManager.hpp"
#include "../modules/KeyManager.hpp"

class CommandDispatcher {
private:
    std::unordered_map<std::string, std::unique_ptr<IRemoteModule>> modules_;

public: 
    CommandDispatcher() {
        register_module(std::make_unique<ProcessManager>());
        register_module(std::make_unique<SystemManager>());
        register_module(std::make_unique<ScreenManager>());
        register_module(std::make_unique<AppManager>());
        register_module(std::make_unique<WebcamManager>());
        register_module(std::make_unique<KeyManager>());
    }

    void register_module(std::unique_ptr<IRemoteModule> module) {
        if (!module) return;
        const std::string name = module->get_module_name();
        auto [it, inserted] = modules_.emplace(name, std::move(module));
        if (inserted) std::cout << "Module registered: " << name << "\n";
    }

    // --- MỚI: Hàm lấy Module pointer để Main ép kiểu ---
    IRemoteModule* get_module(const std::string& name) {
        auto it = modules_.find(name);
        if (it != modules_.end()) return it->second.get();
        return nullptr;
    }

    json dispatch(const json& request) {
        if (!request.contains("module")) return { {"status","error"},{"message","Missing module"} };
        
        const std::string module_name = request["module"].get<std::string>();
        auto it = modules_.find(module_name);
        if (it == modules_.end()) return { {"status","error"},{"message","Unknown module"} };
        
        return it->second->handle_command(request);
    }
};
