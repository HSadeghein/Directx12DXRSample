#pragma once
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include<stdexcept>

enum class RaytracingAPI {
	FallbackLayer,
	DirectXRaytracing,
};


inline void ThrowifFailed(HRESULT hr) {
	if (FAILED(hr)) {
		throw std::exception();
	}
}

inline void ReleaseCom(IUnknown *object) {
	object->Release();
}

