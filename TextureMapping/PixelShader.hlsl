#include "ShaderHeader.hlsli"

Texture2D txDiffuse : register(t0);
SamplerState samLinear : register(s0);

float4 main(PS_INPUT input) : SV_TARGET
{
    return txDiffuse.Sample(samLinear, input.texCoord) * float4(1.f, 1.f, 1.f, 1.f);
}