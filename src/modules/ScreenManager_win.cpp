// src/modules/ScreenManager.cpp
#include "ScreenManager.hpp"
#include <windows.h>
#include <cstring>   // memcpy
#include <stdexcept>

// ====================== Base64 ENCODE ======================

static const char* BASE64_CHARS =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/";

std::string ScreenManager::base64_encode(const std::vector<uint8_t>& data) {
    std::string ret;
    if (data.empty()) return ret;

    size_t len = data.size();
    ret.reserve(((len + 2) / 3) * 4);

    size_t i = 0;

    // Encode từng block 3 byte
    while (i + 3 <= len) {
        uint32_t octet_a = data[i++];
        uint32_t octet_b = data[i++];
        uint32_t octet_c = data[i++];

        uint32_t triple = (octet_a << 16) | (octet_b << 8) | octet_c;

        ret.push_back(BASE64_CHARS[(triple >> 18) & 0x3F]);
        ret.push_back(BASE64_CHARS[(triple >> 12) & 0x3F]);
        ret.push_back(BASE64_CHARS[(triple >> 6)  & 0x3F]);
        ret.push_back(BASE64_CHARS[ triple        & 0x3F]);
    }

    // Xử lý phần dư 1 hoặc 2 byte
    size_t rem = len - i;
    if (rem == 1) {
        uint32_t octet_a = data[i];
        uint32_t triple  = (octet_a << 16);

        ret.push_back(BASE64_CHARS[(triple >> 18) & 0x3F]);
        ret.push_back(BASE64_CHARS[(triple >> 12) & 0x3F]);
        ret.push_back('=');
        ret.push_back('=');
    } else if (rem == 2) {
        uint32_t octet_a = data[i];
        uint32_t octet_b = data[i + 1];
        uint32_t triple  = (octet_a << 16) | (octet_b << 8);

        ret.push_back(BASE64_CHARS[(triple >> 18) & 0x3F]);
        ret.push_back(BASE64_CHARS[(triple >> 12) & 0x3F]);
        ret.push_back(BASE64_CHARS[(triple >> 6)  & 0x3F]);
        ret.push_back('=');
    }

    return ret;
}

// ====================== Capture Screen BMP ======================

bool ScreenManager::capture_screen_bmp(std::vector<uint8_t>& out_bmp,
                                      int& width,
                                      int& height,
                                      std::string& error_msg)
{
    error_msg.clear();

    // 1. Lấy kích thước màn hình
    int screenX = GetSystemMetrics(SM_CXSCREEN);
    int screenY = GetSystemMetrics(SM_CYSCREEN);

    if (screenX <= 0 || screenY <= 0) {
        error_msg = "GetSystemMetrics failed.";
        return false;
    }

    width  = screenX;
    height = screenY;

    // 2. DC màn hình và DC bộ nhớ
    HDC hScreenDC  = GetDC(nullptr);
    if (!hScreenDC) {
        error_msg = "GetDC(NULL) failed.";
        return false;
    }

    HDC hMemoryDC = CreateCompatibleDC(hScreenDC);
    if (!hMemoryDC) {
        ReleaseDC(nullptr, hScreenDC);
        error_msg = "CreateCompatibleDC failed.";
        return false;
    }

    // 3. Tạo bitmap tương thích
    HBITMAP hBitmap = CreateCompatibleBitmap(hScreenDC, screenX, screenY);
    if (!hBitmap) {
        DeleteDC(hMemoryDC);
        ReleaseDC(nullptr, hScreenDC);
        error_msg = "CreateCompatibleBitmap failed.";
        return false;
    }

    // Select bitmap vào memory DC
    HGDIOBJ oldObj = SelectObject(hMemoryDC, hBitmap);

    // 4. Copy từ màn hình vào hMemoryDC
    if (!BitBlt(hMemoryDC, 0, 0, screenX, screenY, hScreenDC, 0, 0, SRCCOPY)) {
        SelectObject(hMemoryDC, oldObj);
        DeleteObject(hBitmap);
        DeleteDC(hMemoryDC);
        ReleaseDC(nullptr, hScreenDC);
        error_msg = "BitBlt failed.";
        return false;
    }

    BITMAP bmpScreen{};
    if (!GetObject(hBitmap, sizeof(BITMAP), &bmpScreen)) {
        SelectObject(hMemoryDC, oldObj);
        DeleteObject(hBitmap);
        DeleteDC(hMemoryDC);
        ReleaseDC(nullptr, hScreenDC);
        error_msg = "GetObject(BITMAP) failed.";
        return false;
    }

    // 5. Chuẩn bị header BMP
    BITMAPFILEHEADER bmfHeader{};
    BITMAPINFOHEADER bi{};

    bi.biSize          = sizeof(BITMAPINFOHEADER);
    bi.biWidth         = bmpScreen.bmWidth;
    bi.biHeight        = bmpScreen.bmHeight;
    bi.biPlanes        = 1;
    bi.biBitCount      = 32;
    bi.biCompression   = BI_RGB;
    bi.biSizeImage     = 0;
    bi.biXPelsPerMeter = 0;
    bi.biYPelsPerMeter = 0;
    bi.biClrUsed       = 0;
    bi.biClrImportant  = 0;

    // Kích thước vùng pixel
    DWORD dwBmpSize = ((bmpScreen.bmWidth * bi.biBitCount + 31) / 32) * 4 * bmpScreen.bmHeight;

    // 6. All-in-one buffer: [FILE_HEADER][INFO_HEADER][PIXELS...]
    out_bmp.resize(sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + dwBmpSize);

    uint8_t* pData = out_bmp.data();

    // FILE HEADER
    bmfHeader.bfType      = 0x4D42; // 'BM'
    bmfHeader.bfOffBits   = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
    bmfHeader.bfSize      = bmfHeader.bfOffBits + dwBmpSize;
    bmfHeader.bfReserved1 = 0;
    bmfHeader.bfReserved2 = 0;

    std::memcpy(pData, &bmfHeader, sizeof(BITMAPFILEHEADER));
    std::memcpy(pData + sizeof(BITMAPFILEHEADER), &bi, sizeof(BITMAPINFOHEADER));

    // 7. Lấy pixel vào buffer
    uint8_t* pPixels = pData + bmfHeader.bfOffBits;

    BITMAPINFO bInfo{};
    bInfo.bmiHeader = bi;

    if (!GetDIBits(hScreenDC,
                   hBitmap,
                   0,
                   (UINT)bmpScreen.bmHeight,
                   pPixels,
                   &bInfo,
                   DIB_RGB_COLORS))
    {
        SelectObject(hMemoryDC, oldObj);
        DeleteObject(hBitmap);
        DeleteDC(hMemoryDC);
        ReleaseDC(nullptr, hScreenDC);
        error_msg = "GetDIBits failed.";
        out_bmp.clear();
        return false;
    }

    // 8. Giải phóng GDI
    SelectObject(hMemoryDC, oldObj);
    DeleteObject(hBitmap);
    DeleteDC(hMemoryDC);
    ReleaseDC(nullptr, hScreenDC);

    return true;
}

// ====================== IRemoteModule API ======================

json ScreenManager::handle_command(const json& request) {
    // JSON từ dispatcher dự kiến:
    // {
    //   "module": "SCREEN",
    //   "command": "CAPTURE",
    //   ...
    // }

    json reply;
    reply["module"] = module_name_;

    // Hỗ trợ cả "command" (đúng chuẩn hiện tại) lẫn "command" cho chắc
    std::string command = request.value("command", "");
    if (command.empty()) {
        command = request.value("command", "");
    }
    reply["command"] = command;   // để frontend JS đọc được response.command

    if (command != "CAPTURE") {
        reply["status"]  = "error";
        reply["message"] = "Unsupported command for SCREEN. Use command = CAPTURE.";
        return reply;
    }

    // Optional: "format": "BMP_BASE64"
    std::string format = request.value("format", "BMP_BASE64");

    int width = 0, height = 0;
    std::vector<uint8_t> bmpBuffer;
    std::string err;

    if (!capture_screen_bmp(bmpBuffer, width, height, err)) {
        reply["status"]  = "error";
        reply["message"] = std::string("capture_screen_bmp failed: ") + err;
        return reply;
    }

    // Encode base64
    std::string encoded = base64_encode(bmpBuffer);

    reply["status"]       = "success";
    reply["format"]       = format;                 // "BMP_BASE64"
    reply["width"]        = width;
    reply["height"]       = height;
    reply["image_base64"] = encoded;

    return reply;
}
