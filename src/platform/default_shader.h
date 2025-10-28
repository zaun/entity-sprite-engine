#ifndef ESE_DEFAUT_SHADER_H
#define ESE_DEFAUT_SHADER_H

#ifdef __cplusplus
extern "C" {
#endif

const char *DEFAULT_SHADER =
    "#version 450\n"
    "\n"
    "#ifdef VERTEX_SHADER\n"
    "layout(location = 0) in vec3 aPos;\n"
    "layout(location = 1) in vec2 aTexCoord;\n"
    "\n"
    "layout(location = 0) out vec2 TexCoord;\n"
    "\n"
    "void main() {\n"
    "    gl_Position = vec4(aPos, 1.0);\n"
    "    TexCoord = aTexCoord;\n"
    "}\n"
    "#endif\n"
    "\n"
    "#ifdef FRAGMENT_SHADER\n"
    "precision mediump float;\n"
    "\n"
    "layout(location = 0) in vec2 TexCoord;\n"
    "layout(location = 0) out vec4 FragColor;\n"
    "\n"
    "layout(binding = 0) uniform sampler2D ourTexture;\n"
    "\n"
    "layout(binding = 1) uniform UniformBufferObject {\n"
    "    bool useTexture;\n"
    "    vec4 rectColor;\n"
    "    vec4 tint;\n"
    "    float opacity;\n"
    "} ubo;\n"
    "\n"
    "void main() {\n"
    "    if (ubo.useTexture) {\n"
    "        vec4 tex = texture(ourTexture, TexCoord);\n"
    "        tex *= ubo.tint;\n"
    "        tex.a *= ubo.opacity;\n"
    "        FragColor = tex;\n"
    "    } else {\n"
    "        vec4 solid = ubo.rectColor;\n"
    "        solid *= ubo.tint;\n"
    "        solid.a *= ubo.opacity;\n"
    "        FragColor = solid;\n"
    "    }\n"
    "}\n"
    "#endif\n"
    "\n"
    "#ifdef COMPUTE_SHADER\n"
    "#endif\n"
    "\n";

#ifdef __cplusplus
}
#endif

#endif // ESE_DEFAUT_SHADER_H
