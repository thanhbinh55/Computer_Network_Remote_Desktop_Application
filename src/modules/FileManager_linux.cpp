#include "FileManager.hpp"

namespace fs = std::filesystem;

// --- HELPER: BASE64 DECODE (Giữ nguyên) ---
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
json FileManager::list_dir(const json& request) {
    json file_list = json::array();
    std::string req_path = "";
    if (request.contains("payload") && request["payload"].contains("path")) {
        req_path = request["payload"]["path"];
    }

    // Nếu path rỗng -> Trả về Root Directory
    if (req_path.empty()) {
        req_path = "/"; 
    }

    try {
        if (fs::exists(req_path) && fs::is_directory(req_path)) {
            for (const auto& entry : fs::directory_iterator(req_path)) {
                try {
                    std::string filename = entry.path().filename().string();
                    // Bỏ qua các file ẩn (bắt đầu bằng dấu chấm) trên Linux nếu muốn
                    // if (filename[0] == '.') continue; 

                    std::string type = entry.is_directory() ? "dir" : "file";
                    
                    // Lấy size an toàn (file hệ thống có thể gây lỗi permission)
                    uintmax_t size = 0;
                    if (!entry.is_directory()) {
                        std::error_code ec;
                        size = entry.file_size(ec); 
                        if (ec) size = 0; // Nếu lỗi permission thì để size = 0
                    }
                    
                    // Detect extension
                    std::string ext = entry.path().extension().string();
                    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                    if(ext == ".webm" || ext == ".mp4" || ext == ".mkv") type = "video";
                    if(ext == ".jpg" || ext == ".png" || ext == ".jpeg") type = "image";

                    file_list.push_back({
                        {"name", filename}, {"type", type}, {"size", size}, {"path", entry.path().string()}
                    });
                } catch (...) { continue; }
            }
            return {{"status", "success"}, {"module", "FILE"}, {"command", "LIST_DIR"}, 
                    {"current_path", fs::absolute(req_path).string()}, {"data", file_list}};
        }
    } catch (const std::exception& e) { return {{"status", "error"}, {"message", e.what()}}; }
    
    return json::array();
}

json FileManager::read_file(const json& request) {
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
            {"content", buffer.str()}
        };
    }
    return {{"status", "error"}, {"message", "Cannot open file"}};

}

json FileManager::write_file(const json &request) {
    std::string path = request["payload"].value("path", "");
    std::string content = request["payload"].value("content", "");
    
    std::ofstream file(path, std::ios::trunc);
    if (file.is_open()) {
        file << content;
        return {{"status", "success"}, {"message", "File saved successfully"}};
    }
    // Linux rất chặt về quyền (Permission), lỗi này thường xảy ra nếu ghi vào thư mục hệ thống
    return {{"status", "error"}, {"message", "Cannot write file (Permission Denied?)"}};
}

json FileManager::execute_file(const json &request) {
    std::string path = request["payload"].value("path", "");
    if (path.empty()) return {{"status", "error"}, {"message", "Missing path"}};

       std::string cmd_linux = "xdg-open \"" + path + "\" > /dev/null 2>&1 &";
        int res = system(cmd_linux.c_str());
        
        if (res == 0) {
             return {{"status", "success"}, {"message", "Executed command"}};
        }
    return {{"status", "error"}, {"message", "Failed to execute"}};
}

json FileManager::delete_item(const json &request) {
    std::string path = request["payload"].value("path", "");
    try {
        std::error_code ec;
        if (fs::remove_all(path, ec) > 0 || !ec) // Check error code để tránh throw exception
            return {{"status", "success"}, {"message", "Deleted"}};
    } catch (...) {}
    return {{"status", "error"}, {"message", "Delete failed"}};
}

json FileManager::list() {
    json file_list = json::array();
    std::string path = "captured_data";
    
    // Tạo thư mục nếu chưa có (kèm quyền đọc ghi 0777 cho Linux)
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

json FileManager::save_video(const json &request) {
    if (request.contains("payload")) {
        std::string name = request["payload"].value("name", "video.webm");
        std::string b64Data = request["payload"].value("data", "");
        
        if (!b64Data.empty()) {
            std::vector<unsigned char> binaryData = base64_decode(b64Data);
            
            // Đảm bảo thư mục tồn tại
            if (!fs::exists("captured_data")) fs::create_directory("captured_data");

            std::string path = "captured_data/" + name;
            std::ofstream file(path, std::ios::binary);
            if (file.is_open()) {
                file.write(reinterpret_cast<const char*>(binaryData.data()), binaryData.size());
                file.close();
                std::cout << "[FILE] Video saved: " << path << "\n";
                return {{"status", "success"}, {"message", "Video saved successfully"}};
            }
        }
    }
    return {{"status", "error"}, {"message", "Failed to save video"}};
}

json FileManager::handle_command(const json& request) {
    std::string cmd = request.value("command", "");
    
    // 1. LIST_DIR (Liệt kê file/thư mục)
    if (cmd == "LIST_DIR") {
        return list_dir(request);
    }

    // 2. READ_TEXT
    else if (cmd == "READ_TEXT") {
        return read_file(request);
    }

    // 3. WRITE_TEXT
    else if (cmd == "WRITE_TEXT") {
        return write_file(request);
    }

    // 4. EXECUTE 
    else if (cmd == "EXECUTE") {
        return execute_file(request);
    }

    // 5. DELETE
    else if (cmd == "DELETE") {
        return delete_item(request);
    }

    // 6. LIST (Dành cho captured_data)
    else if (cmd == "LIST") {
        return list();
    }
    
    // 7. SAVE_VIDEO
    else if (cmd == "SAVE_VIDEO") {
        return save_video(request);
    }

    return {{"status", "ok"}}; 
}

// Giữ nguyên hàm read_file_binary
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
