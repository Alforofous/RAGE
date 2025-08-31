#version 460
#extension GL_EXT_ray_tracing : require

layout(location = 0) rayPayloadInEXT vec4 payload;

void main() {
    // Return black/transparent for missed rays
    payload = vec4(0.0, 0.0, 0.0, 0.0);
}
