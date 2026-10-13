Texture2D<float4> imageTexture : register(t0, space2);
SamplerState imageSampler : register(s0, space2);
cbuffer UiFragmentUniforms : register(b0, space3) {
    float4 tint;
    float4 glyphBounds;
    float4 parameters;
};
struct VertexOutput {
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};
float4 main(VertexOutput input) : SV_Target0 {
    float2 uv = input.uv;
    if (parameters.x > 0.5) {
        uv = clamp(uv, glyphBounds.xy, glyphBounds.zw);
        // The atlas has transparent black pixels; use alpha as coverage only.
        float coverage = imageTexture.Sample(imageSampler, uv).a;
        return float4(tint.rgb, coverage * tint.a);
    }
    return imageTexture.Sample(imageSampler, uv) * tint;
}
