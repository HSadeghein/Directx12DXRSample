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

#define M_PI 3.14159265359f

struct passCB
{
	float4x4 gView;
	float4x4 gInvView;
	float4x4 gProj;
	float4x4 gInvProj;
	float4x4 gViewProj;
	float4x4 gInvViewProj;
	float3 gEyePosW;
	float lightPower;
	float2 gRenderTargetSize;
	float2 gInvRenderTargetSize;
	float4 lightAmbientColor;
	float4 lightDiffuseColor;
	float3 lightPosition;
	float SpecularPower;
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
ConstantBuffer<materialCB> g_closestHitCB : register(b0);
ConstantBuffer<passCB> g_passCB : register(b1);
ByteAddressBuffer LocalIndices : register(t2);
Buffer<float3> LocalVertices : register(t3);
typedef BuiltInTriangleIntersectionAttributes MyAttributes;



static float4 BackGroundColor = float4(0.8f, 0.9f, 1.0f, 1.0f);

struct RayPayload
{
	float4 color;
	uint recursionDepth;
};

struct ShadowPayLoad
{
	bool isHit;
};
struct Ray
{
    float3 origin;
    float3 direction;
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
//uint3 Load3x16BitIndices(uint offsetBytes)
//{
//	uint3 indices;
//
//	// ByteAdressBuffer loads must be aligned at a 4 byte boundary.
//	// Since we need to read three 16 bit indices: { 0, 1, 2 } 
//	// aligned at a 4 byte boundary as: { 0 1 } { 2 0 } { 1 2 } { 0 1 } ...
//	// we will load 8 bytes (~ 4 indices { a b | c d }) to handle two possible index triplet layouts,
//	// based on first index's offsetBytes being aligned at the 4 byte boundary or not:
//	//  Aligned:     { 0 1 | 2 - }
//	//  Not aligned: { - 0 | 1 2 }
//	const uint dwordAlignedOffset = offsetBytes & ~3;
//	const uint2 four16BitIndices = Indices.Load2(dwordAlignedOffset);
//
//	// Aligned: { 0 1 | 2 - } => retrieve first three 16bit indices
//	if (dwordAlignedOffset == offsetBytes)
//	{
//		indices.x = four16BitIndices.x & 0xffff;
//		indices.y = (four16BitIndices.x >> 16) & 0xffff;
//		indices.z = four16BitIndices.y & 0xffff;
//	}
//	else // Not aligned: { - 0 | 1 2 } => retrieve last three 16bit indices
//	{
//		indices.x = (four16BitIndices.x >> 16) & 0xffff;
//		indices.y = four16BitIndices.y & 0xffff;
//		indices.z = (four16BitIndices.y >> 16) & 0xffff;
//	}
//
//	return indices;
//}

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

// Fresnel reflectance - schlick approximation.
float FresnelReflectanceSchlick(in float3 I, in float3 N, in float3 f0)
{
	float cosi = saturate(dot(-I, N));
	float r = pow((0.5f) / (2.5f), 2);
	return r + (1 - r) * pow(1 - cosi, 5);
}

// Diffuse lighting calculation.
float4 CalculateDiffuseLighting(float3 hitPosition, float3 normal)
{
	float3 pixelToLight = normalize(g_passCB.lightPosition.xyz - hitPosition);

	// Diffuse contribution.
	float fNDotL = max(0.0f, dot(pixelToLight, normal));

	return g_closestHitCB.gDiffuseAlbedo * g_passCB.lightDiffuseColor * fNDotL * g_passCB.lightPower / M_PI;
}

float4 CalculateBlinnPhong(float3 hitPosition, float3 normal, float hardness, float isInShadow)
{
	float3 lightDir = normalize(g_passCB.lightPosition.xyz - hitPosition);
	float3 viewDir = normalize(g_passCB.gEyePosW - hitPosition);
	float shadowFactor = isInShadow ? 0.1f : 1.0f;
	float4 diffuseColor = CalculateDiffuseLighting(hitPosition, normal) * shadowFactor;


	float4 specularColor = float4(0, 0, 0, 0);
	float intensity = 0.0f;
	if (!isInShadow) {
		float3 H = normalize(lightDir + viewDir);
		float NdotH = dot(normal, H);
		intensity = pow(saturate(NdotH), hardness);

		specularColor = (intensity) * g_passCB.lightDiffuseColor;

	}
	float EnergyConversion = 1 - max(specularColor.r, max(specularColor.g, specularColor.b));

	float4 ambientColor;
	float4 ambientColorMin = BackGroundColor -0.7f;
	float4 ambientColorMax = BackGroundColor-0.6;
	float a = 1 - saturate(dot(normal, float3(0, -1, 0)));
	ambientColor = g_closestHitCB.gDiffuseAlbedo * lerp(ambientColorMin, ambientColorMax, a);

	return diffuseColor * (1 - intensity) + specularColor + ambientColor;

}

float4 TraceRadianceRay(in Ray ray, in uint currentRayRecursionDepth)
{
    if (currentRayRecursionDepth >= 2)
    {
        return float4(0, 0, 0, 0);
    }

    // Set the ray's extents.
    RayDesc rayDesc;
    rayDesc.Origin = ray.origin;
    rayDesc.Direction = ray.direction;
    // Set TMin to a zero value to avoid aliasing artifacts along contact areas.
    // Note: make sure to enable face culling so as to avoid surface face fighting.
    rayDesc.TMin = 0;
    rayDesc.TMax = 10000;
    RayPayload rayPayload = { float4(0, 0, 0, 0), currentRayRecursionDepth + 1 };
    TraceRay(Scene,
        RAY_FLAG_CULL_BACK_FACING_TRIANGLES, ~0, 0, 1, 0,rayDesc, rayPayload);

    return rayPayload.color;
}

// Trace a shadow ray and return true if it hits any geometry.
bool TraceShadowRayAndReportIfHit(in Ray ray, in uint currentRayRecursionDepth)
{
    if (currentRayRecursionDepth >= 2)
    {
        return false;
    }

    // Set the ray's extents.
    RayDesc rayDesc;
    rayDesc.Origin = ray.origin;
    rayDesc.Direction = ray.direction;
    // Set TMin to a zero value to avoid aliasing artifcats along contact areas.
    // Note: make sure to enable back-face culling so as to avoid surface face fighting.
    rayDesc.TMin = 0.0;
    rayDesc.TMax = 10000;

    // Initialize shadow ray payload.
    // Set the initial value to true since closest and any hit shaders are skipped. 
    // Shadow miss shader, if called, will set it to false.
    ShadowPayLoad shadowPayload = { true };
    TraceRay(Scene,
        RAY_FLAG_CULL_BACK_FACING_TRIANGLES
		| RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH
		| RAY_FLAG_FORCE_OPAQUE             // ~skip any hit shaders
		| RAY_FLAG_SKIP_CLOSEST_HIT_SHADER, // ~skip closest hit shaders,
        ~0,
        1,
        1,
        1,
        rayDesc, shadowPayload);

    return shadowPayload.isHit;
}



[shader("raygeneration")]
void MyRaygenShader()
{

	float3 rayDir;
	float3 origin;

	float3 raysIndex = DispatchRaysIndex();
	// Generate a ray for a camera pixel corresponding to an index from the dispatched 2D grid.
	GenerateCameraRay(raysIndex.xy, origin, rayDir);
	Ray ray;
	ray.origin = origin;
	ray.direction = rayDir;


	uint currentRecursionDepth = 0;
	float4 color = TraceRadianceRay(ray, currentRecursionDepth);
    RenderTarget[raysIndex.xy] = color;


	// // Trace the ray.
	// // Set the ray's extents.
	// RayDesc ray;
	// ray.Origin = origin;
	// ray.Direction = rayDir;
	// // Set TMin to a non-zero small value to avoid aliasing issues due to floating - point errors.
	// // TMin should be kept small to prevent missing geometry at close contact areas.
	// ray.TMin = 0.001;
	// ray.TMax = 10000.0;
	// RayPayload payload = { float4(0, 0, 0, 0), currentRecursionDepth + 1};
	// TraceRay(Scene, RAY_FLAG_CULL_BACK_FACING_TRIANGLES /*rayFlags*/, ~0, 0/*ray index*/, 1/*MultiplierForGeometryContributionToShaderIndex */, 0, ray, payload);
	// // Write the raytraced color to the output texture.
	// RenderTarget[raysIndex.xy] = payload.color;
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


	// Load up 3 16 bit indices for the triangle.
	//const uint3 indices = Load3x16BitIndices(baseIndex);

	const uint strideInFloat3s = 2;
	const uint positionOffsetInFloat3s = 0;
	const uint normalOffsetInFloat3s = 1;
	//const uint3 indices = Indices.Load3(baseIndex);
	const uint3 localindices = LocalIndices.Load3(baseIndex);

	// Retrieve corresponding vertex normals for the triangle vertices.
	float3 vertexNormals[3] = {
		LocalVertices[localindices[0] * strideInFloat3s + normalOffsetInFloat3s],
		LocalVertices[localindices[1] * strideInFloat3s + normalOffsetInFloat3s],
		LocalVertices[localindices[2] * strideInFloat3s + normalOffsetInFloat3s]
	};
	float3 vertexPositions[3] = {
	LocalVertices[localindices[0] * strideInFloat3s + positionOffsetInFloat3s],
	LocalVertices[localindices[1] * strideInFloat3s + positionOffsetInFloat3s],
	LocalVertices[localindices[2] * strideInFloat3s + positionOffsetInFloat3s]
	};


	float3 localPos = (vertexPositions[0] * bary.x + vertexPositions[1] * bary.y + vertexPositions[2] * bary.z);
	float3 localNormal = (vertexNormals[0] * bary.x + vertexNormals[1] * bary.y + vertexNormals[2] * bary.z);




	float3 lightPosition = g_passCB.lightPosition.xyz;

	Ray shadowRay = { hitPosition - 0.001f, normalize(lightPosition - hitPosition) };
	bool shadowRayHit = TraceShadowRayAndReportIfHit(shadowRay, payload.recursionDepth);
	// Reflected component.
	float4 reflectedColor = float4(0, 0, 0, 0);

	 //Trace a reflection ray.
	//Ray reflectionRay = { HitWorldPosition() -0.001f, reflect(WorldRayDirection(), localNormal) };
	//float4 reflectionColor = TraceRadianceRay(reflectionRay, payload.recursionDepth);

	//float fresnelR = FresnelReflectanceSchlick(WorldRayDirection(), localNormal, g_closestHitCB.gDiffuseAlbedo.xyz);
	//reflectedColor =   g_closestHitCB.gRoughness * fresnelR * reflectionColor;

	float4 phongColor = CalculateBlinnPhong(hitPosition, localNormal, g_passCB.SpecularPower, shadowRayHit);
	float4 color = phongColor;

	float4 finalColor = color + reflectedColor ;

	// it gives the distance between the point which ray was radiated and the hit point
	float t = RayTCurrent();
	finalColor = lerp(finalColor, BackGroundColor, 1.0 - exp(-0.000002 * t * t * t));

	payload.color = finalColor;

}

[shader("miss")]
void MyMissShader(inout RayPayload payload)
{
	payload.color = BackGroundColor;
}


[shader("miss")]
void MyMissShadowRay(inout ShadowPayLoad payload)
{
	payload.isHit = false;
}

#endif // RAYTRACING_HLSL