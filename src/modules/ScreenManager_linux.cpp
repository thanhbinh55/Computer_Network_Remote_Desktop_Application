#include "ScreenManager.hpp"

json ScreenManager::handle_command(const json& request) {
    // Chúng ta sẽ xử lý capture binary ở main.cpp để truy cập socket trực tiếp
    // Hàm này chỉ trả về OK để báo hiệu nếu cần.
    return { {"status", "ok"} };
}

bool ScreenManager::capture_screen_data(std::vector<uint8_t>& out_buffer, std::string& error_msg, bool save_to_disk) {
    error_msg.clear();

    // 1. Kết nối với X Server
    Display* display = XOpenDisplay(NULL);
    if (!display) {
        error_msg = "Cannot open X Display";
        return false;
    }

    Window root = DefaultRootWindow(display);
    
    // Lấy thông số màn hình
    XWindowAttributes attributes;
    XGetWindowAttributes(display, root, &attributes);
    int width = attributes.width;
    int height = attributes.height;

    // 2. Chụp màn hình
    XImage* img = XGetImage(display, root, 0, 0, width, height, AllPlanes, ZPixmap);
    if (!img) {
        error_msg = "XGetImage failed";
        XCloseDisplay(display);
        return false;
    }

    // 3. Chuẩn bị dữ liệu RGB để nén
    // X11 thường trả về BGRA, libjpeg cần RGB. Ta phải convert thủ công.
    std::vector<uint8_t> rgb_data;
    rgb_data.resize(width * height * 3);

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            unsigned long pixel = XGetPixel(img, x, y);
            
            // Tách màu dựa trên Mask của XImage (để an toàn với mọi loại màn hình)
            uint8_t r = (pixel & img->red_mask) >> 16;  // Thường shift 16
            uint8_t g = (pixel & img->green_mask) >> 8; // Thường shift 8
            uint8_t b = (pixel & img->blue_mask);       // Thường shift 0
            
            // Nếu mask không đúng chuẩn (ví dụ màn hình BGR), code trên cần chỉnh lại shift
            
            int index = (y * width + x) * 3;
            rgb_data[index] = r;
            rgb_data[index + 1] = g;
            rgb_data[index + 2] = b;
        }
    }

    // 4. Nén sang JPEG bằng libjpeg
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);

    // Cấu hình output vào bộ nhớ (Memory Buffer) thay vì File
    // out_buffer sẽ tự động được resize bởi libjpeg
    unsigned char* mem_buffer = NULL;
    unsigned long mem_size = 0;
    jpeg_mem_dest(&cinfo, &mem_buffer, &mem_size); 

    // Cấu hình thông số ảnh
    cinfo.image_width = width;
    cinfo.image_height = height;
    cinfo.input_components = 3; // RGB
    cinfo.in_color_space = JCS_RGB;

    jpeg_set_defaults(&cinfo);
    
    // Đặt chất lượng nén (Quality = 60)
    jpeg_set_quality(&cinfo, 60, TRUE);

    // Bắt đầu nén
    jpeg_start_compress(&cinfo, TRUE);

    int row_stride = width * 3;
    while (cinfo.next_scanline < cinfo.image_height) {
        // Ghi từng dòng
        JSAMPROW row_pointer[1];
        row_pointer[0] = &rgb_data[cinfo.next_scanline * row_stride];
        jpeg_write_scanlines(&cinfo, row_pointer, 1);
    }

    jpeg_finish_compress(&cinfo);
    
    // 5. Copy dữ liệu từ buffer của libjpeg sang vector output
    out_buffer.assign(mem_buffer, mem_buffer + mem_size);

    // --- Lưu file JPEG ra ổ đĩa ---
    FILE* fp = fopen("screenshot.jpg", "wb");
    if (fp) {
        fwrite(out_buffer.data(), 1, out_buffer.size(), fp);
        fclose(fp);
    } else {
        error_msg = "Cannot write screenshot.jpg";
        // Bạn có thể return false tùy nhu cầu
    }

    // Dọn dẹp libjpeg
    if (mem_buffer) free(mem_buffer); // jpeg_mem_dest cấp phát bằng malloc
    jpeg_destroy_compress(&cinfo);

    // Dọn dẹp X11
    XDestroyImage(img); // Hàm này tự free memory của img
    XCloseDisplay(display);

    return true;
}
