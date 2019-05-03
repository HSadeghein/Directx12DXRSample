//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

#ifndef RAYTRACING_HLSL
#define RAYTRACING_HLSL

#include "RaytracingHlslCompat.h"
struct passCB
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
struct materialCB
{
	float4 gDiffuseAlbedo;
	float3 gFresnelR0;
	float  gRoughness;
	float4x4 gMatTransform;
};
struct Vertex
{
	float3 position;
	float3 normal;
};

RaytracingAccelerationStructure Scene : register(t0, space0);
RWTexture2D<float4> RenderTarget : register(u0);
ConstantBuffer<materialCB> g_rayGenCB : register(b0);
ConstantBuffer<passCB> g_passCB : register(b1);
ByteAddressBuffer Indices : register(t1);
StructuredBuffer<Vertex> Vertices : register(t2);

typedef BuiltInTriangleIntersectionAttributes MyAttributes;
struct RayPayload
{
	float4 color;
};

inline void GenerateCameraRay(uint2 index, out float3 origin, out float3 direction)
{
	float2 xy = index + 0.5f;
	float2 screenPos = xy / DispatchRaysDimensions().xy * 2.0 - 1.0;

	screenPos.y = -screenPos.y;

	// Unproject the pixel coordinate into a ray.
	float4 world = mul(float4(screenPos, 0, 1),g_passCB.gInvViewProj);

	world.xyz /= world.w;
	float3 tmp = g_passCB.gEyePosW.xyz;
	origin = float3(tmp.x, tmp.y, tmp.z);
	direction = normalize(world.xyz - origin);
}

// Load three 16 bit indices from a byte addressed buffer.
uint3 Load3x16BitIndices(uint offsetBytes)
{
	uint3 indices;

	// ByteAdressBuffer loads must be aligned at a 4 byte boundary.
	// Since we need to read three 16 bit indices: { 0, 1, 2 } 
	// aligned at a 4 byte boundary as: { 0 1 } { 2 0 } { 1 2 } { 0 1 } ...
	// we will load 8 bytes (~ 4 indices { a b | c d }) to handle two possible index triplet layouts,
	// based on first index's offsetBytes being aligned at the 4 byte boundary or not:
	//  Aligned:     { 0 1 | 2 - }
	//  Not aligned: { - 0 | 1 2 }
	const uint dwordAlignedOffset = offsetBytes & ~3;
	const uint2 four16BitIndices = Indices.Load2(dwordAlignedOffset);

	// Aligned: { 0 1 | 2 - } => retrieve first three 16bit indices
	if (dwordAlignedOffset == offsetBytes)
	{
		indices.x = four16BitIndices.x & 0xffff;
		indices.y = (four16BitIndices.x >> 16) & 0xffff;
		indices.z = four16BitIndices.y & 0xffff;
	}
	else // Not aligned: { - 0 | 1 2 } => retrieve last three 16bit indices
	{
		indices.x = (four16BitIndices.x >> 16) & 0xffff;
		indices.y = four16BitIndices.y & 0xffff;
		indices.z = (four16BitIndices.y >> 16) & 0xffff;
	}

	return indices;
}

// Retrieve hit world position.
float3 HitWorldPosition()
{
	return WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
}

float3 HitAttribute(float3 vertexAttribute[3], BuiltInTriangleIntersectionAttributes attr)
{
	return vertexAttribute[0] +
		attr.barycentrics.x * (vertexAttribute[1] - vertexAttribute[0]) +
		attr.barycentrics.y * (vertexAttribute[2] - vertexAttribute[0]);
}

// Diffuse lighting calculation.
float4 CalculateDiffuseLighting(float3 hitPosition, float3 normal)
{
	float3 pixelToLight = normalize(g_passCB.lightPosition.xyz - hitPosition);

	// Diffuse contribution.
	float fNDotL = max(0.0f, dot(pixelToLight, normal));

	return g_rayGenCB.gDiffuseAlbedo* g_passCB.lightDiffuseColor * fNDotL;
}


bool IsInsideViewport(float2 p, Viewport viewport)
{
	return (p.x >= viewport.left && p.x <= viewport.right)
		&& (p.y >= viewport.top && p.y <= viewport.bottom);
}



[shader("raygeneration")]
void MyRaygenShader()
{
	float3 rayDir;
	float3 origin;

	float3 raysIndex = DispatchRaysIndex();
	// Generate a ray for a camera pixel corresponding to an index from the dispatched 2D grid.
	GenerateCameraRay(raysIndex.xy, origin, rayDir);
	// Trace the ray.
	// Set the ray's extents.
	RayDesc ray;
	ray.Origin = origin;
	ray.Direction = rayDir;
	// Set TMin to a non-zero small value to avoid aliasing issues due to floating - point errors.
	// TMin should be kept small to prevent missing geometry at close contact areas.
	ray.TMin = 0.001;
	ray.TMax = 10000.0;
	RayPayload payload = { float4(0, 0, 0, 0) };
	TraceRay(Scene, RAY_FLAG_CULL_BACK_FACING_TRIANGLES /*rayFlags*/, ~0, 0/*ray index*/, 0/*MultiplierForGeometryContributionToShaderIndex */, 0, ray, payload);
	// Write the raytraced color to the output texture.
	RenderTarget[raysIndex.xy] = payload.color;
}

[shader("closesthit")]
void MyClosestHitShader(inout RayPayload payload, in MyAttributes attr)
{
	float3 bary = float3(1.0 - attr.barycentrics.x - attr.barycentrics.y, attr.barycentrics.x, attr.barycentrics.y);
	float3 hitPosition = HitWorldPosition();
	// Get the base index of the triangle's first 16 bit index.
	uint indexSizeInBytes = 4;
	uint indicesPerTriangle = 3;
	uint triangleIndexStride = indicesPerTriangle * indexSizeInBytes;
	uint baseIndex = PrimitiveIndex() * triangleIndexStride;

	//float3 indicesnew = Indices.Load3(baseIndex);
	//Vertex v;
	//v.position = float3(0.0f, 0.0f, 0.0f);
	//v.normal = float3(0, 0, 0);
	//for (uint i = 0; i < 3; i++)
	//{
	//	int address = (indicesnew[i] * 6) * 4;
	//	v.position += asfloat(Vertices.Load3(address)) * bary[i];
	//	address += (3 * 2);
	//	v.normal += asfloat(Vertices.Load3(address)) * bary[i];
	//}

	// Load up 3 16 bit indices for the triangle.
	//const uint3 indices = Load3x16BitIndices(baseIndex);

	const uint3 indices = Indices.Load3(baseIndex);
	// Retrieve corresponding vertex normals for the triangle vertices.
	float3 vertexNormals[3] = {
		Vertices[indices[0]].normal,
		Vertices[indices[1]].normal,
		Vertices[indices[2]].normal
	};
	float3 vertexPositions[3] = {
	Vertices[indices[0]].position,
	Vertices[indices[1]].position,
	Vertices[indices[2]].position
	};

	//float3 e1 = vertexPositions[1] - vertexPositions[0];
	//float3 e2 = vertexPositions[2] - vertexPositions[0];
	//float3 nnormal = normalize(cross(e2, e1));

	float3 vsNormal = (vertexPositions[0] * bary.x + vertexPositions[1] * bary.y + vertexPositions[2] * bary.z);
	// Compute the triangle's normal.
// This is redundant and done for illustration purposes 
// as all the per-vertex normals are the same and match triangle's normal in this sample. 
	//float3 triangleNormal = HitAttribute(vertexNormals, attr);
	//float4 worldNormal = mul(float4(triangleNormal,1), g_passCB.gInvViewProj);
	//worldNormal = normalize(worldNormal);
	float4 diffuseColor = CalculateDiffuseLighting(hitPosition, float3(0,0,0));
	float4 color = g_passCB.lightAmbientColor + diffuseColor;

	payload.color = float4(vsNormal,1.0f);

	//uint instanceID = InstanceID();
	//float3 barycentrics = float3(1 - attr.barycentrics.x - attr.barycentrics.y, attr.barycentrics.x, attr.barycentrics.y);
	//payload.color = float4(barycentrics, 1) * g_rayGenCB.gDiffuseAlbedo ;


}

[shader("miss")]
void MyMissShader(inout RayPayload payload)
{
	payload.color = float4(0.8f, 0.9f, 1.0f, 1.0f);
}

#endif // RAYTRACING_HLSL