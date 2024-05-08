#include "ShaderHeader.hlsli"

struct VS_INPUT
{
    float4 pos : POSITION;
    float4 color : COLOR;
};

VS_OUTPUT main(VS_INPUT input)
{   
    VS_OUTPUT output;
    output.pos = input.pos;
    // output.pos = mul(input.pos, transform);
    output.color = input.color;
    
    return output;
}