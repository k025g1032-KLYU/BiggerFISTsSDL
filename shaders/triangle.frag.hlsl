Texture2D<float4> colorTexture : register(t0, space2);
SamplerState colorSampler : register(s0, space2);
cbuffer ColorUniforms : register(b0, space3) {
    float4 colorTint;
    float4 materialAmbientAndOpacity;
};

struct VertexOutput {
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

float4 main(VertexOutput input) : SV_Target0 {
    float4 sampled = colorTexture.Sample(colorSampler, input.uv);
    clip(sampled.a * colorTint.a * materialAmbientAndOpacity.a - 0.01f);
    return float4(sampled.rgb * colorTint.rgb * materialAmbientAndOpacity.rgb,
        sampled.a * colorTint.a * materialAmbientAndOpacity.a);
}
