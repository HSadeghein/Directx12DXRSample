#pragma once
#include<string>
#include<DirectXMath.h>
#include "MathHelper.h"
class Material
{
public:
	Material();
	~Material();

	std::string Name;
	int MatCBIndex = -1;
	//it can be used when we need textures
	int DiffuseSRVHeapIndex = -1;

	int NumFrameDirty = 3;

	DirectX::XMFLOAT4 DiffuseAlbedo = { 1.0f,1.0f,1.0f,1.0f };
	DirectX::XMFLOAT3 FresnelR0 = { 0.01f,0.01f,0.01f };
	float Roughness = 0.1f;
	DirectX::XMFLOAT4X4 MatTransform = MathHelper::Identity4x4();
	 

};

