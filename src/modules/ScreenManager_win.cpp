#include "ScreenManager.hpp"

#include <vector>
#include <memory>
#include <iostream>

// Link thư viện GDI+ (Chỉ hoạt động với MSVC, nếu dùng MinGW cần thêm trong CMakeLists.txt)
#pragma comment (lib,"Gdiplus.lib")

using namespace Gdiplus;

// Helper để lấy Encoder ID cho JPEG
static int GetEncoderClsid(const WCHAR* format, CLSID* pClsid) {
    UINT  num = 0;          // number of image encoders
    UINT  size = 0;         // size of the image encoder array in bytes

    GetImageEncodersSize(&num, &size);
    if (size == 0) return -1;

    auto pImageCodecInfo = (ImageCodecInfo*)(malloc(size));
    if (pImageCodecInfo == NULL) return -1;

    GetImageEncoders(num, size, pImageCodecInfo);

    for (UINT j = 0; j < num; ++j) {
        if (wcscmp(pImageCodecInfo[j].MimeType, format) == 0) {
            *pClsid = pImageCodecInfo[j].Clsid;
            free(pImageCodecInfo);
            return j;
        }
    }

    free(pImageCodecInfo);
    return -1;
}

// Hàm khởi tạo/hủy GDI+ (Singleton đơn giản)
struct GdiPlusInit {
    ULONG_PTR gdiplusToken;
    GdiPlusInit() {
        GdiplusStartupInput gdiplusStartupInput;
        GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);
    }
    ~GdiPlusInit() {
        GdiplusShutdown(gdiplusToken);
    }
};

// Biến static để khởi động GDI+ 1 lần duy nhất khi chương trình chạy
static GdiPlusInit init;

// === CAPTURE SCREEN TO JPEG BUFFER ===
bool ScreenManager::capture_screen_data(std::vector<uint8_t>& out_buffer, std::string& error_msg) {
    error_msg.clear();

    int width = GetSystemMetrics(SM_CXSCREEN);
    int height = GetSystemMetrics(SM_CYSCREEN);

    HDC hScreenDC = GetDC(NULL);
    HDC hMemoryDC = CreateCompatibleDC(hScreenDC);
    HBITMAP hBitmap = CreateCompatibleBitmap(hScreenDC, width, height);
    HGDIOBJ hOldBitmap = SelectObject(hMemoryDC, hBitmap);

    // 1. Chụp màn hình vào Bitmap
    if (!BitBlt(hMemoryDC, 0, 0, width, height, hScreenDC, 0, 0, SRCCOPY)) {
        error_msg = "BitBlt failed";
        return false;
    }

    // 2. Dùng GDI+ để save Bitmap sang Stream (JPEG format)
    Bitmap* bitmap = Bitmap::FromHBITMAP(hBitmap, NULL);
    
    IStream* stream = NULL;
    if (CreateStreamOnHGlobal(NULL, TRUE, &stream) != S_OK) {
        error_msg = "CreateStreamOnHGlobal failed";
        delete bitmap; return false;
    }

    CLSID jpgClsid;
    GetEncoderClsid(L"image/jpeg", &jpgClsid);

    // Chất lượng ảnh JPEG (50 để nhanh, 100 để đẹp)
    EncoderParameters encoderParameters;
    encoderParameters.Count = 1;
    encoderParameters.Parameter[0].Guid = EncoderQuality;
    encoderParameters.Parameter[0].Type = EncoderParameterValueTypeLong;
    encoderParameters.Parameter[0].NumberOfValues = 1;
    ULONG quality = 60; // Giảm xuống 60 để tối ưu tốc độ mạng
    encoderParameters.Parameter[0].Value = &quality;

    Status stat = bitmap->Save(stream, &jpgClsid, &encoderParameters);
    
    if (stat == Ok) {
        // 3. Copy từ Stream ra Vector
        STATSTG stg;
        stream->Stat(&stg, STATFLAG_NONAME);
        ULONG streamSize = stg.cbSize.LowPart;
        
        out_buffer.resize(streamSize);
        
        LARGE_INTEGER seekPos; seekPos.QuadPart = 0;
        stream->Seek(seekPos, STREAM_SEEK_SET, NULL);
        
        ULONG bytesRead;
        stream->Read(out_buffer.data(), streamSize, &bytesRead);
    } else {
        error_msg = "GDI+ Save Failed";
    }

    // Cleanup
    stream->Release();
    delete bitmap;
    SelectObject(hMemoryDC, hOldBitmap);
    DeleteObject(hBitmap);
    DeleteDC(hMemoryDC);
    ReleaseDC(NULL, hScreenDC);

    return (stat == Ok);
}

// Xử lý lệnh (chỉ dùng cho các lệnh Text, lệnh Binary xử lý ở main)
json ScreenManager::handle_command(const json& request) {
    // Chúng ta sẽ xử lý capture binary ở main.cpp để truy cập socket trực tiếp
    // Hàm này chỉ trả về OK để báo hiệu nếu cần.
    return { {"status", "ok"} };
<<<<<<< HEAD
}
=======
}
>>>>>>> master
