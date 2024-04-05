cbuffer ConstantBuffer : register(b0)
{
    float4x4 transform;
}

float4 main(float4 pos : POSITION) : SV_POSITION
{   
    return mul(pos, transform);
}