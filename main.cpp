// main.cpp  – WebSocket Server cho RemoteTool
// Build với: Boost.Beast, Boost.Asio, nlohmann_json
// Yêu cầu: link ws2_32.lib, wsock32.lib (nếu dùng MSVC)

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <iostream>
#include <string>
#include <thread>
#include <mutex>

#include <nlohmann/json.hpp>
#include "core/CommandDispatcher.hpp"

namespace beast     = boost::beast;
namespace websocket = beast::websocket;
namespace net       = boost::asio;
using tcp           = net::ip::tcp;
using json          = nlohmann::json;

static std::mutex cout_mtx;
static CommandDispatcher g_dispatcher;

//==================== SESSION ====================//
// Xử lý 1 phiên WebSocket: đọc JSON -> dispatch -> trả JSON
static void do_session(tcp::socket s) {
    try {
        {
            std::lock_guard<std::mutex> lk(cout_mtx);
            std::cout << "[SESSION] START from "
                      << s.remote_endpoint() << "\n";
        }

        websocket::stream<tcp::socket> ws{std::move(s)};
        ws.accept();

        {
            std::lock_guard<std::mutex> lk(cout_mtx);
            std::cout << "[SESSION] WS ACCEPTED\n";
        }

        for (;;) {
            beast::flat_buffer buffer;
            ws.read(buffer);

            std::string req = beast::buffers_to_string(buffer.data());
            {
                std::lock_guard<std::mutex> lk(cout_mtx);
                std::cout << "[SESSION] RECV: " << req << "\n";
            }

            json reply;
            try {
                // Ở đây bạn cần đảm bảo đã register module vào g_dispatcher
                reply = g_dispatcher.dispatch(json::parse(req));
            } catch (const json::exception& e) {
                reply = {
                    {"status","error"},
                    {"module","CORE"},
                    {"message", std::string("JSON error: ") + e.what()}
                };
            }

            ws.text(true);
            ws.write(net::buffer(reply.dump()));

            {
                std::lock_guard<std::mutex> lk(cout_mtx);
                std::cout << "[SESSION] SEND: " << reply.dump() << "\n";
            }
        }
    } catch (const beast::system_error& se) {
        if (se.code() == websocket::error::closed) {
            std::lock_guard<std::mutex> lk(cout_mtx);
            std::cout << "[SESSION] Client closed connection\n";
        } else {
            std::lock_guard<std::mutex> lk(cout_mtx);
            std::cerr << "[SESSION] Beast error: "
                      << se.code().message() << "\n";
        }
    } catch (const std::exception& e) {
        std::lock_guard<std::mutex> lk(cout_mtx);
        std::cerr << "[SESSION] ERROR: " << e.what() << "\n";
    }
}

//==================== MAIN ====================//

int main() {
    // Tự flush cout ngay sau mỗi << (tránh log bị “đọng” trong buffer)
    std::cout << std::unitbuf;

    try {
        SetConsoleOutputCP(CP_UTF8);
        // Vô hiệu Ctrl+C để tránh accept() bị cancel
        SetConsoleCtrlHandler(NULL, TRUE);

        std::cout << "[MAIN] START\n";
        std::cout << "[MAIN] BUILD: " << __DATE__ << " " << __TIME__ << "\n";

        // 1. Đăng ký các module cho g_dispatcher (TÙY BẠN)
        // Ví dụ:
        // g_dispatcher.register_module(std::make_unique<ProcessManager>());
        // g_dispatcher.register_module(std::make_unique<SystemControl>());
        // ...

        const unsigned short port = 9010;
        auto address = net::ip::make_address("0.0.0.0");   // listen trên mọi IP
        std::cout << "[MAIN] BIND ADDRESS = " << address.to_string()
                  << ", PORT = " << port << "\n";

        net::io_context ioc{1};

        // 2. Tạo acceptor & bind port
        tcp::acceptor acceptor{ioc};
        beast::error_code ec;

        tcp::endpoint endpoint(address, port);
        acceptor.open(endpoint.protocol(), ec);
        if (ec) {
            std::cerr << "[MAIN] acceptor.open error: " << ec.message() << "\n";
            std::cout << "Press Enter to exit...\n";
            std::cin.get();
            return 1;
        }

        acceptor.set_option(net::socket_base::reuse_address(true), ec);
        if (ec) {
            std::cerr << "[MAIN] set_option error: " << ec.message() << "\n";
        }

        acceptor.bind(endpoint, ec);
        if (ec) {
            std::cerr << "[MAIN] bind error: " << ec.message()
                      << " (port có thể đang bận)\n";
            std::cout << "Press Enter to exit...\n";
            std::cin.get();
            return 1;
        }

        acceptor.listen(net::socket_base::max_listen_connections, ec);
        if (ec) {
            std::cerr << "[MAIN] listen error: " << ec.message() << "\n";
            std::cout << "Press Enter to exit...\n";
            std::cin.get();
            return 1;
        }

        std::cout << "[MAIN] ACCEPTOR READY\n";
        std::cout << "LISTENING ws://" << address.to_string()
                  << ":" << port << "\n";
        std::cout << "READY, entering accept loop...\n";

        // 3. Vòng lặp accept
        for (;;) {
            try {
                tcp::socket s{ioc};
                std::cout << "[ACCEPT] Waiting for connection...\n";
                acceptor.accept(s, ec);
                if (ec) {
                    std::cerr << "[ACCEPT] error: " << ec.message() << "\n";
                    continue;
                }

                std::cout << "[ACCEPT] From "
                          << s.remote_endpoint() << "\n";

                // Mỗi client 1 thread
                std::thread(do_session, std::move(s)).detach();
            }
            catch (const std::exception& e) {
                std::cerr << "[ACCEPT LOOP ERROR] " << e.what() << "\n";
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "[FATAL] " << e.what() << "\n";
        std::cout << "Press Enter to exit...\n";
        std::cin.get();      // giữ console lại để đọc log
        return 1;
    }
}
