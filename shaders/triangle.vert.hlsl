cbuffer TransformUniforms : register(b0, space1) {
    row_major float4x4 transform;
};

struct VertexInput {
    float3 position : TEXCOORD0;
    float2 uv : TEXCOORD1;
};

struct VertexOutput {
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

VertexOutput main(VertexInput input) {
    VertexOutput output;
    output.position = mul(float4(input.position, 1.0f), transform);
    output.uv = input.uv;
    return output;
}

