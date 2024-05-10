cbuffer ConstantBuffer : register(b0)
{
    float4x4 world;
    float4x4 view;
    float4x4 projection;
    
    float4x4 transform;
}

struct VS_OUTPUT
{
    float4 pos : SV_Position;
    float4 color : COLOR;
};

typedef VS_OUTPUT PS_INPUT;