#include "utils/file_reader.hpp"
#include <fstream>
#include <filesystem>

namespace {
    std::string project_root;
}

namespace FileUtils {
    void setProjectRoot(const std::string &root) {
        project_root = std::filesystem::absolute(root).string();
    }

    std::string getProjectPath(const std::string &relative_path) {
        if (project_root.empty()) {
            throw std::runtime_error("Project root not set. Call setProjectRoot() first.");
        }

        return (std::filesystem::path(project_root) / relative_path).string();
    }

    std::string readFile(const std::string &relative_path) {
        std::string full_path = getProjectPath(relative_path);
        std::ifstream file(full_path, std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            throw std::runtime_error("Failed to open file: " + full_path);
        }

        std::string str;
        str.resize(file.tellg());
        file.seekg(0);
        file.read(str.data(), static_cast<std::streamsize>(str.size()));

        return str;
    }

    std::vector<char> readBinaryFile(const std::string &relative_path) {
        std::string full_path = getProjectPath(relative_path);
        std::ifstream file(full_path, std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            throw std::runtime_error("Failed to open file: " + full_path);
        }

        size_t fileSize = static_cast<size_t>(file.tellg());
        std::vector<char> buffer(fileSize);

        file.seekg(0);
        file.read(buffer.data(), fileSize);

        return buffer;
    }
}