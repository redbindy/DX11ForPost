#include "ShaderHeader.hlsli"

struct VS_INPUT
{
    float4 pos : POSITION;
    float2 texCoord : TEXCOORD;
};

VS_OUTPUT main(VS_INPUT input)
{   
    float4x4 mat = mul(transform, world);
    mat = mul(mat, view);
    mat = mul(mat, projection);

    VS_OUTPUT output = input;
    output.pos = mul(output.pos, mat);
    
    return output;
}