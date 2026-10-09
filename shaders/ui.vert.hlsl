cbuffer UiVertexUniforms : register(b0, space1) {
    float4 rect;
    float4 uvRect;
    float4 outputSize;
};

struct VertexOutput {
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

VertexOutput main(uint vertexId : SV_VertexID) {
    static const float2 corners[6] = {
        float2(0, 0), float2(1, 0), float2(1, 1),
        float2(0, 0), float2(1, 1), float2(0, 1)
    };
    float2 corner = corners[vertexId];
    float2 local = (corner - 0.5) * rect.zw;
    float angle = outputSize.z;
    float2 rotated = float2(local.x * cos(angle) - local.y * sin(angle),
        local.x * sin(angle) + local.y * cos(angle));
    float2 pixel = rect.xy + rect.zw * 0.5 + rotated;
    VertexOutput result;
    result.position = float4(pixel.x * 2.0 / outputSize.x - 1.0,
        1.0 - pixel.y * 2.0 / outputSize.y, 0.0, 1.0);
    result.uv = uvRect.xy + corner * uvRect.zw;
    return result;
}
