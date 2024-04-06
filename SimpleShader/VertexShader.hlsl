#include "ShaderHeader.hlsli"

float4 main(float4 pos : POSITION) : SV_POSITION
{   
    return mul(pos, transform);
}