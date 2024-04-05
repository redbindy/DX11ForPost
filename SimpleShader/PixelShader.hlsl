cbuffer ConstantBuffer : register(b0)
{
    float4x4 transform;
}

float4 main(float4 pos : SV_Position) : SV_TARGET
{
    return mul(float4(pos.x, pos.y, pos.z, 1.f), transform);
}