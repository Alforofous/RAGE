#pragma once
#include <string>
#include <vector>

namespace FileUtils {
    void setProjectRoot(const std::string& root);

    std::string getProjectPath(const std::string& relative_path);

    std::string readFile(const std::string& relative_path);

    std::vector<char> readBinaryFile(const std::string& relative_path);

    std::vector<uint32_t> readSPIRVFile(const std::string& relative_path);
}