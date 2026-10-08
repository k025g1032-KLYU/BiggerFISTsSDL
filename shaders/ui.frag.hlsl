Texture2D<float4> imageTexture : register(t0, space2);
SamplerState imageSampler : register(s0, space2);
cbuffer UiFragmentUniforms : register(b0, space3) {
    float4 tint;
};
struct VertexOutput {
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};
float4 main(VertexOutput input) : SV_Target0 {
    return imageTexture.Sample(imageSampler, input.uv) * tint;
}
