//#pragma pack_matrix( column_major )


cbuffer cbPerObject : register(b0)
{
    float4x4 gWorld;
};

cbuffer cbPass : register(b1)
{
	float4x4 gView;
	float4x4 gInvView;
	float4x4 gProj;
	float4x4 gInvProj;
	float4x4 gViewProj;
	float4x4 gInvViewProj;
	float3 gEyePosW;
	float cbPerObjectPad1;
	float2 gRenderTargetSize;
	float2 gInvRenderTargetSize;
	float4 lightAmbientColor;
	float4 lightDiffuseColor;
	float3 lightPosition;
	float cbPerObjectPad2;
	float gNearZ;
	float gFarZ;
	float gTotalTime;
	float gDeltaTime;
};
cbuffer cbMaterial : register(b2)
{
	float4 gDiffuseAlbedo;
	float3 gFresnelR0;
	float  gRoughness;
	float4x4 gMatTransform;
};
struct VertexIn
{
    float3 PosL : POSITION;
    float3 Normal : NORMAL;
};

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float4 Color : COLOR;
    float4 WorldPos : TEXCOORD0;
	float3 normal : TEXCOORD1;
};

VertexOut vertex(VertexIn vin)
{
    VertexOut vout;
	
	// Transform to homogeneous clip space.
    float4 posW = mul(float4(vin.PosL, 1.0f), gWorld);
    vout.WorldPos = posW;
    vout.PosH = mul(posW, gViewProj);
	
	// Just pass vertex color into the pixel shader.
    vout.Color = float4(vin.PosL,1.0f);
    vout.normal = mul(vin.Normal, (float3x3)gWorld);
    return vout;
}

float4 pixel(VertexOut pin) : SV_Target
{
	pin.normal.xyz = normalize(pin.normal.xyz);
		float3 pixelToLight = normalize(lightPosition.xyz - pin.WorldPos.xyz);

		// Diffuse contribution.
		float fNDotL = max(0.0f, dot(pixelToLight, pin.normal.xyz));

		//return float4(pin.normal, 1.0f);
		return pin.Color;
		//return gDiffuseAlbedo * lightDiffuseColor * fNDotL;
}