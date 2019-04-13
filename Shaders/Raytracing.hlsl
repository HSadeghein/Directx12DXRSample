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
	float4x4 gProjToWorld;
	float4x4 gInvProj;
	float4x4 gViewProj;
	float4x4 gInvViewProj;
	float3 gEyePosW;
	float cbPerObjectPad1;
	float2 gRenderTargetSize;
	float2 gInvRenderTargetSize;
	float gNearZ;
	float gFarZ;
	float gTotalTime;
	float gDeltaTime;
};

RaytracingAccelerationStructure Scene : register(t0, space0);
RWTexture2D<float4> RenderTarget : register(u0);
ConstantBuffer<RayGenConstantBuffer> g_rayGenCB : register(b0);
ConstantBuffer<passCB> g_passCB : register(b1);
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

// Retrieve hit world position.
float3 HitWorldPosition()
{
	return WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
}

bool IsInsideViewport(float2 p, Viewport viewport)
{
	return (p.x >= viewport.left && p.x <= viewport.right)
		&& (p.y >= viewport.top && p.y <= viewport.bottom);
}

[shader("raygeneration")]
void MyRaygenShader()
{
	//float2 lerpValues = (float2)DispatchRaysIndex() / (float2)DispatchRaysDimensions();

	//// Orthographic projection since we're raytracing in screen space.
	//float3 rayDir = float3(0, 0, 1);
	//float3 origin = float3(
	//	lerp(g_rayGenCB.viewport.left, g_rayGenCB.viewport.right, lerpValues.x),
	//	lerp(g_rayGenCB.viewport.top, g_rayGenCB.viewport.bottom, lerpValues.y),
	//	0.0f);

	//if (IsInsideViewport(origin.xy, g_rayGenCB.stencil))
	//{
	//	// Trace the ray.
	//	// Set the ray's extents.
	//	RayDesc ray;
	//	ray.Origin = origin;
	//	ray.Direction = rayDir;
	//	// Set TMin to a non-zero small value to avoid aliasing issues due to floating - point errors.
	//	// TMin should be kept small to prevent missing geometry at close contact areas.
	//	ray.TMin = 0.001;
	//	ray.TMax = 10000.0;
	//	RayPayload payload = { float4(0, 0, 0, 0) };
	//	TraceRay(Scene, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, ~0, 0, 1, 0, ray, payload);

	//	// Write the raytraced color to the output texture.
	//	RenderTarget[DispatchRaysIndex().xy] = payload.color;
	//}
	//else
	//{
	//	// Render interpolated DispatchRaysIndex outside the stencil window
	//	RenderTarget[DispatchRaysIndex().xy] = float4(lerpValues, 0, 1);
	//}

	float3 rayDir;
	float3 origin;

	// Generate a ray for a camera pixel corresponding to an index from the dispatched 2D grid.
	GenerateCameraRay(DispatchRaysIndex().xy, origin, rayDir);
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
	TraceRay(Scene, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, ~0, 0, 1, 0, ray, payload);

	// Write the raytraced color to the output texture.
	RenderTarget[DispatchRaysIndex().xy] = payload.color;
}

[shader("closesthit")]
void MyClosestHitShader(inout RayPayload payload, in MyAttributes attr)
{
	float3 barycentrics = float3(1 - attr.barycentrics.x - attr.barycentrics.y, attr.barycentrics.x, attr.barycentrics.y);
	payload.color = float4(barycentrics, 1);
}

[shader("miss")]
void MyMissShader(inout RayPayload payload)
{
	payload.color = float4(0,0,0,0);
}

#endif // RAYTRACING_HLSL