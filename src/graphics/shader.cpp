// shader.cpp

#include <SPIRV/GlslangToSpv.h>
#include <glslang/Public/ShaderLang.h>

#include <spirv_cross.hpp>
#include <spirv_msl.hpp>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <regex>
#include <string>
#include <vector>

#include "core/memory_manager.h"
#include "graphics/shader.h"
#include "utility/log.h"

extern "C" {

static TBuiltInResource GetDefaultResources() {
    TBuiltInResource resources = {};

    // Vertex shader limits
    resources.maxVertexAttribs = 64;
    resources.maxVertexUniformComponents = 4096;
    resources.maxVaryingFloats = 64;
    resources.maxVertexTextureImageUnits = 32;
    resources.maxCombinedTextureImageUnits = 80;
    resources.maxTextureImageUnits = 32;
    resources.maxFragmentUniformComponents = 4096;
    resources.maxDrawBuffers = 32;
    resources.maxVertexUniformVectors = 128;
    resources.maxVaryingVectors = 16;
    resources.maxFragmentUniformVectors = 16;
    resources.maxVertexOutputVectors = 16;
    resources.maxFragmentInputVectors = 15;
    resources.minProgramTexelOffset = -8;
    resources.maxProgramTexelOffset = 7;

    // Geometry shader limits
    resources.maxGeometryInputComponents = 64;
    resources.maxGeometryOutputComponents = 128;
    resources.maxGeometryImageUniforms = 8;
    resources.maxGeometryTextureImageUnits = 16;
    resources.maxGeometryOutputVertices = 256;
    resources.maxGeometryTotalOutputComponents = 1024;
    resources.maxGeometryUniformComponents = 1024;
    resources.maxGeometryVaryingComponents = 64;

    // Tessellation limits
    resources.maxTessControlInputComponents = 128;
    resources.maxTessControlOutputComponents = 128;
    resources.maxTessControlTextureImageUnits = 16;
    resources.maxTessControlUniformComponents = 1024;
    resources.maxTessControlTotalOutputComponents = 4096;
    resources.maxTessEvaluationInputComponents = 128;
    resources.maxTessEvaluationOutputComponents = 128;
    resources.maxTessEvaluationTextureImageUnits = 16;
    resources.maxTessEvaluationUniformComponents = 1024;
    resources.maxTessPatchComponents = 120;
    resources.maxPatchVertices = 32;
    resources.maxTessGenLevel = 64;

    // Compute shader limits
    resources.maxComputeWorkGroupCountX = 65535;
    resources.maxComputeWorkGroupCountY = 65535;
    resources.maxComputeWorkGroupCountZ = 65535;
    resources.maxComputeWorkGroupSizeX = 1024;
    resources.maxComputeWorkGroupSizeY = 1024;
    resources.maxComputeWorkGroupSizeZ = 64;
    resources.maxComputeUniformComponents = 1024;
    resources.maxComputeTextureImageUnits = 16;
    resources.maxComputeImageUniforms = 8;
    resources.maxComputeAtomicCounters = 8;
    resources.maxComputeAtomicCounterBuffers = 1;

    // Image and atomic counter limits
    resources.maxImageUnits = 8;
    resources.maxVertexImageUniforms = 8;
    resources.maxTessControlImageUniforms = 8;
    resources.maxTessEvaluationImageUniforms = 8;
    resources.maxGeometryImageUniforms = 8;
    resources.maxFragmentImageUniforms = 8;
    resources.maxCombinedImageUniforms = 8;
    resources.maxCombinedShaderOutputResources = 8;

    resources.maxVertexAtomicCounters = 8;
    resources.maxTessControlAtomicCounters = 8;
    resources.maxTessEvaluationAtomicCounters = 8;
    resources.maxGeometryAtomicCounters = 8;
    resources.maxFragmentAtomicCounters = 8;
    resources.maxCombinedAtomicCounters = 8;
    resources.maxAtomicCounterBindings = 8;
    resources.maxVertexAtomicCounterBuffers = 8;
    resources.maxTessControlAtomicCounterBuffers = 8;
    resources.maxTessEvaluationAtomicCounterBuffers = 8;
    resources.maxGeometryAtomicCounterBuffers = 8;
    resources.maxFragmentAtomicCounterBuffers = 8;
    resources.maxCombinedAtomicCounterBuffers = 8;
    resources.maxAtomicCounterBufferSize = 16384;

    // Other limits
    resources.maxTransformFeedbackBuffers = 4;
    resources.maxTransformFeedbackInterleavedComponents = 64;
    resources.maxCullDistances = 8;
    resources.maxCombinedClipAndCullDistances = 8;
    resources.maxSamples = 4;

    // CRITICAL: Extension-specific limits that cause the array size error
    resources.maxDualSourceDrawBuffersEXT = 1; // This should fix gl_MaxDualSourceDrawBuffersEXT

    // Feature enables
    resources.limits.nonInductiveForLoops = 1;
    resources.limits.whileLoops = 1;
    resources.limits.doWhileLoops = 1;
    resources.limits.generalUniformIndexing = 1;
    resources.limits.generalAttributeMatrixVectorIndexing = 1;
    resources.limits.generalVaryingIndexing = 1;
    resources.limits.generalSamplerIndexing = 1;
    resources.limits.generalVariableIndexing = 1;
    resources.limits.generalConstantMatrixVectorIndexing = 1;

    return resources;
}

// Helper: Initialize glslang once per process
static bool glslangInitialized = false;
static void ensure_glslang_initialized() {
    if (!glslangInitialized) {
        glslang::InitializeProcess();
        glslangInitialized = true;
    }
}

static void finalize_glslang() {
    if (glslangInitialized) {
        glslang::FinalizeProcess();
        glslangInitialized = false;
    }
}

// Compile GLSL source to SPIR-V binary
static bool compile_glsl_to_spirv(const char *source, int shaderStage, std::vector<uint32_t> &spirv,
                                  char *errBuf, size_t errBufSize) {
    glslang::TShader shader(static_cast<EShLanguage>(shaderStage));
    TBuiltInResource resources = GetDefaultResources();

    // Use proper SPIRV generation messages with debug info
    EShMessages messages =
        (EShMessages)(EShMsgSpvRules | EShMsgVulkanRules | EShMsgDebugInfo | EShMsgKeepUncalled);

    // Simple preamble without trying to modify extensions
    std::string preamble;
    switch (shaderStage) {
    case EShLangVertex:
        log_debug("SHADER", "Compiling VERTEX_SHADER");
        preamble = "#define VERTEX_SHADER\n";
        break;
    case EShLangFragment:
        log_debug("SHADER", "Compiling FRAGMENT_SHADER");
        preamble = "#define FRAGMENT_SHADER\n";
        break;
    case EShLangCompute:
        log_debug("SHADER", "Compiling COMPUTE_SHADER");
        preamble = "#define COMPUTE_SHADER\n";
        break;
    }

    shader.setPreamble(preamble.c_str());
    shader.setStrings(&source, 1);
    shader.setAutoMapBindings(true);
    shader.setAutoMapLocations(true);

    // Use GLSL version 450 for better SPIRV compatibility
    if (!shader.parse(&resources, 450, false, messages)) {
        log_error("SHADER", "GLSL parse failed:\n%s", shader.getInfoLog());
        if (errBuf && errBufSize > 0) {
            strncpy(errBuf, shader.getInfoLog(), errBufSize - 1);
            errBuf[errBufSize - 1] = '\0';
        }
        return false;
    }

    glslang::TProgram program;
    program.addShader(&shader);

    if (!program.link(messages)) {
        log_error("SHADER", "Program link failed:\n%s", program.getInfoLog());
        if (errBuf && errBufSize > 0) {
            strncpy(errBuf, program.getInfoLog(), errBufSize - 1);
            errBuf[errBufSize - 1] = '\0';
        }
        return false;
    }

    // Generate SPIRV with debug info to preserve names
    spv::SpvBuildLogger logger;
    glslang::SpvOptions spvOptions;
    spvOptions.generateDebugInfo = true; // Keep debug info for names
    spvOptions.stripDebugInfo = false;   // Don't strip debug info
    spvOptions.disableOptimizer = true;  // Disable optimizer to preserve names
    spvOptions.optimizeSize = false;
    spvOptions.disassemble = false;
    spvOptions.validate = true;

    glslang::GlslangToSpv(*program.getIntermediate(static_cast<EShLanguage>(shaderStage)), spirv,
                          &logger, &spvOptions);

    if (!logger.getAllMessages().empty()) {
        log_debug("SHADER", "SPIRV generation messages: %s", logger.getAllMessages().c_str());
    }

    return true;
}

// Convert SPIR-V binary to GLSL string (desktop OpenGL)
static std::string spirv_to_glsl(const std::vector<uint32_t> &spirv) {
    spirv_cross::CompilerGLSL glsl(spirv);

    // Get the options reference and modify it for OpenGL 3.3 compatibility
    auto options = glsl.get_common_options();
    options.version = 330;
    options.es = false;
    options.force_temporary = false;
    options.separate_shader_objects = false;
    options.flatten_multidimensional_arrays = false;
    options.enable_420pack_extension = false;             // Disable 420pack for compatibility
    options.emit_uniform_buffer_as_plain_uniforms = true; // Convert UBOs to regular uniforms

    // Set vertex-specific options
    options.vertex.fixup_clipspace = false;
    options.vertex.flip_vert_y = false;

    // Apply the modified options back
    glsl.set_common_options(options);

    // Remove binding qualifiers and flatten uniform blocks
    spirv_cross::ShaderResources resources = glsl.get_shader_resources();

    // Process uniform buffers - remove binding qualifiers
    for (auto &ubo : resources.uniform_buffers) {
        glsl.unset_decoration(ubo.id, spv::DecorationBinding);
        glsl.unset_decoration(ubo.id, spv::DecorationDescriptorSet);
    }

    // Process samplers - remove binding qualifiers
    for (auto &sampler : resources.sampled_images) {
        glsl.unset_decoration(sampler.id, spv::DecorationBinding);
        glsl.unset_decoration(sampler.id, spv::DecorationDescriptorSet);
    }

    // Process separate images
    for (auto &image : resources.separate_images) {
        glsl.unset_decoration(image.id, spv::DecorationBinding);
        glsl.unset_decoration(image.id, spv::DecorationDescriptorSet);
    }

    // Process separate samplers
    for (auto &sampler : resources.separate_samplers) {
        glsl.unset_decoration(sampler.id, spv::DecorationBinding);
        glsl.unset_decoration(sampler.id, spv::DecorationDescriptorSet);
    }

    return glsl.compile();
}

// Convert SPIR-V binary to Metal Shading Language string
static std::string spirv_to_metal(const std::vector<uint32_t> &spirv, int shaderStage) {
    spirv_cross::CompilerMSL msl(spirv);
    auto options = msl.get_msl_options();

    // Configure MSL options
    options.platform = spirv_cross::CompilerMSL::Options::macOS; // or iOS
    options.msl_version = spirv_cross::CompilerMSL::Options::make_msl_version(2, 0);
    options.pad_fragment_output_components = true;
    options.force_active_argument_buffer_resources = true;
    options.enable_decoration_binding = true;

    // Set entry point name based on shader stage
    std::string entry_point_name;
    switch (shaderStage) {
    case EShLangVertex:
        entry_point_name = "vertexShader";
        break;
    case EShLangFragment:
        entry_point_name = "fragmentShader";
        break;
    case EShLangCompute:
        entry_point_name = "computeShader";
        break;
    default:
        entry_point_name = "main0";
        break;
    }

    // Rename the entry point
    auto entry_points = msl.get_entry_points_and_stages();
    if (!entry_points.empty()) {
        msl.rename_entry_point(entry_points[0].name, entry_point_name,
                               entry_points[0].execution_model);
    }

    msl.set_msl_options(options);

    try {
        std::string metal_code = msl.compile();

        // Generic post-processing to fix missing struct definitions
        // This uses string analysis to detect what structs are referenced but
        // not defined
        std::vector<std::string> missing_structs;
        std::string additional_code = "";

        // Check for missing input/output structs
        std::regex entry_func_regex(entry_point_name +
                                    R"(\([^)]*(\w+_in)\s+\w+\s+\[\[stage_in\]\])");
        std::smatch match;
        if (std::regex_search(metal_code, match, entry_func_regex)) {
            std::string input_struct_name = match[1].str();
            if (metal_code.find("struct " + input_struct_name) == std::string::npos) {
                missing_structs.push_back(input_struct_name);
            }
        }

        std::regex return_type_regex(entry_point_name + R"(_out\s+)" + entry_point_name);
        if (std::regex_search(metal_code, return_type_regex)) {
            std::string output_struct_name = entry_point_name + "_out";
            if (metal_code.find("struct " + output_struct_name) == std::string::npos) {
                missing_structs.push_back(output_struct_name);
            }
        }

        // Check for missing UBO/buffer structs
        std::regex ubo_regex(R"(constant\s+(\w+)&\s+\w+\s+\[\[buffer\(\d+\)\]\])");
        std::sregex_iterator iter(metal_code.begin(), metal_code.end(), ubo_regex);
        std::sregex_iterator end;
        for (; iter != end; ++iter) {
            std::string struct_name = (*iter)[1].str();
            if (metal_code.find("struct " + struct_name) == std::string::npos) {
                missing_structs.push_back(struct_name);
            }
        }

        // Generate missing struct definitions by analyzing the compiled code
        for (const std::string &struct_name : missing_structs) {
            additional_code += "struct " + struct_name + "\n{\n";

            if (struct_name.find("_in") != std::string::npos) {
                // Input struct - analyze function parameters or guess from
                // vertex outputs
                if (shaderStage == EShLangFragment) {
                    // Fragment input should match common vertex outputs
                    std::regex vertex_out_regex(R"((\w+)\s+(\w+)\s+\[\[user\(locn(\d+)\)\]\])");
                    std::sregex_iterator out_iter(metal_code.begin(), metal_code.end(),
                                                  vertex_out_regex);
                    for (; out_iter != end; ++out_iter) {
                        additional_code += "    " + (*out_iter)[1].str() + " " +
                                           (*out_iter)[2].str() + " [[user(locn" +
                                           (*out_iter)[3].str() + ")]];\n";
                    }
                } else {
                    // Vertex input - analyze attribute declarations
                    std::regex attr_regex(R"((\w+)\s+(\w+)\s+\[\[attribute\((\d+)\)\]\])");
                    std::sregex_iterator attr_iter(metal_code.begin(), metal_code.end(),
                                                   attr_regex);
                    for (; attr_iter != end; ++attr_iter) {
                        additional_code += "    " + (*attr_iter)[1].str() + " " +
                                           (*attr_iter)[2].str() + " [[attribute(" +
                                           (*attr_iter)[3].str() + ")]];\n";
                    }
                }
            } else if (struct_name.find("_out") != std::string::npos) {
                // Output struct - analyze assignments in the function
                if (shaderStage == EShLangFragment) {
                    // Fragment output - look for color outputs
                    std::regex color_assign_regex(R"(out\.(\w+)\s+=)");
                    std::sregex_iterator color_iter(metal_code.begin(), metal_code.end(),
                                                    color_assign_regex);
                    std::set<std::string> found_outputs;
                    for (; color_iter != end; ++color_iter) {
                        std::string output_name = (*color_iter)[1].str();
                        if (found_outputs.find(output_name) == found_outputs.end()) {
                            found_outputs.insert(output_name);
                            additional_code += "    float4 " + output_name + " [[color(0)]];\n";
                        }
                    }
                } else {
                    // Vertex output - analyze out struct assignments
                    std::regex out_assign_regex(R"(out\.(\w+)\s+=)");
                    std::sregex_iterator out_iter(metal_code.begin(), metal_code.end(),
                                                  out_assign_regex);
                    std::set<std::string> found_outputs;
                    int location = 0;
                    for (; out_iter != end; ++out_iter) {
                        std::string output_name = (*out_iter)[1].str();
                        if (found_outputs.find(output_name) == found_outputs.end()) {
                            found_outputs.insert(output_name);
                            if (output_name == "gl_Position") {
                                additional_code += "    float4 " + output_name + " [[position]];\n";
                            } else {
                                additional_code += "    float2 " + output_name + " [[user(locn" +
                                                   std::to_string(location++) + ")]];\n";
                            }
                        }
                    }
                }
            } else {
                // UBO/Buffer struct - analyze member access patterns
                std::regex member_access_regex(R"(\w+\.(\w+))");
                std::sregex_iterator member_iter(metal_code.begin(), metal_code.end(),
                                                 member_access_regex);
                std::set<std::string> found_members;
                for (; member_iter != end; ++member_iter) {
                    std::string member_name = (*member_iter)[1].str();
                    if (found_members.find(member_name) == found_members.end()) {
                        found_members.insert(member_name);
                        // Try to infer type from usage context
                        if (member_name.find("Color") != std::string::npos ||
                            member_name.find("color") != std::string::npos) {
                            additional_code += "    float4 " + member_name + ";\n";
                        } else if (member_name.find("use") != std::string::npos ||
                                   member_name.find("enable") != std::string::npos) {
                            additional_code += "    uint " + member_name + ";\n";
                        } else {
                            additional_code += "    float " + member_name + ";\n";
                        }
                    }
                }
            }

            additional_code += "};\n\n";
        }

        // Insert the generated structs
        if (!additional_code.empty()) {
            size_t insert_pos = metal_code.find("using namespace metal;");
            if (insert_pos != std::string::npos) {
                insert_pos = metal_code.find('\n', insert_pos) + 1;
                metal_code.insert(insert_pos, additional_code);
            }
        }

        return metal_code;
    } catch (const spirv_cross::CompilerError &e) {
        log_error("SHADER", "SPIRV-Cross compilation error: %s", e.what());
        throw;
    }
}

// ---- Exposed C API ----

// glsl_to_spirv:
// Compile GLSL source to SPIR-V binary blob.
// shaderStage must be EShLanguage enum (0=vertex,1=fragment,...)
// Returns ShaderBlob with allocated data buffer (free with free_shader_blob)
ShaderBlob glsl_to_spirv(const char *source, int shaderStage) {
    ShaderBlob blob = {nullptr, 0};
    ensure_glslang_initialized();

    std::vector<uint32_t> spirv;
    char err[1024] = {0};

    if (!compile_glsl_to_spirv(source, shaderStage, spirv, err, sizeof(err))) {
        fprintf(stderr, "%s\n", err);
        return blob;
    }

    size_t byteSize = spirv.size() * sizeof(uint32_t);
    blob.data = (char *)memory_manager.malloc(byteSize, MMTAG_SHADER);
    if (!blob.data)
        return blob;

    memcpy(blob.data, spirv.data(), byteSize);
    blob.size = byteSize;
    return blob;
}

// glsl_to_glsl:
// Convert GLSL source to GLSL output targeting desktop OpenGL (via SPIR-V
// intermediate). Returns null-terminated C string allocated on heap; free with
// free_shader_blob.
ShaderBlob glsl_to_glsl(const char *source, int shaderStage) {
    ShaderBlob blob = {nullptr, 0};
    ensure_glslang_initialized();

    std::vector<uint32_t> spirv;
    char err[1024] = {0};

    if (!compile_glsl_to_spirv(source, shaderStage, spirv, err, sizeof(err))) {
        fprintf(stderr, "%s\n", err);
        return blob;
    }

    std::string glsl = spirv_to_glsl(spirv);
    blob.size = glsl.size();
    blob.data = (char *)memory_manager.malloc(blob.size + 1, MMTAG_SHADER);
    if (!blob.data)
        return blob;

    memcpy(blob.data, glsl.c_str(), blob.size);
    blob.data[blob.size] = '\0';
    return blob;
}

// glsl_to_metal:
// Convert GLSL source to Metal shading language source string (via SPIR-V
// intermediate). Returns null-terminated C string allocated on heap; free with
// free_shader_blob.
ShaderBlob glsl_to_metal(const char *source, int shaderStage) {
    ShaderBlob blob = {nullptr, 0};
    ensure_glslang_initialized();

    std::vector<uint32_t> spirv;
    char err[1024] = {0};

    if (!compile_glsl_to_spirv(source, shaderStage, spirv, err, sizeof(err))) {
        fprintf(stderr, "%s\n", err);
        return blob;
    }

    std::string metal = spirv_to_metal(spirv, shaderStage);
    blob.size = metal.size();
    blob.data = (char *)memory_manager.malloc(blob.size + 1, MMTAG_SHADER);
    if (!blob.data)
        return blob;

    memcpy(blob.data, metal.c_str(), blob.size);
    blob.data[blob.size] = '\0';
    return blob;
}

// Free a ShaderBlob allocated by the above functions
void free_shader_blob(ShaderBlob blob) {
    if (blob.data) {
        memory_manager.free(blob.data);
    }
}

} // extern "C"
