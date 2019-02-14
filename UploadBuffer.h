#pragma once
#include"d3dUtil.h"
template<typename T>
class UploadBuffer
{
public:
	
	UploadBuffer(bool isConstantBuffer, ID3D12Device* device, UINT elementCount) :g_IsConstantBufer(isConstantBuffer)
	{
		g_ElementByteSize = sizeof(T);

		if (g_IsConstantBufer)
			g_ElementByteSize = d3dUtil::CalcConstantBufferByteSize(g_ElementByteSize);

		ThrowifFailed(device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(g_ElementByteSize * elementCount),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&g_UploadBuffer)
		));

		ThrowifFailed(g_UploadBuffer->Map(0, nullptr, reinterpret_cast<void**>(&g_MappedData)));
	}

	UploadBuffer(const UploadBuffer& rhs) = delete;
	UploadBuffer& operator=(const UploadBuffer& rhs) = delete;

	~UploadBuffer()
	{
		if (g_UploadBuffer != nullptr)
		{
			g_UploadBuffer->Unmap(0, nullptr);
		}
		g_MappedData = nullptr;
	}

	ID3D12Resource* Resource() const
	{
		return g_UploadBuffer.Get();
	}

	void CopyData(int elementIndex, const T& data) 
	{
		memcpy(&g_MappedData[elementIndex * g_ElementByteSize], &data, sizeof(T));
	}
private:
	Microsoft::WRL::ComPtr<ID3D12Resource> g_UploadBuffer;
	BYTE* g_MappedData = nullptr;

	UINT g_ElementByteSize = 0;
	bool g_IsConstantBufer = false;
};
