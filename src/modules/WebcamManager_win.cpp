#include "WebcamManager.hpp"
#include <fstream>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <direct.h> // _mkdir
#include <chrono>   // Để đo thời gian lưu file
#include <iostream>

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")

using namespace Gdiplus;
using namespace std;

// --- HELPER: GDI+ Encoder (Static) ---
static int GetEncoderClsid(const WCHAR* format, CLSID* pClsid) {
UINT  num = 0; UINT  size = 0;
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

// --- HELPER: GDI+ Init ---
struct GdiPlusInitWebcam {
    ULONG_PTR gdiplusToken;
    GdiPlusInitWebcam() { GdiplusStartupInput g; GdiplusStartup(&gdiplusToken, &g, NULL); }
    ~GdiPlusInitWebcam() { GdiplusShutdown(gdiplusToken); }
};
static GdiPlusInitWebcam init;

static void SaveWebcamFrame(const std::vector<uint8_t>& data) {
    _mkdir("captured_data");
    
    auto t = std::time(nullptr);
    auto tm = *std::localtime(&t);
    
    std::ostringstream oss;
    oss << "captured_data/cam_" << std::put_time(&tm, "%Y%m%d_%H%M%S") << ".jpg";
    std::string filename = oss.str();

    std::ofstream file(filename, std::ios::binary);
    if (file.is_open()) {
        file.write(reinterpret_cast<const char*>(data.data()), data.size());
        file.close();
        std::cout << "[WEBCAM] Saved snapshot: " << filename << std::endl;
    }
}

void WebcamManager::stop_stream() {
    running_ = false;
    if (stream_thread_.joinable()) stream_thread_.join();
}

void WebcamManager::start_stream(StreamCallback callback) {
    stop_stream(); 
    running_ = true;

    stream_thread_ = std::thread([this, callback]() {
        HRESULT hr = S_OK;
        hr = MFStartup(MF_VERSION);
        if (FAILED(hr)) return;

        // ... (Giữ nguyên phần khởi tạo MFCreateAttributes, tìm thiết bị, tạo Reader như cũ) ...
        // ... (Để tiết kiệm không gian hiển thị, tôi không paste lại đoạn init dài dòng đó, 
        //      bạn hãy giữ nguyên đoạn code từ dòng 55 đến dòng 110 trong file gốc của bạn) ...
        
        // (Đây là đoạn code giả lập phần init webcam bạn đang có - HÃY GIỮ NGUYÊN CODE CŨ CỦA BẠN ĐẾN TRƯỚC VÒNG LẶP WHILE)
        IMFAttributes* pAttributes = NULL; IMFActivate** ppDevices = NULL; UINT32 count = 0;
        IMFMediaSource* pSource = NULL; IMFSourceReader* pReader = NULL;
        MFCreateAttributes(&pAttributes, 1);
        pAttributes->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);
        MFEnumDeviceSources(pAttributes, &ppDevices, &count);
        if (count > 0) ppDevices[0]->ActivateObject(IID_PPV_ARGS(&pSource));
        pAttributes->Release(); for(UINT32 i=0; i<count; i++) ppDevices[i]->Release(); CoTaskMemFree(ppDevices);
        if (!pSource) return;

        IMFAttributes* pReaderAttributes = NULL;
        MFCreateAttributes(&pReaderAttributes, 1);
        pReaderAttributes->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, TRUE);
        MFCreateSourceReaderFromMediaSource(pSource, pReaderAttributes, &pReader);
        if(pReaderAttributes) pReaderAttributes->Release();

        IMFMediaType* pType = NULL; MFCreateMediaType(&pType);
        pType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
        pType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
        pReader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, NULL, pType);
        pType->Release();
        // (Kết thúc phần init giả lập)

        CLSID jpgClsid;
        GetEncoderClsid(L"image/jpeg", &jpgClsid);
        EncoderParameters encoderParameters;
        encoderParameters.Count = 1;
        encoderParameters.Parameter[0].Guid = EncoderQuality;
        encoderParameters.Parameter[0].Type = EncoderParameterValueTypeLong;
        encoderParameters.Parameter[0].NumberOfValues = 1;
        ULONG quality = 50; 
        encoderParameters.Parameter[0].Value = &quality;

        std::cout << "[WEBCAM] Streaming started...\n";

        // [MỚI] BIẾN ĐẾM THỜI GIAN ĐỂ LƯU FILE
        auto last_save_time = std::chrono::steady_clock::now();

        while (running_) {
            IMFSample* pSample = NULL;
            DWORD streamIndex, flags;
            LONGLONG llTimeStamp;

            hr = pReader->ReadSample(MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, &streamIndex, &flags, &llTimeStamp, &pSample);
            if (FAILED(hr)) break;

            if (pSample) {
                IMFMediaBuffer* pBuffer = NULL;
                pSample->ConvertToContiguousBuffer(&pBuffer);
                
                BYTE* pBitmapData = NULL;
                DWORD maxLength = 0, currentLength = 0;
                
                if (SUCCEEDED(pBuffer->Lock(&pBitmapData, &maxLength, &currentLength))) {
                    IMFMediaType* pCurrentType = NULL;
                    pReader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, &pCurrentType);
                    UINT32 width = 0, height = 0;
                    MFGetAttributeSize(pCurrentType, MF_MT_FRAME_SIZE, &width, &height);
                    
                    Bitmap bmp(width, height, width * 4, PixelFormat32bppRGB, pBitmapData);
                    IStream* pStream = NULL;
                    if (CreateStreamOnHGlobal(NULL, TRUE, &pStream) == S_OK) {
                        if (bmp.Save(pStream, &jpgClsid, &encoderParameters) == Ok) {
                            STATSTG stg;
                            pStream->Stat(&stg, STATFLAG_NONAME);
                            ULONG sz = stg.cbSize.LowPart;
                            std::vector<uint8_t> jpgData(sz);
                            LARGE_INTEGER seekPos = {0};
                            pStream->Seek(seekPos, STREAM_SEEK_SET, NULL);
                            ULONG bytesRead;
                            pStream->Read(jpgData.data(), sz, &bytesRead);

                            // 1. Gửi về Client (như cũ)
                            if (callback) callback(jpgData);

                            // 2. [MỚI] Kiểm tra thời gian để lưu file (Mỗi 5 giây lưu 1 lần)
                            auto now = std::chrono::steady_clock::now();
                            if (std::chrono::duration_cast<std::chrono::seconds>(now - last_save_time).count() >= 5) {
                                // SaveWebcamFrame(jpgData);
                                last_save_time = now; // Cập nhật thời gian lưu cuối cùng
                            }
                        }
                        pStream->Release();
                    }
                    pCurrentType->Release();
                    pBuffer->Unlock();
                }
                pBuffer->Release();
                pSample->Release();
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(40));
        }

        if(pSource) pSource->Release();
        if(pReader) pReader->Release();
        MFShutdown();
    });
}