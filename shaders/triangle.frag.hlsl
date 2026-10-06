Texture2D<float4> colorTexture : register(t0, space2);
SamplerState colorSampler : register(s0, space2);

struct VertexOutput {
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

float4 main(VertexOutput input) : SV_Target0 {
    // This first texture pass is opaque and unlit.
    return float4(colorTexture.Sample(colorSampler, input.uv).rgb, 1.0f);
}
