cbuffer ConstantBuffer : register(b0)
{
    float4x4 world;
    float4x4 view;
    float4x4 projection;
    
    float4x4 transform;
    float4x4 normalTransform;
    
    float4 vLightDir;
    float4 vLightColor;
}

struct VS_OUTPUT
{
    float4 pos : SV_Position;
    float3 normal : TEXCOORD;
};

typedef VS_OUTPUT PS_INPUT;