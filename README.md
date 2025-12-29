# 🖥 Remote Control Desktop Application

Đồ án Môn **Mạng Máy Tính** – Hệ thống Điều khiển & Giám sát Máy tính trong mạng LAN

![C++](https://img.shields.io/badge/C%2B%2B-17-blue)
![Node.js](https://img.shields.io/badge/Node.js-18+-green)
![Platform](https://img.shields.io/badge/Platform-Windows%20%7C%20Linux-lightgrey)
![License](https://img.shields.io/badge/License-Educational-yellow)

Giải pháp điều khiển và giám sát máy tính trong mạng cục bộ (LAN), phục vụ mục đích học tập
và thực hành môn Mạng máy tính, với khả năng giao tiếp theo thời gian thực.

**Tính năng · Cài đặt · Sử dụng · Kiến trúc · API**

---

## 📑 Mục lục

- [Giới thiệu](#-giới-thiệu)
- [Tính năng chính](#-tính-năng-chính)
- [Kiến trúc hệ thống](#-kiến-trúc-hệ-thống)
- [Yêu cầu hệ thống](#-yêu-cầu-hệ-thống)
- [Cài đặt và chạy](#-cài-đặt-và-chạy)
- [Cấu trúc thư mục](#-cấu-trúc-thư-mục)
- [Công nghệ sử dụng](#-công-nghệ-sử-dụng)
- [API Reference](#-api-reference)
- [Giao thức truyền thông](#-giao-thức-truyền-thông)
- [Bảo mật](#-bảo-mật)
- [Troubleshooting](#-troubleshooting)
- [Đóng góp](#-đóng-góp)
- [Tác giả](#-tác-giả)
- [Giấy phép](#-giấy-phép)

---

## 🎯 Giới thiệu

Trong môi trường mạng cục bộ (LAN), nhu cầu hỗ trợ kỹ thuật từ xa và giám sát trạng thái
máy tính thường xuyên phát sinh trong các tình huống như quản trị phòng máy, hỗ trợ người
dùng trong cùng cơ quan hoặc lớp học, hay điều khiển nhanh một máy tính khác mà không
cần truy cập vật lý trực tiếp.

Tuy nhiên, nhiều giải pháp điều khiển từ xa hiện nay yêu cầu cấu hình phức tạp hoặc phụ
thuộc vào Internet, gây bất tiện khi triển khai trong môi trường mạng nội bộ, đặc biệt đối
với các nhu cầu học tập và hỗ trợ kỹ thuật cơ bản.

Xuất phát từ thực tế đó, đồ án này tập trung xây dựng một phần mềm điều khiển máy tính
từ xa hoạt động **trong phạm vi mạng LAN**, sử dụng công nghệ **WebSocket** kết hợp
với ngôn ngữ lập trình **C++**. Hệ thống được thiết kế theo mô hình **Client – Server**,
bao gồm server chạy nền trên máy bị điều khiển và client sử dụng giao diện web để thực
hiện các thao tác điều khiển và giám sát theo thời gian thực.

---


## ✨ Tính năng chính

Các chức năng của hệ thống được tổ chức theo **nhóm chức năng**, tương ứng với các
module xử lý phía server và giao diện hiển thị phía client. Thiết kế này bám sát mô hình
Client – Server trong mạng LAN, đảm bảo dễ mở rộng và kiểm soát luồng dữ liệu.

---

## 🧩 Nhóm giám sát hệ thống  
*(Process / App / File)*

🔍 **Mục tiêu & phạm vi**  
Cung cấp các thao tác **truy vấn trạng thái** và **điều khiển mức hệ điều hành** trong
phạm vi cho phép của quyền người dùng. Dữ liệu trả về có cấu trúc, kích thước vừa phải,
phù hợp truyền bằng **JSON qua WebSocket (text frame)**.

🔁 **Luồng xử lý tổng quát**  
`Client` → gửi JSON request  
`Dispatcher` → định tuyến lệnh  
`Module` → gọi OS APIs  
`Server` → đóng gói JSON response → gửi về client

---

### ⚙️ Process Manager

📌 **Chức năng chính**
- `LIST` – Lấy danh sách tiến trình (PID, tên, CPU/RAM nếu có, user/owner, trạng thái)
- `KILL` – Kết thúc tiến trình theo PID
- `START` – Chạy tiến trình theo đường dẫn hoặc command

🛠 **Phân tích kỹ thuật (OS & quyền)**
- Windows/Linux sử dụng cơ chế liệt kê process khác nhau
- Một số thuộc tính (owner, CPU usage) cần quyền cao hơn hoặc API bổ sung
- Thao tác `KILL` phụ thuộc quyền hệ điều hành

⚠️ **Phản hồi & lỗi**
- Response chuẩn hoá: `status`, `request_id`, `data`, `error`
- Lỗi thường gặp:
  - PID không tồn tại / đã kết thúc
  - Không đủ quyền (permission denied)
  - Lỗi OS API

---

### 🧱 App Manager

📌 **Chức năng chính**
- `LIST_APPS` – Liệt kê ứng dụng/tiến trình mức *app*
- `START_APP`, `KILL_APP` – Mở hoặc tắt ứng dụng

🛠 **Phân tích kỹ thuật**
- Khái niệm *app* phụ thuộc hệ điều hành:
  - Windows: process + window/app entry
  - Linux: process hoặc desktop entry
- Output được trừu tượng hoá về schema chung  
  *(name, pid, path, state)*

---

### 📁 File Manager

📌 **Chức năng chính**
- `LIST_DIR` – Liệt kê thư mục (tên, loại, kích thước, thời gian sửa)
- `READ / WRITE` – Đọc, ghi file văn bản (nếu cho phép)
- `UPLOAD / DOWNLOAD` – Truyền file nhị phân

🔐 **Bảo vệ đường dẫn**
- Chuẩn hoá đường dẫn (path normalization)
- Giới hạn phạm vi truy cập hoặc whitelist thư mục
- Xử lý lỗi I/O rõ ràng (không tồn tại, không quyền, file bị khoá)

📦 **Chiến lược truyền dữ liệu**
- Metadata / danh sách → JSON
- File lớn → binary frame, chia chunk nếu cần

---

## ⚡ Nhóm chức năng realtime  
*(Key Logger / Screenshot / Webcam / Remote Control)*

⏱ **Đặc trưng realtime**
- Phát sinh dữ liệu liên tục
- Yêu cầu **độ trễ thấp** và **ổn định**

🧠 **Thiết kế luồng**
- **Control-plane**: JSON (bật/tắt stream, cấu hình FPS, chất lượng)
- **Data-plane**: Binary frame (ảnh, video, remote frame)

⚖️ **Trade-off kỹ thuật**
- FPS/chất lượng cao → tăng CPU, tăng băng thông
- Network yếu → giảm FPS, giảm chất lượng hoặc drop frame

---

### ⌨️ Key Logger

🎯 **Mục tiêu**  
Thu thập sự kiện bàn phím (và có thể chuột) từ máy bị điều khiển, gửi về client theo
thời gian thực.

🔁 **Luồng dữ liệu**
- `START_KEY_STREAM` → bật hook
- Push event JSON liên tục
- `STOP_KEY_STREAM` → giải phóng tài nguyên

🛠 **Lưu ý kỹ thuật**
- Hook phụ thuộc nền tảng (Windows / Linux)
- Cần throttle hoặc batch event
- Tự tắt hook khi client disconnect

---

### 🖼 Screenshot

🎯 **Mục tiêu**  
Chụp ảnh màn hình theo yêu cầu phục vụ giám sát hoặc preview.

🔁 **Luồng xử lý**
- Client gửi JSON (resolution, quality, mode)
- Server capture → encode (PNG/JPEG) → gửi binary frame

📉 **Hiệu năng**
- Capture + encode tốn CPU
- JPEG nhẹ hơn nhưng giảm chất lượng
- PNG nặng hơn nhưng rõ chữ

---

### 🎥 Webcam Streaming

🎯 **Mục tiêu**  
Streaming webcam liên tục theo thời gian thực.

🔁 **Luồng dữ liệu**
- Client bật/tắt stream
- Server chạy luồng nền: capture → encode → gửi frame theo FPS

🛠 **Kỹ thuật**
- Điều chỉnh FPS theo băng thông LAN
- Network chậm → ưu tiên drop frame
- Đồng bộ ghi WebSocket do nhiều luồng

---

### 🖱 Remote Control / Remote Desktop

🎯 **Mục tiêu**  
Quan sát màn hình và gửi thao tác điều khiển từ client đến server.

🔄 **Luồng dữ liệu hai chiều**
- Server → Client: ảnh màn hình (binary) + metadata (JSON)
- Client → Server: input event (key, mouse, click)

⚠️ **Vấn đề kỹ thuật**
- Đồng bộ toạ độ theo scale hiển thị
- Giảm FPS / chất lượng để tối ưu băng thông
- Giới hạn thao tác nguy hiểm, ghi log lệnh

---

## 🔌 Nhóm điều khiển hệ thống & dữ liệu trình duyệt

### 🔄 Shutdown / Restart

⚠️ **Tính chất**
- Lệnh tác động mạnh, có thể ngắt phiên điều khiển

🛠 **Xử lý**
- Validate request
- Trả ACK trước khi thực thi
- Client xử lý reconnect sau khi máy khởi động lại

---

### 🌐 Edge Manager *(Bookmark / History)*

🎯 **Phạm vi**
- Chỉ trích xuất dữ liệu **không nhạy cảm**
  - Bookmark
  - History

🔐 **Giới hạn bảo mật**
- Cookie và mật khẩu được mã hoá bởi OS/trình duyệt
- Không giải mã, không can thiệp ngoài phạm vi đồ án

Giới hạn này phản ánh đúng thực tế bảo mật hiện đại và đảm bảo tính an toàn của hệ thống.


---


## 🏗 Kiến trúc hệ thống

### Tổng quan kiến trúc

Hệ thống được xây dựng theo mô hình **Client – Server**, kết hợp thêm một
**Registry Server** để hỗ trợ phát hiện và quản lý các server đang hoạt động trong
mạng LAN. Thiết kế này phù hợp cho các ứng dụng điều khiển và giám sát máy tính
theo thời gian thực, đồng thời giảm cấu hình thủ công khi triển khai.

Các thành phần giao tiếp với nhau chủ yếu thông qua **WebSocket trên TCP** và
**UDP broadcast** trong mạng LAN.

---

### 🔄 Luồng hoạt động tổng thể
🔹 Web Client gửi lệnh điều khiển và hiển thị kết quả

🔹 Server xử lý logic, gọi API hệ điều hành và trả dữ liệu

🔹 Registry Server hỗ trợ discovery và danh sách server online


```text
+---------------------+        HTTP / REST        +----------------------+
|  Web Client         | <----------------------> |  Registry Server     |
|  (Angular, Browser) |        (Online List)     |  (Node.js)           |
+----------+----------+                          +-----------+----------+
           |                                                     ^
           | Send command / Receive data                         |
           |                                                     | Register / Heartbeat
           v                                                     |
+---------------------+        WebSocket (TCP)       +----------------------+
|  WebSocket Session  | <--------------------------> |  Server (C++)        |
|  (LAN)              |                              |  Listener + Session  |
+----------+----------+                              |  Manager              |
           |                                         +-----------+----------+
           | Frame recv                                           |
           v                                                      |
+---------------------+        Dispatch & Route                   |
|  Command Dispatcher | ------------------------------------------+
+----------+----------+
           |
           v
+---------------------+
|  Functional Modules |
|  Process / App      |
|  File / Screen      |
|  Webcam / Key       |
|  System / Edge      |
+----------+----------+
           |
           v
+---------------------+
|  OS / System APIs   |
|  Windows / Linux    |
+---------------------+


 ```

---

## 🖥️ Yêu cầu hệ thống

Hệ thống được thiết kế và kiểm thử **chỉ trong phạm vi mạng LAN**, phục vụ mục đích
học tập và hỗ trợ kỹ thuật cơ bản. Các yêu cầu dưới đây được tổng hợp dựa trên
**giả định kỹ thuật (Chương 1)** và **môi trường thử nghiệm (Chương 4)** của báo cáo.

---

### 🔹 Agent – Máy bị điều khiển (C++ Server)

| Yêu cầu | Chi tiết |
|------|--------|
| Hệ điều hành | Windows hoặc Linux |
| Kiến trúc | 64-bit |
| Quyền hệ điều hành | Quyền người dùng đủ để truy xuất process, file, screen, input |
| Mạng | Kết nối LAN (cùng subnet hoặc định tuyến nội bộ) |
| CPU | ≥ 2 cores |
| RAM | ≥ 512 MB |
| Thư viện hệ thống (Linux) | libX11, libXtst (phục vụ screen & input) |

**Lưu ý:**
- Agent chạy nền trên máy bị điều khiển.
- Một số thao tác (kill process, shutdown, điều khiển input) phụ thuộc quyền hệ điều hành.
- Hệ thống **không yêu cầu quyền kernel** và **không cài driver**.

---

### 🔹 Gateway / Registry Server (Node.js)

| Yêu cầu | Chi tiết |
|------|--------|
| Hệ điều hành | Windows hoặc Linux |
| Runtime | Node.js v18+ |
| Mạng | LAN |
| Cổng sử dụng | TCP 9100 (WebSocket), UDP 9103 (Discovery), HTTP 8080 (Web UI) |
| Chức năng | UDP discovery, quản lý agent online, trung gian giao tiếp |

---

### 🔹 Web Client (Angular)

| Yêu cầu | Chi tiết |
|------|--------|
| Thiết bị | Máy trong cùng mạng LAN |
| Trình duyệt | Chrome / Edge (hỗ trợ WebSocket & ES6) |
| Kết nối | LAN |

**Lưu ý:**
- Web Client chạy trên trình duyệt, **không cần cài phần mềm riêng**.
- Giao diện dùng để giám sát và điều khiển agent theo thời gian thực.

---

### 🔹 Môi trường phát triển (Build từ source)

| Thành phần | Yêu cầu |
|---------|--------|
| Agent (C++) | Compiler hỗ trợ C++17 |
| Linux | gcc / g++, CMake |
| Windows | Visual Studio hoặc MinGW |
| Gateway | Node.js v18+, npm |
| Thư viện C++ | ASIO, nlohmann/json (đã tích hợp trong dự án) |

---

### ⚠️ Giới hạn có chủ đích

- Hệ thống **chỉ thiết kế cho mạng LAN**, không triển khai WAN/Internet.
- Không thu thập hoặc giải mã dữ liệu nhạy cảm (mật khẩu, cookie).
- Không can thiệp kernel, không bypass cơ chế bảo mật hệ điều hành.


## ⚙️ Cài đặt và Chạy

Hệ thống được triển khai và sử dụng **hoàn toàn trong mạng LAN**.

---

### 🔹 1️⃣ Registry Server (Node.js)

**Registry Server** chịu trách nhiệm:
- Lắng nghe UDP Broadcast
- Quản lý danh sách Remote Server đang online

#### Chạy Registry Server
```bash
cd registry_server
npm install
node server.js
Output mong đợi
```
```text
[UDP] Discovery server listening...
[UDP] Registry server running on port 3000
```

🔹 2️⃣ Remote Server – Linux
Cài dependency
```bash
sudo apt install libx11-dev libxtst-dev
```
Build & chạy
```bash
mkdir build
cd build
cmake ..
make
./linux/server
```
🔹 3️⃣ Remote Server – Windows
Yêu cầu
Visual Studio 2022

CMake

vcpkg (đã cấu hình trong dự án)

Build & chạy
```bash
mkdir build
cd build
cmake --preset windows-vcpkg
cmake --build build/win --config Release
./build/win/Release/server.exe
```
🔹 4️⃣ Web Client (Angular)
```bash
cd web-client
npm install
npm start
```
Truy cập
```arduino
http://localhost:4200
```
▶️ Hướng dẫn sử dụng
Thứ tự khởi động hệ thống
1.Chạy Registry Server

2.Chạy Remote Server trên máy bị điều khiển

3.Chạy Web Client

4.Mở trình duyệt và kết nối tới Remote Server trong LAN

---


## 📁 Cấu trúc thư mục

Dự án được tổ chức theo hướng **module hóa**, tách biệt rõ ràng giữa Registry Server, Remote Server (C++) và Web Client (Angular), nhằm đảm bảo dễ bảo trì, mở rộng và phù hợp với kiến trúc Client–Server trong mạng LAN.

```text
Remote_Project/
├── registry_server/            # Registry Server (Node.js)
│   ├── node_modules/            # Thư viện Node.js
│   ├── agents.js                # Quản lý danh sách Agent online
│   ├── discovery.js             # UDP discovery trong LAN
│   ├── server.js                # Entry point của Registry Server
│   ├── package.json
│   └── package-lock.json
│
├── src/                         # Remote Server (C++)
│   ├── core/                    # Thành phần lõi
│   │   ├── CommandDispatcher.hpp
│   │   ├── WebSocketServer.cpp
│   │   ├── WebSocketServer.hpp
│   │   ├── RegistryClient.cpp
│   │   └── RegistryClient.hpp
│   │
│   ├── interfaces/              # Interface chung cho các module
│   │   └── IRemoteModule.hpp
│   │
│   ├── modules/                 # Các module chức năng
│   │   ├── ProcessManager.*
│   │   ├── AppManager.*
│   │   ├── FileManager.*
│   │   ├── ScreenManager_*.cpp
│   │   ├── WebcamManager_*.cpp
│   │   ├── InputManager_*.cpp
│   │   ├── KeyManager_*.cpp
│   │   ├── SystemManager_*.cpp
│   │   └── EdgeManager_*.cpp
│   │
│   ├── utils/                   # Tiện ích dùng chung
│   │   ├── SystemUtils.cpp
│   │   └── SystemUtils.hpp
│   │
│   └── main.cpp                 # Entry point Remote Server
│
├── web-client/                  # Web Client (Angular)
│   ├── src/
│   │   └── app/
│   │       ├── components/      # UI components tái sử dụng
│   │       ├── models/           # Model / interface dữ liệu
│   │       ├── services/         # WebSocket & service logic
│   │       ├── pages/            # Các trang giao diện
│   │       │   ├── agents/       # Danh sách Agent
│   │       │   ├── agent-detail/ # Điều khiển chi tiết Agent
│   │       │   └── about/        # Trang giới thiệu
│   │       ├── app.config.ts
│   │       ├── app.routes.ts
│   │       ├── app.ts
│   │       └── app.html
│   │
│   └── package.json
│
├── CMakeLists.txt               # Cấu hình build CMake
├── CMakePresets.json            # Preset build Windows / Linux
├── README.md                    # Tài liệu hướng dẫn sử dụng
├── LINK VIDEO YOUTUBE.txt       # Link video demo
├── ĐỒ ÁN MÔN MẠNG MÁY TÍNH.pdf   # Báo cáo đồ án
└── LOGAI_REMOTE_CONTROL_APP.docx
```
🧩 Mô tả tổng quan

🔹 registry_server/
Triển khai bằng Node.js, chịu trách nhiệm phát hiện Agent trong LAN thông qua UDP broadcast và quản lý danh sách Agent đang online.

🔹 src/
Remote Server viết bằng C++, chạy trên máy bị điều khiển. Kiến trúc dựa trên Dispatcher và các module chức năng độc lập, hỗ trợ đa nền tảng Windows và Linux.

🔹 web-client/
Web Client phát triển bằng Angular, cung cấp giao diện điều khiển và giám sát thông qua WebSocket theo thời gian thực.

🔹 Tài liệu & cấu hình
Bao gồm file cấu hình build, báo cáo đồ án và tài liệu minh họa hệ thống.

---


## 🧪 Công nghệ sử dụng

Phần này tổng hợp các công nghệ, thư viện và kỹ thuật được sử dụng trong hệ thống,
dựa trên **Chương 2 – Cơ sở lý thuyết và công nghệ sử dụng** và
**Chương 3 – Thiết kế, triển khai hệ thống** của báo cáo đồ án.

---

### 🔹 Ngôn ngữ và nền tảng

| Công nghệ | Vai trò |
|---------|--------|
| C++ (C++17) | Phát triển Agent chạy nền trên máy bị điều khiển |
| JavaScript | Phát triển Registry Server và Web Client |
| HTML / CSS | Xây dựng giao diện web |
| TypeScript | Phát triển Web Client (Angular) |

---

### 🔹 Công nghệ mạng và giao thức

| Công nghệ | Mục đích sử dụng |
|---------|----------------|
| TCP | Kết nối tin cậy cho WebSocket |
| UDP | Phát hiện server trong mạng LAN (UDP broadcast) |
| Socket | Giao tiếp giữa các tiến trình qua mạng |
| WebSocket | Giao tiếp hai chiều realtime giữa client và server |
| HTTP | Thiết lập handshake WebSocket, truy cập Registry |

---

### 🔹 Thư viện và framework chính

#### Phía Server (Agent – C++)

| Thư viện / API | Vai trò |
|---------------|--------|
| ASIO | Lập trình mạng bất đồng bộ (TCP/UDP) |
| nlohmann/json | Phân tích và đóng gói dữ liệu JSON |
| WinAPI / Linux API | Truy cập tài nguyên hệ điều hành |
| STL (thread, mutex) | Xử lý đa luồng và đồng bộ |

---

#### Phía Registry Server (Node.js)

| Công nghệ | Vai trò |
|---------|--------|
| Node.js | Runtime cho Registry Server |
| UDP Socket | Nhận và phản hồi gói discovery |
| HTTP Server | Cung cấp thông tin server online cho client |

---

#### Phía Web Client (Angular)

| Công nghệ | Vai trò |
|---------|--------|
| Angular | Xây dựng giao diện điều khiển |
| WebSocket API | Nhận/gửi dữ liệu realtime |
| ES6 Modules | Tổ chức mã nguồn frontend |

---

### 🔹 API hệ điều hành (Agent)

| API | Mục đích |
|----|---------|
| Windows API | Liệt kê process, thao tác file, input, screen |
| Linux System API | Truy cập process, file, screen |
| X11 / Xtst (Linux) | Capture màn hình và input |
| User32 (Windows) | Gửi sự kiện chuột & bàn phím |

---

### 🔹 Định dạng và luồng dữ liệu

| Loại dữ liệu | Mục đích |
|------------|---------|
| JSON | Truyền lệnh điều khiển, trạng thái, phản hồi |
| Binary frame | Truyền ảnh màn hình, webcam |
| Control path | Luồng điều khiển (JSON) |
| Data path | Luồng dữ liệu lớn (binary) |

---

### 🔹 Kỹ thuật hệ thống

- Mô hình **Client – Server** kết hợp Registry Server
- Kiến trúc **module hóa** phía server
- **Command Dispatcher** để parse và định tuyến request
- **Đa luồng** cho xử lý realtime (webcam, screenshot)
- **Mutex đồng bộ WebSocket** để tránh trộn frame dữ liệu

---

### ⚠️ Giới hạn công nghệ có chủ đích

- Không triển khai qua Internet, chỉ trong LAN
- Không can thiệp kernel, không cài driver
- Không giải mã dữ liệu nhạy cảm (cookie, mật khẩu)
- Không sử dụng cơ chế bypass bảo mật hệ điều hành

---

## 📡 API Reference

Tài liệu mô tả các WebSocket message được gửi từ Web Client → Gateway → Agent.
Tất cả nội dung bên dưới được trích trực tiếp từ code frontend (sendJson).

🔌 WebSocket Messages (Client → Gateway → Agent)

--- 


📁 FILE Module
List root / directory
```json
// List drives or root directory
{
  "module": "FILE",
  "command": "LIST_DIR",
  "payload": {
    "path": ""
  }
}
```
```json
// List directory
{
  "module": "FILE",
  "command": "LIST_DIR",
  "payload": {
    "path": "C:\\Users"
  }
}
```

Execute file
```json
// Execute file on agent
{
  "module": "FILE",
  "command": "EXECUTE",
  "payload": {
    "path": "C:\\path\\to\\file.exe"
  }
}
```
Read / Write text file
```json
// Read text file (editor)
{
  "module": "FILE",
  "command": "READ_TEXT",
  "payload": {
    "path": "C:\\file.txt"
  }
}
```
```json
// Write text file
{
  "module": "FILE",
  "command": "WRITE_TEXT",
  "payload": {
    "path": "C:\\file.txt",
    "content": "file content"
  }
}
```
---


📁 Gallery
```json
// List gallery files
{
  "module": "FILE",
  "command": "LIST"
}
```
```json
// Get gallery file
{
  "module": "FILE",
  "command": "GET",
  "payload": {
    "name": "image.png"
  }
}
```
Save webcam video
```json
// Save recorded webcam video
{
  "module": "FILE",
  "command": "SAVE_VIDEO",
  "payload": {
    "name": "cam_1690000000000.webm",
    "data": "<base64>"
  }
}
```
---


⚙ PROCESS Module
```json
// List processes
{
  "module": "PROCESS",
  "command": "LIST"
}
```
```json
// Start process
{
  "module": "PROCESS",
  "command": "START",
  "payload": {
    "path": "app.exe",
    "args": ""
  }
}
```
```json
// Kill process
{
  "module": "PROCESS",
  "command": "KILL",
  "pid": 1234
}
```
---


🖥 SYSTEM Module
```json
// Shutdown system
{
  "module": "SYSTEM",
  "command": "SHUTDOWN"
}
```
```json
// Restart system
{
  "module": "SYSTEM",
  "command": "RESTART"
}
```
```json
// Logoff system
{
  "module": "SYSTEM",
  "command": "LOGOFF"
}
```
```json
// Lock system
{
  "module": "SYSTEM",
  "command": "LOCK"
}
```
---


🖼 SCREEN Module
```json
// Capture screenshot (binary)
{
  "module": "SCREEN",
  "command": "CAPTURE_BINARY"
}
```
```json
// Capture screenshot for remote control
{
  "module": "SCREEN",
  "command": "CAPTURE_BINARY",
  "payload": {
    "save": false
  }
}
```
---


🖱 INPUT Module (Remote Control)
```json
// Mouse move
{
  "module": "INPUT",
  "command": "MOUSE_MOVE",
  "payload": {
    "x": 0.5,
    "y": 0.4
  }
}
```
```json
// Mouse button
{
  "module": "INPUT",
  "command": "MOUSE_BTN",
  "payload": {
    "x": 0.5,
    "y": 0.4,
    "type": "down",
    "btn": "left"
  }
}
```
```json
// Mouse wheel
{
  "module": "INPUT",
  "command": "MOUSE_WHEEL",
  "payload": {
    "delta": 120
  }
}
```
```json
// Keyboard event
{
  "module": "INPUT",
  "command": "KEY_EVENT",
  "payload": {
    "key": 65,
    "type": "down"
  }
}
```
---


🎥 WEBCAM Module
```json
// Start webcam stream
{
  "module": "WEBCAM",
  "command": "START_STREAM"
}
```
```json
// Stop webcam stream
{
  "module": "WEBCAM",
  "command": "STOP_STREAM"
}
```
---


⌨ KEYBOARD Module
```json
// Start keylogger
{
  "module": "KEYBOARD",
  "command": "START"
}
```
```json
// Stop keylogger
{
  "module": "KEYBOARD",
  "command": "STOP"
}
```
```json
// Get keylogger log
{
  "module": "KEYBOARD",
  "command": "GET_LOG"
}
```
```json
// Lock keyboard
{
  "module": "KEYBOARD",
  "command": "LOCK"
}
```
```json
// Unlock keyboard
{
  "module": "KEYBOARD",
  "command": "UNLOCK"
}
```
---


📱 APP Module
```json
// List running apps
{
  "module": "APP",
  "command": "LIST"
}
```
```json
// Start app
{
  "module": "APP",
  "command": "START",
  "payload": {
    "path": "app.exe"
  }
}
```
```json
// Kill app
{
  "module": "APP",
  "command": "KILL",
  "payload": {
    "name": "app.exe"
  }
}
```



---


🌐 EDGE Module
```json
// Get Edge passwords
{
  "module": "EDGE",
  "command": "GET_PASSWORDS"
}
```
```json
// Get Edge cookies
{
  "module": "EDGE",
  "command": "GET_COOKIES"
}
```
```json
// Get Edge history
{
  "module": "EDGE",
  "command": "GET_HISTORY"
}
```
```json
// Get Edge bookmarks
{
  "module": "EDGE",
  "command": "GET_BOOKMARKS"
}
```
```json
// Get Edge credit cards
{
  "module": "EDGE",
  "command": "GET_CREDIT_CARDS"
}
```


---



## 🔐 Giao thức truyền thông

---

Agent ↔ Gateway (TCP / WebSocket)

```text
TCP / WebSocket Communication

- Transport: TCP
- Protocol: WebSocket (full-duplex)
- Data format:
  - JSON (control / command / metadata)
  - Binary (image, video frames)
- Scope: Local Area Network (LAN)

```
Luồng giao tiếp
```text
Client (Web Browser)
        │
        │ WebSocket (JSON / Binary)
        ▼
Gateway / Server (C++)

```
Sau khi phát hiện server trong LAN, client thiết lập WebSocket session.

WebSocket được sử dụng cho các thao tác realtime như:

 🔹Điều khiển hệ thống

 🔹Truy vấn trạng thái

 🔹Screenshot, webcam, remote control

Hệ thống tách biệt Control path và Data path:

 🔹Control path: JSON (lệnh, trạng thái)

 🔹Data path: Binary frame (ảnh, video)

📡 UDP Beacon (Auto Discovery)
```text
UDP Broadcast Discovery

- Transport: UDP
- Scope: LAN only
- Purpose: Auto-discover server without manual IP input
```

 🔹Client gửi gói UDP broadcast trong subnet LAN.

 🔹Server đang hoạt động sẽ phản hồi thông tin kết nối.

 🔹UDP chỉ dùng cho discovery, không dùng cho điều khiển hay dữ liệu nhạy cảm.


---


## 🔒 Bảo mật

### Các cơ chế bảo mật được triển khai

| Nội dung | Mô tả |
|--------|-----|
| Phạm vi mạng | Hệ thống chỉ hoạt động trong LAN |
| WebSocket | Kết nối hai chiều, kiểm soát theo phiên |
| Control / Data separation | Phân tách JSON điều khiển và dữ liệu nhị phân |
| Quyền hệ điều hành | Phụ thuộc quyền của tiến trình server |
| Dữ liệu trình duyệt | Chỉ truy xuất Bookmark & History (không nhạy cảm) |

### Giới hạn bảo mật có chủ đích

- Không bypass cơ chế bảo mật của hệ điều hành
- Không hook kernel hoặc cài driver
- Không trích xuất mật khẩu hoặc cookie trình duyệt

Những giới hạn này nhằm đảm bảo hệ thống phù hợp mục tiêu học tập,
tránh vi phạm nguyên tắc an toàn và đạo đức.


--


## 🛠 Troubleshooting

### Không kết nối được WebSocket

```text
- Kiểm tra server đang chạy
- Kiểm tra kết nối mạng LAN
- Kiểm tra client và server cùng subnet
```

Mất kết nối khi đang sử dụng

Nguyên nhân:

 🔹Mạng LAN không ổn định

 🔹Server restart

 🔹Client đóng tab

Khắc phục:

 🔹Server tự động dừng các luồng nền khi disconnect

 🔹Client hỗ trợ reconnect và thông báo trạng thái

Lỗi khi chạy trên máy khác

Nguyên nhân:

 🔹Thiếu thư viện hệ thống (DLL / shared libs)

Khắc phục:

 🔹Đóng gói thư viện cần thiết

 🔹Chuẩn hóa thư mục release (binary + config)

Lỗi khi stream bị lag

 🔹Giảm FPS

 🔹Giảm chất lượng encode

 🔹Ưu tiên drop frame để giữ realtime


---


## 🤝 Đóng góp

Chúng tôi hoan nghênh mọi đóng góp cho dự án.
```text
1. Fork repository
2. Tạo branch mới
3. Commit thay đổi
4. Push branch
5. Tạo Pull Request
```
Coding conventions

 🔹C++: tuân theo nguyên tắc lập trình rõ ràng, module hóa

 🔹Web Client: tách module, dễ bảo trì

 🔹Commit: rõ ràng, có ý nghĩa


---


## 👥 Tác giả

**Đồ án môn Mạng Máy Tính – CSC10008**

| Thành viên | MSSV | Vai trò |
|----------|------|-------|
| Ngô Viết Thanh Bình | 24120269 | Server (Windows), báo cáo, kiến trúc |
| Nguyễn Đức Lãm | 24120083 | Server (Linux), kiểm thử |
| Nguyễn Ngọc Phúc | 24120214 | Web Client, UDP discovery, Registry Server |


---


## 📄 Giấy phép

Dự án không công bố giấy phép phần mềm riêng.
Mã nguồn được sử dụng phục vụ mục đích học tập và nghiên cứu trong khuôn khổ môn học.


---


## ⚠️ Disclaimer

**CẢNH BÁO PHÁP LÝ**

Phần mềm này được phát triển **chỉ cho mục đích học tập và nghiên cứu**
trong môi trường có kiểm soát.

❌ **NGHIÊM CẤM sử dụng để:**
- Truy cập trái phép vào hệ thống máy tính của người khác
- Thu thập dữ liệu cá nhân khi không có sự đồng ý
- Bất kỳ hành vi vi phạm pháp luật nào

✅ **CHỈ SỬ DỤNG cho:**
- Máy tính cá nhân
- Hệ thống được cho phép kiểm tra
- Môi trường học tập, phòng lab

Người sử dụng **chịu hoàn toàn trách nhiệm**
về việc sử dụng phần mềm này.
