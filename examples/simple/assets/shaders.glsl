#version 450

#ifdef VERTEX_SHADER
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aTexCoord;

layout(location = 0) out vec2 TexCoord;

void main() {
    gl_Position = vec4(aPos, 1.0);
    TexCoord = aTexCoord;
}
#endif

#ifdef FRAGMENT_SHADER
precision mediump float;

layout(location = 0) in vec2 TexCoord;
layout(location = 0) out vec4 FragColor;

layout(binding = 0) uniform sampler2D ourTexture;

layout(binding = 1) uniform UniformBufferObject {
    bool useTexture;
    vec4 rectColor;
} ubo;

void main() {
    if (ubo.useTexture) {
        FragColor = texture(ourTexture, TexCoord);
    } else {
        FragColor = ubo.rectColor;
    }
}
#endif

#ifdef COMPUTE_SHADER
#endif
