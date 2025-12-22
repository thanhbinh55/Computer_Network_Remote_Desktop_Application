# 🖥 Hệ thống Điều khiển Máy tính Trong LAN bằng WebSocket

## 📌 Giới thiệu
Đây là hệ thống **điều khiển máy tính trong mạng LAN theo thời gian thực**, cho phép người dùng:
- Giám sát máy tính từ xa
- Thao tác điều khiển trực tiếp
- Truy xuất và theo dõi hoạt động hệ thống

Hệ thống được thiết kế theo mô hình **Client – Server – Registry**:

- 🖥 **Remote Server (C++ – Windows / Linux)**  
  Chạy trên máy bị điều khiển, thực thi lệnh và stream dữ liệu.

- 🌐 **Web Client (Angular)**  
  Giao diện điều khiển trực quan trên trình duyệt.

- 🛰 **Registry & Discovery Server (Node.js)**  
  Hỗ trợ tự động phát hiện server trong LAN bằng UDP.

Hệ thống sử dụng:
- WebSocket (TCP) → Giao tiếp hai chiều realtime  
- UDP Broadcast → Tự động phát hiện server  
- JSON + Binary Frame → Tách control channel & data channel  

---

## 🚀 Chức năng

### 🖥 Nhóm điều khiển & giám sát hệ thống
✔ Process Manager (liệt kê / kill / start process)  
✔ Application Manager  
✔ File Manager (có giới hạn bảo mật)  
✔ Shutdown / Restart hệ thống  

---

### 🎥 Nhóm realtime
✔ Live Screenshot (Remote Desktop Preview)  
✔ Webcam Streaming  
✔ Keylogger  
✔ Điều khiển chuột & bàn phím từ xa  
✔ Mouse move / click / scroll  
✔ Keyboard events  

> 🔒 Hệ thống đảm bảo không truy cập dữ liệu nhạy cảm, tuân thủ cơ chế bảo mật OS.

---

## 🌍 Hỗ trợ đa nền tảng

| Thành phần | Trạng thái |
|-----------|----------|
| Windows Remote Server (C++) | ✅ |
| Linux Remote Server (C++) | ✅ |
| Angular Web Client | ✅ |
| Registry + Discovery Server | ✅ |

---

## 🏗 Kiến trúc tổng thể
Client (Angular) <— WebSocket —> C++ Remote Server
│
│ (UDP + HTTP)
▼
Registry Server (NodeJS)


---

## 📦 Công nghệ sử dụng

### 🔧 Remote Server
- C++
- Boost / Beast (WebSocket)
- X11 + XTest (Linux input)
- Win32 API (Windows)
- JSON

### 🌐 Web Client
- Angular
- WebSocket Client

### 🛰 Registry Server
- Node.js
- UDP Broadcast
- WebSocket Gateway

---

## ⚙️ Cài đặt & Chạy


### 🛰 1️⃣ Registry Server (Node.js)

```bash
cd registry_server
npm install
node server.js
```

### 🐧 2️⃣ Remote Server – Linux
- Cài dependency
```bash
sudo apt install libx11-dev libxtst-dev
```
- Build & chạy
```bash
mkdir build
cd build
cmake ..
make
./linux/server
```

### 🪟 3️⃣ Remote Server – Windows
Yêu cầu:
- Visual Studio 2022
- CMake
- vcpkg

```bash
mkdir build
cd build
cmake ..
cmake --build .
server.exe
```

### 🌐 4️⃣ Web Client
```bash
cd web-client
npm install
npm start
```

- Truy cập:
```bash
http://localhost:4200
```
