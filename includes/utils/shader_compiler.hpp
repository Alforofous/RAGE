#pragma once
#include <vector>
#include <string>

enum class ShaderKind : uint8_t {
    VERTEX,
    FRAGMENT,
    COMPUTE,
    RAYGEN,
    MISS,
    CLOSEST_HIT,
    ANY_HIT,
};

class ShaderCompiler {
public:
    static std::vector<uint32_t> compileGLSL(
        const std::string &source,
        ShaderKind kind,
        const std::string &entryPoint = "main"
    );
    static std::string getLastError();
private:
    static std::string LAST_ERROR;
};