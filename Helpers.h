#pragma once
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include<stdexcept>

enum class RaytracingAPI {
	FallbackLayer,
	DirectXRaytracing,
};

struct Viewport
{
	float left;
	float top;
	float right;
	float bottom;
};

struct RayGenConstantBuffer
{
	Viewport viewport;
	Viewport stencil;
};

inline void ThrowifFailed(HRESULT hr) {
	if (FAILED(hr)) {
		throw std::exception();
	}
}

inline void ReleaseCom(IUnknown *object) {
	object->Release();
}

