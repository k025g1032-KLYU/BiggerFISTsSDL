cbuffer TransformUniforms : register(b0, space1) {
    row_major float4x4 transform;
    row_major float4x4 world;
    row_major float4x4 normalTransform;
};

struct VertexInput {
    float3 position : TEXCOORD0;
    float2 uv : TEXCOORD1;
    float3 normal : TEXCOORD2;
};

struct VertexOutput {
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
    float3 worldPosition : TEXCOORD1;
    float3 worldNormal : TEXCOORD2;
};

VertexOutput main(VertexInput input) {
    VertexOutput output;
    output.position = mul(float4(input.position, 1.0f), transform);
    output.uv = input.uv;
    output.worldPosition = mul(float4(input.position, 1.0f), world).xyz;
    output.worldNormal = normalize(mul(float4(input.normal, 0.0f), normalTransform).xyz);
    return output;
}

