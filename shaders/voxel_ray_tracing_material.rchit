#version 460
#extension GL_EXT_ray_tracing : require

layout(location = 0) rayPayloadInEXT vec4 payload;
hitAttributeEXT vec2 attribs;

void main() {
    // For now, just return red color for any hit
    payload = vec4(1.0, 0.0, 0.0, 1.0);
}
