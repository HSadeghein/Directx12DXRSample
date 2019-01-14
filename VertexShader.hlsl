//struct ModelViewProjection
//{
//	matrix MVP;
//};

//ConstantBuffer<ModelViewProjection> ModelViewProjectionCB : register(b0);

cbuffer ModelViewProjectionCB : register(b0)
{
    matrix MVP;
};

struct VertexInput
{
    float3 position : POSITION;
    float3 color : COLOR;
};

struct VertexOutput
{
    float4 color : COLOR;
    float4 clipPos : SV_POSITION;
};

VertexOutput main(VertexInput IN)
{
    VertexOutput OUT;
    OUT.clipPos = mul(MVP, float4(IN.position, 1.0f));
    OUT.color = float4(IN.color, 1.0f);

    return OUT;
}
