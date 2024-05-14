#include "ShaderHeader.hlsli"

float4 main(PS_INPUT input) : SV_TARGET
{
    float weight = dot(input.normal, (float3) vLightDir);
    
    float4 fragmentColor = saturate(vLightColor * weight);
    fragmentColor.a = 1.f;
    
    return fragmentColor;
}