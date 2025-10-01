// shader.h
#ifndef ESE_SHADER_H
#define ESE_SHADER_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Represents compiled shader data or source code.
 * 
 * @details This structure stores either compiled shader binary data or
 *          source code strings. The data pointer is heap-allocated and
 *          must be freed when the blob is destroyed. For strings, the
 *          size excludes the null terminator.
 */
typedef struct ShaderBlob {
    char* data;                     /** Pointer to compiled shader data or source string */
    size_t size;                    /** Size in bytes (for strings, excludes null terminator) */
} ShaderBlob;

// Compile GLSL source to SPIR-V binary.
// shaderStage: 0=vertex,1=tess control,2=tess eval,3=geometry,4=fragment,5=compute
// Returns ShaderBlob with allocated data buffer. Free with free_shader_blob.
ShaderBlob glsl_to_spirv(const char* source, int shaderStage);

// Compile GLSL source to desktop GLSL string (via SPIR-V intermediate).
// Returns null-terminated string allocated on heap. Free with free_shader_blob.
ShaderBlob glsl_to_glsl(const char* source, int shaderStage);

// Compile GLSL source to Metal Shading Language string (via SPIR-V intermediate).
// Returns null-terminated string allocated on heap. Free with free_shader_blob.
ShaderBlob glsl_to_metal(const char* source, int shaderStage);

// Free a ShaderBlob allocated by any of the above functions.
void free_shader_blob(ShaderBlob blob);

#ifdef __cplusplus
}
#endif

#endif // ESE_SHADER_H
