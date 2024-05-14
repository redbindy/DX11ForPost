#include "ShaderHeader.hlsli"

struct VS_INPUT
{
    float4 pos : POSITION;
    float3 normal : NORMAL;
};

VS_OUTPUT main(VS_INPUT input)
{   
    float4x4 mat = mul(transform, world);
    mat = mul(mat, view);
    mat = mul(mat, projection);

    VS_OUTPUT output = input;
    output.pos = mul(output.pos, mat);
    output.normal = mul(float4(output.normal, 1.f), normalTransform);
    
    return output;
}