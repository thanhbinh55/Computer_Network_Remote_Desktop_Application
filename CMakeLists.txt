cmake_minimum_required(VERSION 3.10)
project(RemoteControlTool LANGUAGES CXX)

# C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---- Dependencies (vcpkg toolchain đã bật trong VS) ----
find_package(Boost REQUIRED COMPONENTS system)      # Beast là header-only, chỉ cần system
find_package(nlohmann_json CONFIG REQUIRED)

# ---- Sources ----
set(SERVER_SOURCES
    main.cpp                                   # main ở THƯ MỤC GỐC
    src/modules/ProcessManager_win.cpp
    src/modules/SystemManager_win.cpp
    src/modules/ScreenManager_win.cpp
    src/modules/AppManager_win.cpp
    # thêm các .cpp khác nếu có
)

add_executable(server ${SERVER_SOURCES})

# Đưa "src" vào include path để #include "interfaces/IRemoteModule.hpp" hoạt động
target_include_directories(server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# (tùy chọn) macro bạn dùng
target_compile_definitions(server PRIVATE
    WIN32_LEAN_AND_MEAN
    # ASIO_STANDALONE  # bật nếu code yêu cầu
)

# Link thư viện
target_link_libraries(server PRIVATE
    Boost::system
    nlohmann_json::nlohmann_json
    ws2_32
    user32
    advapi32
)
