#pragma once

#include <chrono>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <unordered_map>

namespace process_monitor {

struct FileInfo {
    std::string content;
    std::filesystem::file_time_type last_modified;
    bool has_changed;
};

class FileWatcher {
public:
    FileWatcher() = default;

    std::optional<FileInfo> get_file_info(const std::string& path) {
        if (path.empty()) {
            return std::nullopt;
        }

        std::error_code ec;
        if (!std::filesystem::exists(path, ec)) {
            return std::nullopt;
        }

        auto last_modified = std::filesystem::last_write_time(path, ec);
        if (ec) {
            return std::nullopt;
        }

        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) {
            return std::nullopt;
        }

        std::string content((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());

        bool has_changed = false;
        auto it = cache_.find(path);
        if (it != cache_.end()) {
            has_changed = (it->second.last_modified != last_modified);
        }

        FileInfo info{content, last_modified, has_changed || it == cache_.end()};
        cache_[path] = info;

        return info;
    }

    void clear_cache() {
        cache_.clear();
    }

private:
    std::unordered_map<std::string, FileInfo> cache_;
};

}