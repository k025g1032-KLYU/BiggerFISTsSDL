Texture2D<float4> colorTexture : register(t0, space2);
SamplerState colorSampler : register(s0, space2);
cbuffer ColorUniforms : register(b0, space3) {
    float4 colorTint;
    float4 materialAmbientAndOpacity;
    float4 materialDiffuseAndWrap;
    float4 materialSpecular;
    float4 cameraPosition;
};

// Values from KamataEngine LightGroup::DefaultLightSetting in the bundled Release library.
static const float3 lightDirections[3] = {
    float3(0.0f, -1.0f, 0.0f),
    float3(0.5f, 0.1f, 0.2f),
    float3(-0.5f, 0.1f, -0.2f)
};

struct VertexOutput {
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
    float3 worldPosition : TEXCOORD1;
    float3 worldNormal : TEXCOORD2;
};

float3 LinearToSrgb(float3 color) {
    color = saturate(color);
    return lerp(color * 12.92f,
        1.055f * pow(color, 1.0f / 2.4f) - 0.055f,
        step(0.0031308f, color));
}

float4 main(VertexOutput input) : SV_Target0 {
    float4 sampled = colorTexture.Sample(colorSampler, input.uv);
    clip(sampled.a * colorTint.a * materialAmbientAndOpacity.a - 0.01f);
    float3 normal = input.worldNormal;
    float3 viewDirection = normalize(cameraPosition.xyz - input.worldPosition);
    float3 shade = materialAmbientAndOpacity.rgb;
    [unroll]
    for (int index = 0; index < 3; ++index) {
        float3 lightDirection = normalize(lightDirections[index]);
        float normalDotLight = dot(normal, lightDirection);
        float3 halfVector = normalize(lightDirection + viewDirection);
        float diffuse = pow(saturate(normalDotLight + materialDiffuseAndWrap.a) /
            (1.0f + materialDiffuseAndWrap.a), 2.0f);
        float specular = pow(saturate(dot(normal, halfVector)), 4.0f);
        shade += diffuse * materialDiffuseAndWrap.rgb +
            specular * materialSpecular.rgb;
    }
    float3 litColor = sampled.rgb * colorTint.rgb * shade;
    // KamataEngine draws to an sRGB render target. Encode when SDL's
    // swapchain target is UNORM; sRGB targets perform this conversion.
    if (cameraPosition.w > 0.5f) {
        litColor = LinearToSrgb(litColor);
    }
    return float4(litColor,
        sampled.a * colorTint.a * materialAmbientAndOpacity.a);
}
