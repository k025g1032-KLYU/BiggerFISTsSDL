cbuffer TransformUniforms : register(b0, space1) {
    row_major float4x4 transform;
};

struct VertexInput {
    float3 position : TEXCOORD0;
    float3 color : TEXCOORD1;
};

struct VertexOutput {
    float4 position : SV_Position;
    float3 color : TEXCOORD0;
};

VertexOutput main(VertexInput input) {
    VertexOutput output;
    output.position = mul(float4(input.position, 1.0f), transform);
    output.color = input.color;
    return output;
}

