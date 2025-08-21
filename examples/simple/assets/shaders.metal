#include <metal_stdlib>
using namespace metal;

struct VertexOut {
    float4 position [[position]];
    float2 texCoord;
};

vertex VertexOut vertexShader(uint vertexID [[vertex_id]],
                             constant float* vertexData [[buffer(0)]]) {
    VertexOut out;
    
    int index = vertexID * 5;
    out.position = float4(vertexData[index], vertexData[index + 1], vertexData[index + 2], 1.0);
    out.texCoord = float2(vertexData[index + 3], vertexData[index + 4]);
    
    return out;
}

fragment float4 fragmentShader(VertexOut in [[stage_in]],
                               texture2d<float> texture [[texture(0)]],
                               constant bool &useTexture [[buffer(1)]],
                               constant float4 &rectColor [[buffer(2)]]) {
    constexpr sampler textureSampler(mag_filter::linear, min_filter::linear);
    
    if (useTexture) {
        return texture.sample(textureSampler, in.texCoord);
    } else {
        return rectColor;
    }
}