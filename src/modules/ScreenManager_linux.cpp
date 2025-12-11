#include "ScreenManager.hpp"

json ScreenManager::handle_command(const json& request) {
    // Chúng ta sẽ xử lý capture binary ở main.cpp để truy cập socket trực tiếp
    // Hàm này chỉ trả về OK để báo hiệu nếu cần.
    return { {"status", "ok"} };
}

namespace fs = std::filesystem;

// --- HELPER: Save to Disk (Giống hệt logic cũ nhưng dùng filesystem chuẩn) ---
static void SaveDataToDisk(const std::vector<uint8_t>& data, const std::string& prefix) {
    // 1. Tạo thư mục nếu chưa có
    // fs::create_directory an toàn hơn _mkdir trên Linux
    if (!fs::exists("captured_data")) {
        fs::create_directory("captured_data");
    }
    
    // 2. Tạo tên file theo thời gian
    auto t = std::time(nullptr);
    auto tm = *std::localtime(&t);
    
    std::ostringstream oss;
    oss << "captured_data/" << prefix << "_" 
        << std::put_time(&tm, "%Y%m%d_%H%M%S") << ".jpg";
    
    std::string filename = oss.str();

    // 3. Ghi file
    std::ofstream file(filename, std::ios::binary);
    if (file.is_open()) {
        file.write(reinterpret_cast<const char*>(data.data()), data.size());
        file.close();
        std::cout << "[STORAGE] Saved: " << filename << std::endl;
    }
}

// === CAPTURE SCREEN TO JPEG BUFFER (LINUX X11) ===
bool ScreenManager::capture_screen_data(std::vector<uint8_t>& out_buffer, std::string& error_msg, bool save_to_disk) {
    error_msg.clear();

    // 1. Kết nối X Server (Thay cho GetDC)
    Display* display = XOpenDisplay(NULL);
    if (!display) {
        error_msg = "Cannot open X Display";
        return false;
    }

    Window root = DefaultRootWindow(display);
    
    XWindowAttributes attributes;
    XGetWindowAttributes(display, root, &attributes);
    int width = attributes.width;
    int height = attributes.height;

    // 2. Chụp màn hình (Lấy dữ liệu Pixel thô - Thay cho BitBlt)
    XImage* img = XGetImage(display, root, 0, 0, width, height, AllPlanes, ZPixmap);
    if (!img) {
        error_msg = "XGetImage failed";
        XCloseDisplay(display);
        return false;
    }

    // 3. Chuẩn bị dữ liệu RGB cho libjpeg
    // X11 trả về BGRA (thường là vậy), libjpeg cần RGB. Phải convert thủ công.
    std::vector<uint8_t> rgb_data;
    rgb_data.resize(width * height * 3);

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            unsigned long pixel = XGetPixel(img, x, y);
            
            // Tách màu (Shift bit)
            uint8_t r = (pixel & img->red_mask) >> 16;
            uint8_t g = (pixel & img->green_mask) >> 8;
            uint8_t b = (pixel & img->blue_mask);

            int index = (y * width + x) * 3;
            rgb_data[index] = r;
            rgb_data[index + 1] = g;
            rgb_data[index + 2] = b;
        }
    }

    // 4. Nén JPEG vào bộ nhớ (Thay cho GDI+ Save stream)
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);

    // Setup output buffer (libjpeg tự malloc, mình sẽ copy ra vector sau)
    unsigned char* mem_buffer = NULL;
    unsigned long mem_size = 0;
    jpeg_mem_dest(&cinfo, &mem_buffer, &mem_size); 

    cinfo.image_width = width;
    cinfo.image_height = height;
    cinfo.input_components = 3;
    cinfo.in_color_space = JCS_RGB;

    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, 60, TRUE); // Quality 60 giống code Windows

    jpeg_start_compress(&cinfo, TRUE);

    int row_stride = width * 3;
    while (cinfo.next_scanline < cinfo.image_height) {
        JSAMPROW row_pointer[1];
        row_pointer[0] = &rgb_data[cinfo.next_scanline * row_stride];
        jpeg_write_scanlines(&cinfo, row_pointer, 1);
    }

    jpeg_finish_compress(&cinfo);
    
    // 5. Copy kết quả nén vào out_buffer
    out_buffer.assign(mem_buffer, mem_buffer + mem_size);

    // 6. Xử lý lưu đĩa (Nếu được yêu cầu)
    if (save_to_disk) {
        SaveDataToDisk(out_buffer, "screen");
    }

    // Dọn dẹp
    if (mem_buffer) free(mem_buffer);
    jpeg_destroy_compress(&cinfo);
    XDestroyImage(img);
    XCloseDisplay(display);

    return true;
}
