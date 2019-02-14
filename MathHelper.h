#pragma once
#include<DirectXMath.h>
using namespace DirectX;
namespace MathHelper {
	inline XMFLOAT4X4 Identity4x4() {
		XMFLOAT4X4 out;
		XMStoreFloat4x4(&out, XMMatrixIdentity());
		return out;
		//for (size_t i = 0; i < 4; i++)
		//{
		//	for (size_t j = 0; j < 4; j++)
		//	{
		//		out.m[i][j] = 0;
		//		if (i == j)
		//			out.m[i][j] = 1;
		//	}
		//}
		//return out;
	}

	template<typename T>
	inline T Clamp(const T& x, const T& low, const T& high)
	{
		return x < low ? low : (x > high ? high : x);
	}
}