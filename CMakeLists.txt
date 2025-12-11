cmake_minimum_required(VERSION 3.10)
project(RemoteControlTool LANGUAGES CXX)

# ---- C++17 ----
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---- [QUAN TRỌNG] ÉP STATIC RUNTIME (/MT) ----
# Giúp file .exe chạy được trên máy khác mà không cần cài Visual C++ Redistributable
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
add_compile_options(/MT$<$<CONFIG:Debug>:d>)

# ---- Build static library (để tránh phụ thuộc DLL ngoài) ----
set(BUILD_SHARED_LIBS OFF)

# ---- Tìm package (vcpkg) ----
find_package(Boost REQUIRED COMPONENTS system)
find_package(nlohmann_json CONFIG REQUIRED)

# ---- Source Files ----
set(SERVER_SOURCES
    main.cpp
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
    src/modules/KeyManager_win.cpp
    src/modules/WebcamManager_win.cpp        # thêm webcam manager
    # thêm các .cpp khác nếu có
)

add_executable(server ${SERVER_SOURCES})

# ---- Include Path ----
# Đưa "src" vào include path để #include "interfaces/IRemoteModule.hpp" hoạt động
target_include_directories(server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# ---- Link Libraries (Thư viện liên kết) ----
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
    shlwapi        # Cần cho xử lý đường dẫn file
    ole32          # Cần cho COM (CoInitialize)
    mf             # Cần cho Webcam (Media Foundation)
    mfplat         # Cần cho Webcam
    mfreadwrite    # Cần cho Webcam
    mfuuid         # Cần cho Webcam
    gdiplus        # Cần cho Screenshot (lưu ảnh JPEG/PNG)
)
    # mf              # <--- [THÊM] Media Foundation
    # mfplat          # <--- [THÊM]
    # mfreadwrite     # <--- [THÊM]
    # mfuuid          # <--- [THÊM]
    # shlwapi         # <--- [THÊM] (Cho thao tác file path)
    # ole32           # <--- [THÊM] (Cho CoInitialize)
)
