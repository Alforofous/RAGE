#include "../includes/app.hpp"
#include <iostream>
#include "utils/file_reader.hpp"
#include <filesystem>

int main(int argc, char *argv[]) {
    try {
        (void)argc;
        std::filesystem::path exe_path = std::filesystem::absolute(argv[0]);
        std::filesystem::path project_root = exe_path.parent_path().parent_path();
        std::cout << "Project root: " << project_root.string() << std::endl;
        FileUtils::setProjectRoot(project_root.string());
        App app;
        app.run();

        return 0;
    }
    catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << std::endl;

        return -1;
    }
}