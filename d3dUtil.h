#pragma once
#include<d3d12.h>
#include<wrl.h>
#include<DirectXCollision.h>
#include<string>
#include<map>
#include<unordered_map>
#include<d3dcompiler.h>
#include"Helpers.h"
#include"d3dx12.h"
namespace d3dUtil {


	struct SubmeshGeometry
	{
		UINT IndexCount = 0;
		UINT VertexCount = 0;
		UINT StartIndexLocation = 0;

		UINT StartVertexLocation = 0;

		DirectX::BoundingBox Bounds;
	};

	struct MeshGeometry
	{
		std::string Name;

		Microsoft::WRL::ComPtr<ID3DBlob> VertexBufferCPU = nullptr;
		Microsoft::WRL::ComPtr<ID3DBlob> IndexBufferCPU = nullptr;

		Microsoft::WRL::ComPtr<ID3D12Resource> VertexBufferGPU = nullptr;
		Microsoft::WRL::ComPtr<ID3D12Resource> IndexBufferGPU = nullptr;

		Microsoft::WRL::ComPtr<ID3D12Resource> VertexBufferUploader = nullptr;
		Microsoft::WRL::ComPtr<ID3D12Resource> IndexBufferUploader = nullptr;

		UINT VertexByteStride = 0;
		UINT VertexBufferByteSize = 0;
		DXGI_FORMAT IndexFormat = DXGI_FORMAT_R16_UINT;
		UINT IndexBufferByteSize = 0;

		std::unordered_map<std::string, SubmeshGeometry> DrawArgs;

		D3D12_VERTEX_BUFFER_VIEW VertexBufferView() const
		{
			D3D12_VERTEX_BUFFER_VIEW vbv;
			vbv.BufferLocation = VertexBufferGPU->GetGPUVirtualAddress();
			vbv.SizeInBytes = VertexBufferByteSize;
			vbv.StrideInBytes = VertexByteStride;

			return vbv;
		}

		D3D12_INDEX_BUFFER_VIEW IndexBufferView() const
		{
			D3D12_INDEX_BUFFER_VIEW ibv;
			ibv.BufferLocation = IndexBufferGPU->GetGPUVirtualAddress();
			ibv.Format = IndexFormat;
			ibv.SizeInBytes = IndexBufferByteSize;

			return ibv;
		}

		void DisposeUploaders()
		{
			IndexBufferUploader = nullptr;
			VertexBufferUploader = nullptr;
		}
	};

	inline Microsoft::WRL::ComPtr<ID3D12Resource> CreateDefaultBuffer(ID3D12Device* device,
		ID3D12GraphicsCommandList* commandList, const void* initData, UINT64 byteSize,
		Microsoft::WRL::ComPtr<ID3D12Resource>& uploadBuffer)
	{
		Microsoft::WRL::ComPtr<ID3D12Resource> defaultBuffer;

		//destination buffer in GPU
		ThrowifFailed(device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(byteSize),
			D3D12_RESOURCE_STATE_COMMON,
			nullptr,
			IID_PPV_ARGS(defaultBuffer.GetAddressOf())
		));

		//intermediate buffer
		ThrowifFailed(device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(byteSize),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(uploadBuffer.GetAddressOf())
		));

		//the data we want to copy that to GPU
		D3D12_SUBRESOURCE_DATA subResourceData = {};
		subResourceData.pData = initData;
		subResourceData.RowPitch = byteSize;
		subResourceData.SlicePitch = byteSize;

		commandList->ResourceBarrier(1,
			&CD3DX12_RESOURCE_BARRIER::Transition(defaultBuffer.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST));

		UpdateSubresources<1>(commandList, defaultBuffer.Get(), uploadBuffer.Get(), 0, 0, 1, &subResourceData);

		commandList->ResourceBarrier(1,
			&CD3DX12_RESOURCE_BARRIER::Transition(defaultBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COMMON));


		return defaultBuffer;
	}

	inline UINT CalcConstantBufferByteSize(UINT byteSize)
	{
		return (byteSize + 255) & ~255;
	}

	inline Microsoft::WRL::ComPtr<ID3DBlob> CompileShader(
		const std::wstring& filename,
		const D3D_SHADER_MACRO* defines,
		const std::string& entrypoint,
		const std::string& target)
	{
		UINT compileFlags = 0;
#if defined(DEBUG) || defined(_DEBUG)  
		compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

		HRESULT hr = S_OK;

		Microsoft::WRL::ComPtr<ID3DBlob> byteCode = nullptr;
		Microsoft::WRL::ComPtr<ID3DBlob> errors;
		hr = D3DCompileFromFile(filename.c_str(), defines, D3D_COMPILE_STANDARD_FILE_INCLUDE,
			entrypoint.c_str(), target.c_str(), compileFlags, 0, &byteCode, &errors);

		if (errors != nullptr)
			OutputDebugStringA((char*)errors->GetBufferPointer());

		ThrowifFailed(hr);

		return byteCode;
	}
}