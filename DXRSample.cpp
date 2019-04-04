#define NOMINMAX
#include "DXRSample.h"
#include"Application.h"
#include"Window.h"
#include"CommandQueue.h"

#include <wrl.h>
#include "imgui/imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx12.h"

#include "d3dx12.h"
#include <algorithm>
#include<array>


#include "d3d12_1.h"
#include "Raytracing.hlsl.h"
#include "GeometryGenerator.h"

using namespace Microsoft::WRL;

using namespace DirectX;


const wchar_t* DXRSample::c_hitGroupName = L"MyHitGroup";
const wchar_t* DXRSample::c_raygenShaderName = L"MyRaygenShader";
const wchar_t* DXRSample::c_closestHitShaderName = L"MyClosestHitShader";
const wchar_t* DXRSample::c_missShaderName = L"MyMissShader";


// Clamp a value between a min and max range.
template<typename T>
constexpr const T& clamp(const T& val, const T& min, const T& max)
{
	return val < min ? min : val > max ? max : val;
}




static VertexPosColor g_Vertices[8] = {
	{ XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT3(0.0f, 0.0f, 0.0f) }, // 0
	{ XMFLOAT3(-1.0f,  1.0f, -1.0f), XMFLOAT3(0.0f, 1.0f, 0.0f) }, // 1
	{ XMFLOAT3(1.0f,  1.0f, -1.0f), XMFLOAT3(1.0f, 1.0f, 0.0f) }, // 2
	{ XMFLOAT3(1.0f, -1.0f, -1.0f), XMFLOAT3(1.0f, 0.0f, 0.0f) }, // 3
	{ XMFLOAT3(-1.0f, -1.0f,  1.0f), XMFLOAT3(0.0f, 0.0f, 1.0f) }, // 4
	{ XMFLOAT3(-1.0f,  1.0f,  1.0f), XMFLOAT3(0.0f, 1.0f, 1.0f) }, // 5
	{ XMFLOAT3(1.0f,  1.0f,  1.0f), XMFLOAT3(1.0f, 1.0f, 1.0f) }, // 6
	{ XMFLOAT3(1.0f, -1.0f,  1.0f), XMFLOAT3(1.0f, 0.0f, 1.0f) }  // 7
};

static WORD g_Indicies[36] =
{
	0, 1, 2, 0, 2, 3,
	4, 6, 5, 4, 7, 6,
	4, 5, 1, 4, 1, 0,
	3, 2, 6, 3, 6, 7,
	1, 5, 6, 1, 6, 2,
	4, 0, 3, 4, 3, 7
};

DXRSample::DXRSample(const std::wstring & name, int width, int height, bool vSync) :
	super(name, width, height, vSync),
	g_Viewport(CD3DX12_VIEWPORT(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height))),
	g_ScissorRect(CD3DX12_RECT(0, 0, LONG_MAX, LONG_MAX)),
	g_FoV(45.0),
	g_ContentLoaded(false)
{
	g_rayGenCB.viewport = { -1.0f, -1.0f, 1.0f, 1.0f };
	UpdateForSizeChange(width, height);

}


void DXRSample::UpdateBufferResource(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> commandList, ID3D12Resource ** pDestinationResource, ID3D12Resource ** pIntermediateResource, size_t numElements, size_t elementSize, const void * bufferData, D3D12_RESOURCE_FLAGS flags)
{
	auto device = Application::Get().GetDevice();

	const size_t bufferSize = numElements * elementSize;
	ThrowifFailed(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(bufferSize, flags),
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(pDestinationResource)));

	if (bufferData)
	{
		ThrowifFailed(device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(bufferSize),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(pIntermediateResource)));

		D3D12_SUBRESOURCE_DATA subresourceData = {};
		subresourceData.pData = bufferData;
		subresourceData.RowPitch = bufferSize;
		subresourceData.SlicePitch = subresourceData.RowPitch;

		UpdateSubresources(commandList.Get(),
			*pDestinationResource, *pIntermediateResource,
			0, 0, 1, &subresourceData);
	}
}

void DXRSample::BuildPSOs()
{
	//// Load the vertex shader.
	//ComPtr<ID3DBlob> vertexShaderBlob;
	//ThrowifFailed(D3DReadFileToBlob(L"VertexShader.cso", &vertexShaderBlob));

	//// Load the pixel shader.
	//ComPtr<ID3DBlob> pixelShaderBlob;
	//ThrowifFailed(D3DReadFileToBlob(L"PixelShader.cso", &pixelShaderBlob));

	auto device = Application::Get().GetDevice();

	D3D12_GRAPHICS_PIPELINE_STATE_DESC opaquePsoDesc;
	ZeroMemory(&opaquePsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	opaquePsoDesc.InputLayout = { g_InputLayout.data(), (UINT)g_InputLayout.size() };
	opaquePsoDesc.pRootSignature = g_RootSignature.Get();
	opaquePsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(g_Shaders["standardVS"]->GetBufferPointer()),
		g_Shaders["standardVS"]->GetBufferSize()
	};
	opaquePsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(g_Shaders["opaquePS"]->GetBufferPointer()),
		g_Shaders["opaquePS"]->GetBufferSize()
	};
	opaquePsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	opaquePsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	opaquePsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	opaquePsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	opaquePsoDesc.SampleMask = UINT_MAX;
	opaquePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	opaquePsoDesc.NumRenderTargets = 1;
	opaquePsoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	opaquePsoDesc.SampleDesc.Count = 1;
	opaquePsoDesc.SampleDesc.Quality = 0;
	opaquePsoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;

	//Microsoft::WRL::ComPtr<ID3D12PipelineState> x;
	//g_PSOs.insert(std::make_pair< std::string, Microsoft::WRL::ComPtr<ID3D12PipelineState>>("opaque", x.GetAddressOf()));

	ThrowifFailed(device->CreateGraphicsPipelineState(&opaquePsoDesc, IID_PPV_ARGS(&g_PSOs["opaque"])));


	//
	// PSO for opaque wireframe objects.
	//
	D3D12_GRAPHICS_PIPELINE_STATE_DESC opaqueWireFramePsoDesc;
	opaqueWireFramePsoDesc = opaquePsoDesc;
	opaqueWireFramePsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
	ThrowifFailed(device->CreateGraphicsPipelineState(&opaqueWireFramePsoDesc, IID_PPV_ARGS(&g_PSOs["opaque_wireframe"])));
}

void DXRSample::DrawRenderItems(ID3D12GraphicsCommandList * cmdList, const std::vector<RenderItem*>& ritems)
{

	UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));

	auto objectCB = g_CurrFrameResource->ObjectCB->Resource();

	// For each render item...
	for (size_t i = 0; i < ritems.size(); ++i)
	{
		auto ri = ritems[i];

		cmdList->IASetVertexBuffers(0, 1, &ri->Geo->VertexBufferView());
		cmdList->IASetIndexBuffer(&ri->Geo->IndexBufferView());
		cmdList->IASetPrimitiveTopology(ri->PrimitiveType);

		// Offset to the CBV in the descriptor heap for this object and for this frame resource.
		UINT cbvIndex = g_CurrFrameResourceIndex * (UINT)g_AllRenderItems.size() + ri->ObjCBIndex + g_ObjectCBVOffset;
		auto cbvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(g_CBVHeap->GetGPUDescriptorHandleForHeapStart());
		cbvHandle.Offset(cbvIndex, Application::Get().GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));

		cmdList->SetGraphicsRootDescriptorTable(0, cbvHandle);

		cmdList->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
	}
}

void DXRSample::LoadImgui(HANDLE hWnd, ID3D12Device2* device)
{
	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	//io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls

	// Setup Dear ImGui style
	ImGui::StyleColorsDark();
	//ImGui::StyleColorsClassic();

	// Setup Platform/Renderer bindings
	ImGui_ImplWin32_Init(hWnd);
	ImGui_ImplDX12_Init(device , super::g_pWindow->BufferCount,
		DXGI_FORMAT_R8G8B8A8_UNORM,
		g_CBVHeap->GetCPUDescriptorHandleForHeapStart(),
		g_CBVHeap->GetGPUDescriptorHandleForHeapStart());

}

void DXRSample::BuildConstantBufferViews()
{
	auto device = Application::Get().GetDevice();
	UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));

	UINT objCount = (UINT)g_AllRenderItems.size();

	// Need a CBV descriptor for each object for each frame resource.
	for (int frameIndex = 0; frameIndex < NUMBER_OF_FRAME_RESOURCES; ++frameIndex)
	{
		auto objectCB = g_FrameResources[frameIndex]->ObjectCB->Resource();
		for (UINT i = 0; i < objCount; ++i)
		{
			D3D12_GPU_VIRTUAL_ADDRESS cbAddress = objectCB->GetGPUVirtualAddress();

			// Offset to the ith object constant buffer in the buffer.
			cbAddress += i * objCBByteSize;

			// Offset to the object cbv in the descriptor heap.
			int heapIndex = frameIndex * objCount + i + g_ObjectCBVOffset;
			auto handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(g_CBVHeap->GetCPUDescriptorHandleForHeapStart());
			handle.Offset(heapIndex, Application::Get().GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));

			D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
			cbvDesc.BufferLocation = cbAddress;
			cbvDesc.SizeInBytes = objCBByteSize;

			device->CreateConstantBufferView(&cbvDesc, handle);
		}
	}
	UINT passCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(PassConstants));

	// Last three descriptors are the pass CBVs for each frame resource.
	for (int frameIndex = 0; frameIndex < NUMBER_OF_FRAME_RESOURCES; ++frameIndex)
	{
		auto passCB = g_FrameResources[frameIndex]->PassCB->Resource();
		D3D12_GPU_VIRTUAL_ADDRESS cbAddress = passCB->GetGPUVirtualAddress();

		// Offset to the pass cbv in the descriptor heap.
		int heapIndex = g_PassCBVOffset + frameIndex;
		auto handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(g_CBVHeap->GetCPUDescriptorHandleForHeapStart());
		handle.Offset(heapIndex, Application::Get().GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));

		D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
		cbvDesc.BufferLocation = cbAddress;
		cbvDesc.SizeInBytes = passCBByteSize;

		device->CreateConstantBufferView(&cbvDesc, handle);
	}
}

void DXRSample::BuildRootSignature()
{
	auto device = Application::Get().GetDevice();
	D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};
	featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
	if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
	{
		featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
	}

	// Allow input layout and deny unnecessary access to certain pipeline stages.
	D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_STREAM_OUTPUT;

	CD3DX12_DESCRIPTOR_RANGE cbvTable0;
	cbvTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
	CD3DX12_DESCRIPTOR_RANGE cbvTable1;
	cbvTable1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 1);


	CD3DX12_ROOT_PARAMETER rootParameters[2];
	rootParameters[0].InitAsDescriptorTable(1, &cbvTable0);
	rootParameters[1].InitAsDescriptorTable(1, &cbvTable1);


	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(2, rootParameters, 0, nullptr, rootSignatureFlags);
	// Serialize the root signature.
	ComPtr<ID3DBlob> rootSignatureBlob;
	ComPtr<ID3DBlob> errorBlob;
	ThrowifFailed(D3D12SerializeRootSignature(&rootSigDesc,D3D_ROOT_SIGNATURE_VERSION_1,rootSignatureBlob.GetAddressOf(),errorBlob.GetAddressOf()));
	// Create the root signature.
	ThrowifFailed(device->CreateRootSignature(0, rootSignatureBlob->GetBufferPointer(),
		rootSignatureBlob->GetBufferSize(), IID_PPV_ARGS(g_RootSignature.GetAddressOf())));
}

void DXRSample::SerializeAndCreateRaytracingRootSignature(D3D12_ROOT_SIGNATURE_DESC& desc, ComPtr<ID3D12RootSignature>* rootSig)
{
	auto device = Application::Get().GetDevice();
	ComPtr<ID3DBlob> blob;
	ComPtr<ID3DBlob> error;

	if (g_raytracingAPI == RaytracingAPI::FallbackLayer)
	{
		ThrowIfFailed(g_fallbackDevice->D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error), error ? static_cast<wchar_t*>(error->GetBufferPointer()) : nullptr);
		ThrowIfFailed(g_fallbackDevice->CreateRootSignature(1, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&(*rootSig))));
	}
	else // DirectX Raytracing
	{
		ThrowIfFailed(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error), error ? static_cast<wchar_t*>(error->GetBufferPointer()) : nullptr);
		ThrowIfFailed(device->CreateRootSignature(1, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&(*rootSig))));
	}
}

void DXRSample::CreateRaytracingRootSignatures()
{
	// Global Root Signature
	// This is a root signature that is shared across all raytracing shaders invoked during a DispatchRays() call.
	{
		CD3DX12_DESCRIPTOR_RANGE UAVDescriptor;
		UAVDescriptor.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);
		CD3DX12_ROOT_PARAMETER rootParameters[GlobalRootSignatureParams::Count];
		rootParameters[GlobalRootSignatureParams::OutputViewSlot].InitAsDescriptorTable(1, &UAVDescriptor);
		rootParameters[GlobalRootSignatureParams::AccelerationStructureSlot].InitAsShaderResourceView(0);
		CD3DX12_ROOT_SIGNATURE_DESC globalRootSignatureDesc(ARRAYSIZE(rootParameters), rootParameters);
		SerializeAndCreateRaytracingRootSignature(globalRootSignatureDesc, &g_raytracingGlobalRootSignature);
	}

	// Local Root Signature
	// This is a root signature that enables a shader to have unique arguments that come from shader tables.
	{
		CD3DX12_ROOT_PARAMETER rootParameters[LocalRootSignatureParams::Count];
		rootParameters[LocalRootSignatureParams::ViewportConstantSlot].InitAsConstants(SizeOfInUint32(g_rayGenCB), 0, 0);
		CD3DX12_ROOT_SIGNATURE_DESC localRootSignatureDesc(ARRAYSIZE(rootParameters), rootParameters);
		localRootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;
		SerializeAndCreateRaytracingRootSignature(localRootSignatureDesc, &g_raytracingLocalRootSignature);
	}
}

void DXRSample::CreateDescriptorHeaps(ID3D12Device * device)
{
	UINT objCount = (UINT)g_AllRenderItems.size();

	// Need a CBV descriptor for each object for each frame resource,
	// +1 for the perPass CBV for each frame resource. +1 for IMGUI
	UINT numDescriptors = (objCount + 1) * NUMBER_OF_FRAME_RESOURCES + 1 + 3;

	// Save an offset to the start of the pass CBVs.  These are the last 3 descriptors.
	// Allocate a heap for 3 descriptors:
	// 2 - bottom and top level acceleration structure fallback wrapped pointers
	// 1 - raytracing output texture SRV
	// The first oone is for imgui
	// The 2,3,4th is for DXR
	g_RaytracingCBVOffset = 1;
	g_ObjectCBVOffset = 4;
	g_PassCBVOffset = objCount * NUMBER_OF_FRAME_RESOURCES + g_ObjectCBVOffset;



	D3D12_DESCRIPTOR_HEAP_DESC descDSV;
	descDSV.NumDescriptors = 1;
	descDSV.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	descDSV.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	descDSV.NodeMask = 0;
	ThrowifFailed(device->CreateDescriptorHeap(&descDSV, IID_PPV_ARGS(&g_DSVHeap)));

	D3D12_DESCRIPTOR_HEAP_DESC descCBV;
	descCBV.NumDescriptors = numDescriptors;
	descCBV.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	descCBV.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	descCBV.NodeMask = 0;
	ThrowifFailed(device->CreateDescriptorHeap(&descCBV, IID_PPV_ARGS(&g_CBVHeap)));
}

void DXRSample::BuildFrameResources()
{
	auto device = Application::Get().GetDevice();
	for (size_t i = 0; i < NUMBER_OF_FRAME_RESOURCES; i++)
	{
		g_FrameResources.push_back(std::make_unique<FrameResource>(device.Get(), 1, (UINT)g_AllRenderItems.size()));
	}
}
bool DXRSample::LoadContent()
{
	auto device = Application::Get().GetDevice();
	auto commandQueue = Application::Get().GetCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT);
	auto commandList = commandQueue->GetCommandList();
	auto adapter = Application::Get().GetAdapter();
	Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter1;
	adapter.As(&adapter1);






	BuildRootSignature();
	BuildShadersAndInputLayout();
	//BuildShapeGeometry(commandList.Get());
	BuildTriangle(commandList.Get());
	BuildRenderTriangleItem();
	BuildFrameResources();
	// Create the descriptor heap for the depth-stencil view and constatn buffer
	CreateDescriptorHeaps(device.Get());
	//LoadImgui
	LoadImgui(super::g_pWindow->GetWindowHandle(), device.Get());
	BuildConstantBufferViews();
	BuildPSOs();
	auto fenceValue = commandQueue->ExecuteCommandList(commandList);
	commandQueue->WaitForFenceValue(fenceValue);
	commandQueue->Flush();

	CreateRaytracingDeviceDependentResources();
	//commandList = commandQueue->GetCommandList();


	
	g_ContentLoaded = true;

	// Resize/Create the depth buffer.
	ResizeDepthBuffer(super::GetWidth(), super::GetHeight());

	return true;
}


// Create resources that depend on the device.
void DXRSample::CreateRaytracingDeviceDependentResources()
{
	//auto device = Application::Get().GetDevice();
	//auto commandQueue = Application::Get().GetCommandQueue();
	//auto commandList = commandQueue->GetCommandList();
	// Initialize raytracing pipeline.

	// Create raytracing interfaces: raytracing device and commandlist.
	CreateRaytracingDeviceCommandlist();

	// Create root signatures for the shaders.
	CreateRaytracingRootSignatures();

	// Create a raytracing pipeline state object which defines the binding of shaders, state and resources to be used during raytracing.
	CreateRaytracingPipelineStateObject();



	// Build raytracing acceleration structures from the generated geometry.
	BuildAccelerationStructures();

	// Build shader tables, which define shaders and their local root arguments.
	BuildShaderTables();

	// Create an output 2D texture to store the raytracing result to.
	CreateRaytracingOutputResource();
}


void DXRSample::BuildShapeGeometry(ID3D12GraphicsCommandList* commandList)
{
	auto device = Application::Get().GetDevice();

	GeometryGenerator geoGen;
	GeometryGenerator::MeshData box = geoGen.CreateBox(1.5f, 2.0f, 1.5f, 3);
	GeometryGenerator::MeshData grid = geoGen.CreateGrid(20.0f, 30.0f, 60, 40);
	GeometryGenerator::MeshData sphere = geoGen.CreateSphere(0.5f, 20, 20);
	GeometryGenerator::MeshData cylinder = geoGen.CreateCylinder(0.5f, 0.3f, 3.0f, 20, 20);

	//
	// We are concatenating all the geometry into one big vertex/index buffer.  So
	// define the regions in the buffer each submesh covers.
	//

	// Cache the vertex offsets to each object in the concatenated vertex buffer.
	UINT boxVertexOffset = 0;
	UINT gridVertexOffset = (UINT)box.Vertices.size();
	UINT sphereVertexOffset = gridVertexOffset + (UINT)grid.Vertices.size();
	UINT cylinderVertexOffset = sphereVertexOffset + (UINT)sphere.Vertices.size();

	// Cache the starting index for each object in the concatenated index buffer.
	UINT boxIndexOffset = 0;
	UINT gridIndexOffset = (UINT)box.Indices32.size();
	UINT sphereIndexOffset = gridIndexOffset + (UINT)grid.Indices32.size();
	UINT cylinderIndexOffset = sphereIndexOffset + (UINT)sphere.Indices32.size();

	// Define the SubmeshGeometry that cover different 
	// regions of the vertex/index buffers.

	d3dUtil::SubmeshGeometry boxSubmesh;
	boxSubmesh.IndexCount = (UINT)box.Indices32.size();
	boxSubmesh.StartIndexLocation = boxIndexOffset;
	boxSubmesh.StartVertexLocation = boxVertexOffset;

	d3dUtil::SubmeshGeometry gridSubmesh;
	gridSubmesh.IndexCount = (UINT)grid.Indices32.size();
	gridSubmesh.StartIndexLocation = gridIndexOffset;
	gridSubmesh.StartVertexLocation = gridVertexOffset;

	d3dUtil::SubmeshGeometry sphereSubmesh;
	sphereSubmesh.IndexCount = (UINT)sphere.Indices32.size();
	sphereSubmesh.StartIndexLocation = sphereIndexOffset;
	sphereSubmesh.StartVertexLocation = sphereVertexOffset;

	d3dUtil::SubmeshGeometry cylinderSubmesh;
	cylinderSubmesh.IndexCount = (UINT)cylinder.Indices32.size();
	cylinderSubmesh.StartIndexLocation = cylinderIndexOffset;
	cylinderSubmesh.StartVertexLocation = cylinderVertexOffset;

	//
	// Extract the vertex elements we are interested in and pack the
	// vertices of all the meshes into one vertex buffer.
	//

	auto totalVertexCount =
		box.Vertices.size() +
		grid.Vertices.size() +
		sphere.Vertices.size() +
		cylinder.Vertices.size();

	std::vector<VertexPosColor> vertices(totalVertexCount);

	UINT k = 0;
	for (size_t i = 0; i < box.Vertices.size(); ++i, ++k)
	{
		vertices[k].position = box.Vertices[i].Position;
		vertices[k].color = XMFLOAT3(1.0, 1.0, 0.0);
	}

	for (size_t i = 0; i < grid.Vertices.size(); ++i, ++k)
	{
		vertices[k].position = grid.Vertices[i].Position;
		vertices[k].color = XMFLOAT3(1.0, 1.0, 0.0);
	}

	for (size_t i = 0; i < sphere.Vertices.size(); ++i, ++k)
	{
		vertices[k].position = sphere.Vertices[i].Position;
		vertices[k].color = XMFLOAT3(1.0, 1.0, 0.0);
	}

	for (size_t i = 0; i < cylinder.Vertices.size(); ++i, ++k)
	{
		vertices[k].position = cylinder.Vertices[i].Position;
		vertices[k].color = XMFLOAT3(1.0, 1.0, 0.0);
	}

	std::vector<std::uint16_t> indices;
	indices.insert(indices.end(), std::begin(box.GetIndices16()), std::end(box.GetIndices16()));
	indices.insert(indices.end(), std::begin(grid.GetIndices16()), std::end(grid.GetIndices16()));
	indices.insert(indices.end(), std::begin(sphere.GetIndices16()), std::end(sphere.GetIndices16()));
	indices.insert(indices.end(), std::begin(cylinder.GetIndices16()), std::end(cylinder.GetIndices16()));

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(VertexPosColor);
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	auto geo = std::make_unique<d3dUtil::MeshGeometry>();
	geo->Name = "shapeGeo";

	ThrowifFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowifFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(device.Get(),
		commandList, vertices.data(), vbByteSize, geo->VertexBufferUploader);

	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(device.Get(),
		commandList, indices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(VertexPosColor);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	geo->DrawArgs["box"] = boxSubmesh;
	geo->DrawArgs["grid"] = gridSubmesh;
	geo->DrawArgs["sphere"] = sphereSubmesh;
	geo->DrawArgs["cylinder"] = cylinderSubmesh;

	g_Geometries[geo->Name] = std::move(geo);
}

void DXRSample::BuildTriangle(ID3D12GraphicsCommandList* commandList)
{
	using Vertex = GeometryGenerator::Vertex;
	auto device = Application::Get().GetDevice();



	Vertex vertices[3];
	GeometryGenerator::MeshData triangle;

	vertices[0] = Vertex(-1.0f, -1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
	vertices[1] = Vertex(1.0f, -1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
	vertices[2] = Vertex(0.f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
	
	std::uint32_t indices[3];
	indices[0] = 1; indices[1] = 0; indices[2] = 2;

	triangle.Vertices.assign(&vertices[0], &vertices[3]);
	triangle.Indices32.assign(&indices[0], &indices[3]);

	d3dUtil::SubmeshGeometry subMesh;
	subMesh.IndexCount = (UINT)triangle.Indices32.size();
	subMesh.StartIndexLocation = 0;
	subMesh.StartVertexLocation = 0;

	std::vector<VertexPosColor> totalVertices(triangle.Vertices.size());
	std::vector<std::uint16_t> totalIndices;

	totalIndices.insert(totalIndices.end(), std::begin(triangle.GetIndices16()), std::end(triangle.GetIndices16()));
	for (size_t i = 0; i < triangle.Vertices.size(); i++)
	{
		totalVertices[i].position = triangle.Vertices[i].Position;
		totalVertices[i].color = XMFLOAT3(1, 1, 1);
	}

	const UINT vbByteSize = (UINT)totalVertices.size() * sizeof(VertexPosColor);
	const UINT ibByteSize = (UINT)totalIndices.size() * sizeof(std::uint16_t);

	auto geo = std::make_unique<d3dUtil::MeshGeometry>();
	geo->Name = "triangleGeo";

	ThrowifFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), totalVertices.data(), vbByteSize);

	ThrowifFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), totalIndices.data(), ibByteSize);

	geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(device.Get(),
		commandList, totalVertices.data(), vbByteSize, geo->VertexBufferUploader);

	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(device.Get(),
		commandList, totalIndices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(VertexPosColor);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	geo->DrawArgs["triangle"] = subMesh;
	g_Geometries[geo->Name] = std::move(geo);
}

void DXRSample::BuildRenderTriangleItem()
{
	auto triangleRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&triangleRitem->WorldMatrix, XMMatrixScaling(2.0f, 2.0f, 2.0f)*XMMatrixTranslation(0.0f, 0.5f, 0.0f));
	triangleRitem->ObjCBIndex = 0;
	triangleRitem->Geo = g_Geometries["triangleGeo"].get();
	triangleRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	triangleRitem->IndexCount = triangleRitem->Geo->DrawArgs["triangle"].IndexCount;
	triangleRitem->StartIndexLocation = triangleRitem->Geo->DrawArgs["triangle"].StartIndexLocation;
	triangleRitem->BaseVertexLocation = triangleRitem->Geo->DrawArgs["triangle"].StartVertexLocation;
	g_AllRenderItems.push_back(std::move(triangleRitem));

	for (auto& e : g_AllRenderItems)
		g_AllOpaqueItems.push_back(e.get());
}

void DXRSample::UnloadContent()
{
	Application::Get().GetCommandQueue()->Flush();
	ReleaseWindowSizeDependentResource();
	ReleaseDeviceDependentResource();
	g_ContentLoaded = false;
}

void DXRSample::ReleaseWindowSizeDependentResource()
{
	m_rayGenShaderTable.Reset();
	m_missShaderTable.Reset();
	m_hitGroupShaderTable.Reset();
	g_raytracingOutput.Reset();
}

void DXRSample::ReleaseDeviceDependentResource()
{
	g_fallbackDevice.Reset();
	g_fallbackCommandList.Reset();
	g_fallbackStateObject.Reset();
	g_raytracingGlobalRootSignature.Reset();
	g_raytracingLocalRootSignature.Reset();

	g_dxrDevice.Reset();
	g_dxrCommandList.Reset();
	g_dxrStateObject.Reset();

	g_CBVHeap.Reset();
	g_RaytracingDescriptorsAllocated = 0;
	g_raytracingOutputResourceUAVDescriptorHeapIndex = UINT_MAX;
	//m_indexBuffer.Reset();
	//m_vertexBuffer.Reset();

	g_accelerationStructure.Reset();
	g_bottomLevelAccelerationStructure.Reset();
	g_topLevelAccelerationStructure.Reset();
}

// Create resources that are dependent on the size of the main window.
void DXRSample::CreateRaytracingWindowSizeDependentResources()
{
	CreateRaytracingOutputResource();

	// For simplicity, we will rebuild the shader tables.
	BuildShaderTables();
}

void DXRSample::OnResize(ResizeEventArgs& e)
{
	std::wstring s = L"Client Width" + std::to_wstring(super::GetWidth()) + L"\n";
	OutputDebugString(s.c_str());
	s = L"Width" + std::to_wstring(e.Width) + L"\n";
	OutputDebugString(s.c_str());
	if (e.Width != super::GetWidth() || e.Height != super::GetHeight())
	{
		super::OnResize(e);
		UpdateForSizeChange(e.Width, e.Height);
		ReleaseWindowSizeDependentResource();
		CreateRaytracingWindowSizeDependentResources();

		ImGui::SetWindowPos("Hello, world!", ImVec2(e.Width / 2, e.Height / 2));
		g_Viewport = CD3DX12_VIEWPORT(0.0f, 0.0f,
			static_cast<float>(e.Width), static_cast<float>(e.Height));

		ResizeDepthBuffer(e.Width, e.Height);
	}

}

void DXRSample::BuildRenderItems()
{
	auto boxRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&boxRitem->WorldMatrix, XMMatrixScaling(2.0f, 2.0f, 2.0f)*XMMatrixTranslation(0.0f, 0.5f, 0.0f));
	boxRitem->ObjCBIndex = 0;
	boxRitem->Geo = g_Geometries["shapeGeo"].get();
	boxRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	boxRitem->IndexCount = boxRitem->Geo->DrawArgs["box"].IndexCount;
	boxRitem->StartIndexLocation = boxRitem->Geo->DrawArgs["box"].StartIndexLocation;
	boxRitem->BaseVertexLocation = boxRitem->Geo->DrawArgs["box"].StartVertexLocation;
	g_AllRenderItems.push_back(std::move(boxRitem));
	
	auto gridRitem = std::make_unique<RenderItem>();
	gridRitem->WorldMatrix = MathHelper::Identity4x4();
	gridRitem->ObjCBIndex = 1;
	gridRitem->Geo = g_Geometries["shapeGeo"].get();
	gridRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	gridRitem->IndexCount = gridRitem->Geo->DrawArgs["grid"].IndexCount;
	gridRitem->StartIndexLocation = gridRitem->Geo->DrawArgs["grid"].StartIndexLocation;
	gridRitem->BaseVertexLocation = gridRitem->Geo->DrawArgs["grid"].StartVertexLocation;
	g_AllRenderItems.push_back(std::move(gridRitem));

	UINT objCBIndex = 2;
	for (int i = 0; i < 5; ++i)
	{
		auto leftCylRitem = std::make_unique<RenderItem>();
		auto rightCylRitem = std::make_unique<RenderItem>();
		auto leftSphereRitem = std::make_unique<RenderItem>();
		auto rightSphereRitem = std::make_unique<RenderItem>();

		XMMATRIX leftCylWorld = XMMatrixTranslation(-5.0f, 1.5f, -10.0f + i * 5.0f);
		XMMATRIX rightCylWorld = XMMatrixTranslation(+5.0f, 1.5f, -10.0f + i * 5.0f);

		XMMATRIX leftSphereWorld = XMMatrixTranslation(-5.0f, 3.5f, -10.0f + i * 5.0f);
		XMMATRIX rightSphereWorld = XMMatrixTranslation(+5.0f, 3.5f, -10.0f + i * 5.0f);

		XMStoreFloat4x4(&leftCylRitem->WorldMatrix, rightCylWorld);
		leftCylRitem->ObjCBIndex = objCBIndex++;
		leftCylRitem->Geo = g_Geometries["shapeGeo"].get();
		leftCylRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		leftCylRitem->IndexCount = leftCylRitem->Geo->DrawArgs["cylinder"].IndexCount;
		leftCylRitem->StartIndexLocation = leftCylRitem->Geo->DrawArgs["cylinder"].StartIndexLocation;
		leftCylRitem->BaseVertexLocation = leftCylRitem->Geo->DrawArgs["cylinder"].StartVertexLocation;

		XMStoreFloat4x4(&rightCylRitem->WorldMatrix, leftCylWorld);
		rightCylRitem->ObjCBIndex = objCBIndex++;
		rightCylRitem->Geo = g_Geometries["shapeGeo"].get();
		rightCylRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		rightCylRitem->IndexCount = rightCylRitem->Geo->DrawArgs["cylinder"].IndexCount;
		rightCylRitem->StartIndexLocation = rightCylRitem->Geo->DrawArgs["cylinder"].StartIndexLocation;
		rightCylRitem->BaseVertexLocation = rightCylRitem->Geo->DrawArgs["cylinder"].StartVertexLocation;

		XMStoreFloat4x4(&leftSphereRitem->WorldMatrix, leftSphereWorld);
		leftSphereRitem->ObjCBIndex = objCBIndex++;
		leftSphereRitem->Geo = g_Geometries["shapeGeo"].get();
		leftSphereRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		leftSphereRitem->IndexCount = leftSphereRitem->Geo->DrawArgs["sphere"].IndexCount;
		leftSphereRitem->StartIndexLocation = leftSphereRitem->Geo->DrawArgs["sphere"].StartIndexLocation;
		leftSphereRitem->BaseVertexLocation = leftSphereRitem->Geo->DrawArgs["sphere"].StartVertexLocation;

		XMStoreFloat4x4(&rightSphereRitem->WorldMatrix, rightSphereWorld);
		rightSphereRitem->ObjCBIndex = objCBIndex++;
		rightSphereRitem->Geo = g_Geometries["shapeGeo"].get();
		rightSphereRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		rightSphereRitem->IndexCount = rightSphereRitem->Geo->DrawArgs["sphere"].IndexCount;
		rightSphereRitem->StartIndexLocation = rightSphereRitem->Geo->DrawArgs["sphere"].StartIndexLocation;
		rightSphereRitem->BaseVertexLocation = rightSphereRitem->Geo->DrawArgs["sphere"].StartVertexLocation;

		g_AllRenderItems.push_back(std::move(leftCylRitem));
		g_AllRenderItems.push_back(std::move(rightCylRitem));
		g_AllRenderItems.push_back(std::move(leftSphereRitem));
		g_AllRenderItems.push_back(std::move(rightSphereRitem));
	}

	
	for (auto& e : g_AllRenderItems)
		g_AllOpaqueItems.push_back(e.get());

}


void DXRSample::BuildShadersAndInputLayout()
{


	g_Shaders["standardVS"] = d3dUtil::CompileShader(L"color.hlsl", nullptr, "vertex", "vs_5_1");
	g_Shaders["opaquePS"] = d3dUtil::CompileShader(L"color.hlsl", nullptr, "pixel", "ps_5_1");

	// Create the vertex input layout
	g_InputLayout = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};


}

// Create 2D output texture for raytracing.
void DXRSample::CreateRaytracingOutputResource()
{
	auto device = Application::Get().GetDevice();
	auto backbufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	
	// Create the output resource. The dimensions and format should match the swap-chain.
	auto uavDesc = CD3DX12_RESOURCE_DESC::Tex2D(backbufferFormat, g_pWindow->GetClientWidth(), g_pWindow->GetClientHeight(), 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

	auto defaultHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	ThrowIfFailed(device->CreateCommittedResource(
		&defaultHeapProperties, D3D12_HEAP_FLAG_NONE, &uavDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&g_raytracingOutput)));
	NAME_D3D12_OBJECT(g_raytracingOutput);

	D3D12_CPU_DESCRIPTOR_HANDLE uavDescriptorHandle;
	g_raytracingOutputResourceUAVDescriptorHeapIndex = AllocateDescriptor(&uavDescriptorHandle, g_raytracingOutputResourceUAVDescriptorHeapIndex);
	D3D12_UNORDERED_ACCESS_VIEW_DESC UAVDesc = {};
	UAVDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	device->CreateUnorderedAccessView(g_raytracingOutput.Get(), nullptr, &UAVDesc, uavDescriptorHandle);
	g_raytracingOutputResourceUAVGpuDescriptor = CD3DX12_GPU_DESCRIPTOR_HANDLE(g_CBVHeap->GetGPUDescriptorHandleForHeapStart(), g_raytracingOutputResourceUAVDescriptorHeapIndex, Application::Get().GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
}

void DXRSample::BuildShaderTables()
{
	auto device = Application::Get().GetDevice();

	void* rayGenShaderIdentifier;
	void* missShaderIdentifier;
	void* hitGroupShaderIdentifier;

	auto GetShaderIdentifiers = [&](auto* stateObjectProperties)
	{
		rayGenShaderIdentifier = stateObjectProperties->GetShaderIdentifier(c_raygenShaderName);
		missShaderIdentifier = stateObjectProperties->GetShaderIdentifier(c_missShaderName);
		hitGroupShaderIdentifier = stateObjectProperties->GetShaderIdentifier(c_hitGroupName);
	};

	// Get shader identifiers.
	UINT shaderIdentifierSize;
	if (g_raytracingAPI == RaytracingAPI::FallbackLayer)
	{
		GetShaderIdentifiers(g_fallbackStateObject.Get());
		shaderIdentifierSize = g_fallbackDevice->GetShaderIdentifierSize();
	}
	else // DirectX Raytracing
	{
		ComPtr<ID3D12StateObjectPropertiesPrototype> stateObjectProperties;
		ThrowIfFailed(g_dxrStateObject.As(&stateObjectProperties));
		GetShaderIdentifiers(stateObjectProperties.Get());
		shaderIdentifierSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
	}

	// Ray gen shader table
	{
		struct RootArguments {
			RayGenConstantBuffer cb;
		} rootArguments;
		rootArguments.cb = g_rayGenCB;

		UINT numShaderRecords = 1;
		UINT shaderRecordSize = shaderIdentifierSize + sizeof(rootArguments);
		ShaderTable rayGenShaderTable(device.Get(), numShaderRecords, shaderRecordSize, L"RayGenShaderTable");
		rayGenShaderTable.push_back(ShaderRecord(rayGenShaderIdentifier, shaderIdentifierSize, &rootArguments, sizeof(rootArguments)));
		m_rayGenShaderTable = rayGenShaderTable.GetResource();
	}

	// Miss shader table
	{
		UINT numShaderRecords = 1;
		UINT shaderRecordSize = shaderIdentifierSize;
		ShaderTable missShaderTable(device.Get(), numShaderRecords, shaderRecordSize, L"MissShaderTable");
		missShaderTable.push_back(ShaderRecord(missShaderIdentifier, shaderIdentifierSize));
		m_missShaderTable = missShaderTable.GetResource();
	}

	// Hit group shader table
	{
		UINT numShaderRecords = 1;
		UINT shaderRecordSize = shaderIdentifierSize;
		ShaderTable hitGroupShaderTable(device.Get(), numShaderRecords, shaderRecordSize, L"HitGroupShaderTable");
		hitGroupShaderTable.push_back(ShaderRecord(hitGroupShaderIdentifier, shaderIdentifierSize));
		m_hitGroupShaderTable = hitGroupShaderTable.GetResource();
	}
}

void DXRSample::ResizeDepthBuffer(int width, int height)
{
	if (g_ContentLoaded)
	{
		// Flush any GPU commands that might be referencing the depth buffer.
		Application::Get().Flush();

		width = std::max(1, width);
		height = std::max(1, height);

		auto device = Application::Get().GetDevice();

		// Resize screen dependent resources.
		// Create a depth buffer.
		D3D12_CLEAR_VALUE optimizedClearValue = {};
		optimizedClearValue.Format = DXGI_FORMAT_D32_FLOAT;
		optimizedClearValue.DepthStencil = { 1.0f, 0 };

		ThrowifFailed(device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D32_FLOAT, width, height,
				1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL),
			D3D12_RESOURCE_STATE_DEPTH_WRITE,
			&optimizedClearValue,
			IID_PPV_ARGS(&g_DepthBuffer)
		));
		
		// Update the depth-stencil view.
		D3D12_DEPTH_STENCIL_VIEW_DESC dsv = {};
		dsv.Format = DXGI_FORMAT_D32_FLOAT;
		dsv.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		dsv.Texture2D.MipSlice = 0;
		dsv.Flags = D3D12_DSV_FLAG_NONE;

		device->CreateDepthStencilView(g_DepthBuffer.Get(), &dsv,
			g_DSVHeap->GetCPUDescriptorHandleForHeapStart());
		
	}
}
//Create raytracing commandlist and device

void DXRSample::CreateRaytracingDeviceCommandlist()
{
	auto device = Application::Get().GetDevice();
	auto commandQueue = Application::Get().GetCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT);
	auto commandList = commandQueue->GetCommandList();
	
	if (g_raytracingAPI == RaytracingAPI::FallbackLayer)
	{
		CreateRaytracingFallbackDeviceFlags createDeviceFlags = g_forceComputeFallback ? CreateRaytracingFallbackDeviceFlags::ForceComputeFallback
			: CreateRaytracingFallbackDeviceFlags::None;

		ThrowifFailed(D3D12CreateRaytracingFallbackDevice(device.Get(), createDeviceFlags, 0, IID_PPV_ARGS(g_fallbackDevice.GetAddressOf())));
		g_fallbackDevice->QueryRaytracingCommandList(commandList.Get(), IID_PPV_ARGS(g_fallbackCommandList.GetAddressOf()));
	}
	else //DXR
	{
		ThrowifFailed(device->QueryInterface(IID_PPV_ARGS(g_dxrDevice.GetAddressOf())));
		ThrowifFailed(commandList->QueryInterface(IID_PPV_ARGS(g_dxrCommandList.GetAddressOf())));
	}

	UINT currentBackBufferIndex = g_pWindow->GetCurrentBackBufferIndex();

	g_FenceValues[currentBackBufferIndex] = commandQueue->ExecuteCommandList(commandList);
	commandQueue->WaitForFenceValue(g_FenceValues[currentBackBufferIndex]);
}



void DXRSample::CreateRaytracingPipelineStateObject()
{
	// Create 7 subobjects that combine into a RTPSO:
   // Subobjects need to be associated with DXIL exports (i.e. shaders) either by way of default or explicit associations.
   // Default association applies to every exported shader entrypoint that doesn't have any of the same type of subobject associated with it.
   // This simple sample utilizes default shader association except for local root signature subobject
   // which has an explicit association specified purely for demonstration purposes.
   // 1 - DXIL library
   // 1 - Triangle hit group
   // 1 - Shader config
   // 2 - Local root signature and association
   // 1 - Global root signature
   // 1 - Pipeline config
	CD3D12_STATE_OBJECT_DESC raytracingPipeline{ D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE };


	// DXIL library
	// This contains the shaders and their entrypoints for the state object.
	// Since shaders are not considered a subobject, they need to be passed in via DXIL library subobjects.
	auto lib = raytracingPipeline.CreateSubobject<CD3D12_DXIL_LIBRARY_SUBOBJECT>();
	D3D12_SHADER_BYTECODE libdxil = CD3DX12_SHADER_BYTECODE((void *)g_pRaytracing, ARRAYSIZE(g_pRaytracing));
	lib->SetDXILLibrary(&libdxil);
	// Define which shader exports to surface from the library.
	// If no shader exports are defined for a DXIL library subobject, all shaders will be surfaced.
	// In this sample, this could be omitted for convenience since the sample uses all shaders in the library. 
	{
		lib->DefineExport(c_raygenShaderName);
		lib->DefineExport(c_closestHitShaderName);
		lib->DefineExport(c_missShaderName);
	}

	// Triangle hit group
	// A hit group specifies closest hit, any hit and intersection shaders to be executed when a ray intersects the geometry's triangle/AABB.
	// In this sample, we only use triangle geometry with a closest hit shader, so others are not set.
	auto hitGroup = raytracingPipeline.CreateSubobject<CD3D12_HIT_GROUP_SUBOBJECT>();
	hitGroup->SetClosestHitShaderImport(c_closestHitShaderName);
	hitGroup->SetHitGroupExport(c_hitGroupName);
	hitGroup->SetHitGroupType(D3D12_HIT_GROUP_TYPE_TRIANGLES);

	// Shader config
	// Defines the maximum sizes in bytes for the ray payload and attribute structure.
	auto shaderConfig = raytracingPipeline.CreateSubobject<CD3D12_RAYTRACING_SHADER_CONFIG_SUBOBJECT>();
	UINT payloadSize = 4 * sizeof(float);   // float4 color
	UINT attributeSize = 2 * sizeof(float); // float2 barycentrics
	shaderConfig->Config(payloadSize, attributeSize);

	// Local root signature and shader association
	CreateLocalRootSignatureSubobjects(&raytracingPipeline);
	// This is a root signature that enables a shader to have unique arguments that come from shader tables.

	// Global root signature
	// This is a root signature that is shared across all raytracing shaders invoked during a DispatchRays() call.
	auto globalRootSignature = raytracingPipeline.CreateSubobject<CD3D12_GLOBAL_ROOT_SIGNATURE_SUBOBJECT>();
	globalRootSignature->SetRootSignature(g_raytracingGlobalRootSignature.Get());

	// Pipeline config
	// Defines the maximum TraceRay() recursion depth.
	auto pipelineConfig = raytracingPipeline.CreateSubobject<CD3D12_RAYTRACING_PIPELINE_CONFIG_SUBOBJECT>();
	// PERFOMANCE TIP: Set max recursion depth as low as needed 
	// as drivers may apply optimization strategies for low recursion depths. 
	UINT maxRecursionDepth = 1; // ~ primary rays only. 
	pipelineConfig->Config(maxRecursionDepth);

#if _DEBUG
	PrintStateObjectDesc(raytracingPipeline);
#endif

	// Create the state object.
	if (g_raytracingAPI == RaytracingAPI::FallbackLayer)
	{
		ThrowifFailed(g_fallbackDevice->CreateStateObject(raytracingPipeline, IID_PPV_ARGS(&g_fallbackStateObject)));
	}
	else // DirectX Raytracing
	{
		ThrowifFailed(g_dxrDevice->CreateStateObject(raytracingPipeline, IID_PPV_ARGS(&g_dxrStateObject)));
	}
}

// Local root signature and shader association
// This is a root signature that enables a shader to have unique arguments that come from shader tables.
void DXRSample::CreateLocalRootSignatureSubobjects(CD3D12_STATE_OBJECT_DESC* raytracingPipeline)
{
	// Hit group and miss shaders in this sample are not using a local root signature and thus one is not associated with them.

	// Local root signature to be used in a ray gen shader.
	{
		auto localRootSignature = raytracingPipeline->CreateSubobject<CD3D12_LOCAL_ROOT_SIGNATURE_SUBOBJECT>();
		localRootSignature->SetRootSignature(g_raytracingLocalRootSignature.Get());
		// Shader association
		auto rootSignatureAssociation = raytracingPipeline->CreateSubobject<CD3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION_SUBOBJECT>();
		rootSignatureAssociation->SetSubobjectToAssociate(*localRootSignature);
		rootSignatureAssociation->AddExport(c_raygenShaderName);
	}
}

// Build acceleration structures needed for raytracing.

void DXRSample::BuildAccelerationStructures()
{
	auto device = Application::Get().GetDevice();
	auto commandQueue = Application::Get().GetCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT);
	auto commandList = commandQueue->GetCommandList();
	//ID3D12CommandAllocator* commandAllocator;
	//UINT dataSize = sizeof(commandAllocator);
	//ThrowifFailed(commandList->GetPrivateData(__uuidof(ID3D12CommandAllocator), &dataSize, &commandAllocator));
	////Reset the command list for the acceleration structure construction.
	//commandList->Close();
	//commandList->Reset(commandAllocator, nullptr);


	auto geometry = g_Geometries["triangleGeo"].get();
	D3D12_RAYTRACING_GEOMETRY_DESC geometryDesc = {};
	geometryDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
	geometryDesc.Triangles.IndexBuffer = geometry->IndexBufferGPU->GetGPUVirtualAddress();
	geometryDesc.Triangles.IndexCount = static_cast<UINT>(geometry->IndexBufferGPU->GetDesc().Width) / sizeof(std::uint16_t);
	geometryDesc.Triangles.IndexFormat = DXGI_FORMAT_R16_UINT;
	geometryDesc.Triangles.Transform3x4 = 0;
	geometryDesc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;

	geometryDesc.Triangles.VertexCount = static_cast<UINT>(geometry->VertexBufferGPU->GetDesc().Width) / sizeof(VertexPosColor);
	geometryDesc.Triangles.VertexBuffer.StartAddress = geometry->VertexBufferGPU->GetGPUVirtualAddress();
	geometryDesc.Triangles.VertexBuffer.StrideInBytes = sizeof(VertexPosColor);

	// Mark the geometry as opaque. 
	// PERFORMANCE TIP: mark geometry as opaque whenever applicable as it can enable important ray processing optimizations.
	// Note: When rays encounter opaque geometry an any hit shader will not be executed whether it is present or not.
	geometryDesc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;

	// Get required sizes for an acceleration structure.
	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS topLevelInputs = {};
	topLevelInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	topLevelInputs.Flags = buildFlags;
	topLevelInputs.NumDescs = 1;
	topLevelInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;

	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO topLevelPrebuildInfo = {};
	if (g_raytracingAPI == RaytracingAPI::FallbackLayer)
	{
		g_fallbackDevice->GetRaytracingAccelerationStructurePrebuildInfo(&topLevelInputs, &topLevelPrebuildInfo);
	}
	else // DirectX Raytracing
	{
		g_dxrDevice->GetRaytracingAccelerationStructurePrebuildInfo(&topLevelInputs, &topLevelPrebuildInfo);
	}
	ThrowifFailed(topLevelPrebuildInfo.ResultDataMaxSizeInBytes > 0);

	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO bottomLevelPrebuildInfo = {};
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS bottomLevelInputs = topLevelInputs;
	bottomLevelInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
	bottomLevelInputs.pGeometryDescs = &geometryDesc;
	if (g_raytracingAPI == RaytracingAPI::FallbackLayer)
	{
		g_fallbackDevice->GetRaytracingAccelerationStructurePrebuildInfo(&bottomLevelInputs, &bottomLevelPrebuildInfo);
	}
	else // DirectX Raytracing
	{
		g_dxrDevice->GetRaytracingAccelerationStructurePrebuildInfo(&bottomLevelInputs, &bottomLevelPrebuildInfo);
	}
	ThrowifFailed(bottomLevelPrebuildInfo.ResultDataMaxSizeInBytes > 0);

	ComPtr<ID3D12Resource> scratchResource;
	AllocateUAVBuffer(device.Get(), std::max(topLevelPrebuildInfo.ScratchDataSizeInBytes, bottomLevelPrebuildInfo.ScratchDataSizeInBytes), &scratchResource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, L"ScratchResource");

	// Allocate resources for acceleration structures.
	// Acceleration structures can only be placed in resources that are created in the default heap (or custom heap equivalent). 
	// Default heap is OK since the application doesn’t need CPU read/write access to them. 
	// The resources that will contain acceleration structures must be created in the state D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, 
	// and must have resource flag D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS. The ALLOW_UNORDERED_ACCESS requirement simply acknowledges both: 
	//  - the system will be doing this type of access in its implementation of acceleration structure builds behind the scenes.
	//  - from the app point of view, synchronization of writes/reads to acceleration structures is accomplished using UAV barriers.
	{
		D3D12_RESOURCE_STATES initialResourceState;
		if (g_raytracingAPI == RaytracingAPI::FallbackLayer)
		{
			initialResourceState = g_fallbackDevice->GetAccelerationStructureResourceState();
		}
		else // DirectX Raytracing
		{
			initialResourceState = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
		}

		AllocateUAVBuffer(device.Get(), bottomLevelPrebuildInfo.ResultDataMaxSizeInBytes, &g_bottomLevelAccelerationStructure, initialResourceState, L"BottomLevelAccelerationStructure");
		AllocateUAVBuffer(device.Get(), topLevelPrebuildInfo.ResultDataMaxSizeInBytes, &g_topLevelAccelerationStructure, initialResourceState, L"TopLevelAccelerationStructure");
	}

	// Note on Emulated GPU pointers (AKA Wrapped pointers) requirement in Fallback Layer:
	// The primary point of divergence between the DXR API and the compute-based Fallback layer is the handling of GPU pointers. 
	// DXR fundamentally requires that GPUs be able to dynamically read from arbitrary addresses in GPU memory. 
	// The existing Direct Compute API today is more rigid than DXR and requires apps to explicitly inform the GPU what blocks of memory it will access with SRVs/UAVs.
	// In order to handle the requirements of DXR, the Fallback Layer uses the concept of Emulated GPU pointers, 
	// which requires apps to create views around all memory they will access for raytracing, 
	// but retains the DXR-like flexibility of only needing to bind the top level acceleration structure at DispatchRays.
	//
	// The Fallback Layer interface uses WRAPPED_GPU_POINTER to encapsulate the underlying pointer
	// which will either be an emulated GPU pointer for the compute - based path or a GPU_VIRTUAL_ADDRESS for the DXR path.

	// Create an instance desc for the bottom-level acceleration structure.
	ComPtr<ID3D12Resource> instanceDescs;
	if (g_raytracingAPI == RaytracingAPI::FallbackLayer)
	{
		D3D12_RAYTRACING_FALLBACK_INSTANCE_DESC instanceDesc = {};
		instanceDesc.Transform[0][0] = instanceDesc.Transform[1][1] = instanceDesc.Transform[2][2] = 1;
		instanceDesc.InstanceMask = 1;
		UINT numBufferElements = static_cast<UINT>(bottomLevelPrebuildInfo.ResultDataMaxSizeInBytes) / sizeof(UINT32);
		instanceDesc.AccelerationStructure = CreateFallbackWrappedPointer(g_bottomLevelAccelerationStructure.Get(), numBufferElements);
		AllocateUploadBuffer(device.Get(), &instanceDesc, sizeof(instanceDesc), &instanceDescs, L"InstanceDescs");
	}
	else // DirectX Raytracing
	{
		D3D12_RAYTRACING_INSTANCE_DESC instanceDesc = {};
		instanceDesc.Transform[0][0] = instanceDesc.Transform[1][1] = instanceDesc.Transform[2][2] = 1;
		instanceDesc.InstanceMask = 1;
		instanceDesc.AccelerationStructure = g_bottomLevelAccelerationStructure->GetGPUVirtualAddress();
		AllocateUploadBuffer(device.Get(), &instanceDesc, sizeof(instanceDesc), &instanceDescs, L"InstanceDescs");
	}

	// Create a wrapped pointer to the acceleration structure.
	if (g_raytracingAPI == RaytracingAPI::FallbackLayer)
	{
		UINT numBufferElements = static_cast<UINT>(topLevelPrebuildInfo.ResultDataMaxSizeInBytes) / sizeof(UINT32);
		g_fallbackTopLevelAccelerationStructrePointer = CreateFallbackWrappedPointer(g_topLevelAccelerationStructure.Get(), numBufferElements);
	}

	// Bottom Level Acceleration Structure desc
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC bottomLevelBuildDesc = {};
	{
		bottomLevelBuildDesc.Inputs = bottomLevelInputs;
		bottomLevelBuildDesc.ScratchAccelerationStructureData = scratchResource->GetGPUVirtualAddress();
		bottomLevelBuildDesc.DestAccelerationStructureData = g_bottomLevelAccelerationStructure->GetGPUVirtualAddress();
	}

	// Top Level Acceleration Structure desc
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC topLevelBuildDesc = {};
	{
		topLevelInputs.InstanceDescs = instanceDescs->GetGPUVirtualAddress();
		topLevelBuildDesc.Inputs = topLevelInputs;
		topLevelBuildDesc.DestAccelerationStructureData = g_topLevelAccelerationStructure->GetGPUVirtualAddress();
		topLevelBuildDesc.ScratchAccelerationStructureData = scratchResource->GetGPUVirtualAddress();
	}

	auto BuildAccelerationStructure = [&](auto* raytracingCommandList)
	{
		raytracingCommandList->BuildRaytracingAccelerationStructure(&bottomLevelBuildDesc, 0, nullptr);
		commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::UAV(g_bottomLevelAccelerationStructure.Get()));
		raytracingCommandList->BuildRaytracingAccelerationStructure(&topLevelBuildDesc, 0, nullptr);
	};

	// Build acceleration structure.
	if (g_raytracingAPI == RaytracingAPI::FallbackLayer)
	{
		// Set the descriptor heaps to be used during acceleration structure build for the Fallback Layer.
		ID3D12DescriptorHeap *pDescriptorHeaps[] = { g_CBVHeap.Get() };
		g_fallbackCommandList->SetDescriptorHeaps(ARRAYSIZE(pDescriptorHeaps), pDescriptorHeaps);
		BuildAccelerationStructure(g_fallbackCommandList.Get());
	}
	else // DirectX Raytracing
	{
		BuildAccelerationStructure(g_dxrCommandList.Get());
	}

	UINT currentBackBufferIndex = g_pWindow->GetCurrentBackBufferIndex();


	g_FenceValues[currentBackBufferIndex] = commandQueue->ExecuteCommandList(commandList);

	//currentBackBufferIndex = g_pWindow->Present();

	commandQueue->WaitForFenceValue(g_FenceValues[currentBackBufferIndex]);
	commandQueue->Flush();
}

// Copy the raytracing output to the backbuffer.
void DXRSample::CopyRaytracingOutputToBackbuffer(ID3D12GraphicsCommandList* commandList)
{
	auto renderTarget = g_pWindow->GetCurrentBackBuffer();

	D3D12_RESOURCE_BARRIER preCopyBarriers[2];
	preCopyBarriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(renderTarget.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_DEST);
	preCopyBarriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(g_raytracingOutput.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
	commandList->ResourceBarrier(ARRAYSIZE(preCopyBarriers), preCopyBarriers);

	commandList->CopyResource(renderTarget.Get(), g_raytracingOutput.Get());

	D3D12_RESOURCE_BARRIER postCopyBarriers[2];
	postCopyBarriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(renderTarget.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_RENDER_TARGET);
	postCopyBarriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(g_raytracingOutput.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	commandList->ResourceBarrier(ARRAYSIZE(postCopyBarriers), postCopyBarriers);
}


// Create a wrapped pointer for the Fallback Layer path.
WRAPPED_GPU_POINTER DXRSample::CreateFallbackWrappedPointer(ID3D12Resource* resource, UINT bufferNumElements)
{
	auto device = Application::Get().GetDevice();

	D3D12_UNORDERED_ACCESS_VIEW_DESC rawBufferUavDesc = {};
	rawBufferUavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
	rawBufferUavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;
	rawBufferUavDesc.Format = DXGI_FORMAT_R32_TYPELESS;
	rawBufferUavDesc.Buffer.NumElements = bufferNumElements;

	D3D12_CPU_DESCRIPTOR_HANDLE bottomLevelDescriptor;

	// Only compute fallback requires a valid descriptor index when creating a wrapped pointer.
	UINT descriptorHeapIndex = 0;
	if (!g_fallbackDevice->UsingRaytracingDriver())
	{
		descriptorHeapIndex = AllocateDescriptor(&bottomLevelDescriptor);
		device->CreateUnorderedAccessView(resource, nullptr, &rawBufferUavDesc, bottomLevelDescriptor);
	}
	return g_fallbackDevice->GetWrappedPointerSimple(descriptorHeapIndex, resource->GetGPUVirtualAddress());
}

// Allocate a descriptor and return its index. 
// If the passed descriptorIndexToUse is valid, it will be used instead of allocating a new one.
UINT DXRSample::AllocateDescriptor(D3D12_CPU_DESCRIPTOR_HANDLE* cpuDescriptor, UINT descriptorIndexToUse)
{
	auto descriptorHeapCpuBase = g_CBVHeap->GetCPUDescriptorHandleForHeapStart();
	if (descriptorIndexToUse >= g_CBVHeap->GetDesc().NumDescriptors)
	{
		descriptorIndexToUse = g_RaytracingCBVOffset + g_RaytracingDescriptorsAllocated++;
		//g_RaytracingDescriptorsAllocated++;
	}
	*cpuDescriptor = CD3DX12_CPU_DESCRIPTOR_HANDLE(descriptorHeapCpuBase, descriptorIndexToUse, Application::Get().GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
	return descriptorIndexToUse;
}


void DXRSample::DoRaytracing(ID3D12GraphicsCommandList* commandList)
{


	auto DispatchRays = [&](auto* commandList, auto* stateObject, auto* dispatchDesc)
	{
		// Since each shader table has only one shader record, the stride is same as the size.
		dispatchDesc->HitGroupTable.StartAddress = m_hitGroupShaderTable->GetGPUVirtualAddress();
		dispatchDesc->HitGroupTable.SizeInBytes = m_hitGroupShaderTable->GetDesc().Width;
		dispatchDesc->HitGroupTable.StrideInBytes = dispatchDesc->HitGroupTable.SizeInBytes;
		dispatchDesc->MissShaderTable.StartAddress = m_missShaderTable->GetGPUVirtualAddress();
		dispatchDesc->MissShaderTable.SizeInBytes = m_missShaderTable->GetDesc().Width;
		dispatchDesc->MissShaderTable.StrideInBytes = dispatchDesc->MissShaderTable.SizeInBytes;
		dispatchDesc->RayGenerationShaderRecord.StartAddress = m_rayGenShaderTable->GetGPUVirtualAddress();
		dispatchDesc->RayGenerationShaderRecord.SizeInBytes = m_rayGenShaderTable->GetDesc().Width;
		dispatchDesc->Width = super::GetWidth();
		dispatchDesc->Height = super::GetHeight();
		dispatchDesc->Depth = 1;
		commandList->SetPipelineState1(stateObject);
		commandList->DispatchRays(dispatchDesc);
	};

	commandList->SetComputeRootSignature(g_raytracingGlobalRootSignature.Get());

	// Bind the heaps, acceleration structure and dispatch rays.    
	D3D12_DISPATCH_RAYS_DESC dispatchDesc = {};
	if (g_raytracingAPI == RaytracingAPI::FallbackLayer)
	{
		g_fallbackCommandList->SetDescriptorHeaps(1, g_CBVHeap.GetAddressOf());
		commandList->SetComputeRootDescriptorTable(GlobalRootSignatureParams::OutputViewSlot, g_raytracingOutputResourceUAVGpuDescriptor);
		g_fallbackCommandList->SetTopLevelAccelerationStructure(GlobalRootSignatureParams::AccelerationStructureSlot, g_fallbackTopLevelAccelerationStructrePointer);
		DispatchRays(g_fallbackCommandList.Get(), g_fallbackStateObject.Get(), &dispatchDesc);
	}
	else // DirectX Raytracing
	{
		D3D12_DISPATCH_RAYS_DESC dispatchDesc = {};
		commandList->SetDescriptorHeaps(1, g_CBVHeap.GetAddressOf());
		commandList->SetComputeRootDescriptorTable(GlobalRootSignatureParams::OutputViewSlot, g_raytracingOutputResourceUAVGpuDescriptor);
		commandList->SetComputeRootShaderResourceView(GlobalRootSignatureParams::AccelerationStructureSlot, g_topLevelAccelerationStructure->GetGPUVirtualAddress());
		DispatchRays(g_dxrCommandList.Get(), g_dxrStateObject.Get(), &dispatchDesc);
	}
}

void DXRSample::UpdateForSizeChange(UINT width, UINT height)
{
	float border = 0.01f;
	auto aspectRatio = super::GetAspectRatio();
	if (width <= height)
	{
		g_rayGenCB.stencil =
		{
			-1 + border, -1 + border * aspectRatio,
			1.0f - border, 1 - border * aspectRatio
		};
	}
	else
	{
		g_rayGenCB.stencil =
		{
			-1 + border / aspectRatio, -1 + border,
			 1 - border / aspectRatio, 1.0f - border
		};

	}
}

void DXRSample::UpdateObjectCBs()
{
	auto currObjectCB = g_CurrFrameResource->ObjectCB.get();
	for (auto& e : g_AllRenderItems)
	{
		// Only update the cbuffer data if the constants have changed.  
		// This needs to be tracked per frame resource.
		if (e->NumFramesDirty > 0)
		{
			XMMATRIX world = XMLoadFloat4x4(&e->WorldMatrix);

			ObjectConstants objConstants;
			XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(world));

			currObjectCB->CopyData(e->ObjCBIndex, objConstants);

			// Next FrameResource need to be updated too.
			e->NumFramesDirty--;
		}
	}
}


void DXRSample::UpdateMainPassCB(UpdateEventArgs& e)
{
	XMMATRIX view = g_ViewMatrix;
	XMMATRIX proj = g_ProjectionMatrix;

	XMMATRIX viewProj = XMMatrixMultiply(view, proj);
	XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(view), view);
	XMMATRIX invProj = XMMatrixInverse(&XMMatrixDeterminant(proj), proj);
	XMMATRIX invViewProj = XMMatrixInverse(&XMMatrixDeterminant(viewProj), viewProj);

	XMStoreFloat4x4(&g_MainPassCB.View, XMMatrixTranspose(view));
	XMStoreFloat4x4(&g_MainPassCB.InvView, XMMatrixTranspose(invView));
	XMStoreFloat4x4(&g_MainPassCB.Proj, XMMatrixTranspose(proj));
	XMStoreFloat4x4(&g_MainPassCB.InvProj, XMMatrixTranspose(invProj));
	XMStoreFloat4x4(&g_MainPassCB.ViewProj, XMMatrixTranspose(viewProj));
	XMStoreFloat4x4(&g_MainPassCB.InvViewProj, XMMatrixTranspose(invViewProj));
	g_MainPassCB.EyePosW = XMFLOAT3(0, 0, 0);
	g_MainPassCB.RenderTargetSize = XMFLOAT2((float)super::GetWidth(), (float)super::GetHeight());
	g_MainPassCB.InvRenderTargetSize = XMFLOAT2(1.0f / super::GetWidth(), 1.0f / super::GetHeight());
	g_MainPassCB.NearZ = 1.0f;
	g_MainPassCB.FarZ = 1000.0f;
	g_MainPassCB.TotalTime = e.TotalTime;
	g_MainPassCB.DeltaTime = e.ElapsedTime;

	auto currPassCB = g_CurrFrameResource->PassCB.get();
	currPassCB->CopyData(0, g_MainPassCB);
}

void DXRSample::OnUpdate(UpdateEventArgs& e)
{

	auto commandQueue = Application::Get().GetCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT);

	super::OnUpdate(e);

	// Update the view matrix.
	// Convert Spherical to Cartesian coordinates.
	mEyePos.x = mRadius * sinf(mPhi)*cosf(mTheta);
	mEyePos.z = mRadius * sinf(mPhi)*sinf(mTheta);
	mEyePos.y = mRadius * cosf(mPhi);

	// Build the view matrix.
	XMVECTOR pos = XMVectorSet(mEyePos.x, mEyePos.y, mEyePos.z, 1.0f);
	XMVECTOR target = XMVectorZero();
	XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	XMMATRIX view = XMMatrixLookAtLH(pos, target, up);
	g_ViewMatrix = view;
	// Update the projection matrix.
	float aspectRatio = super::g_pWindow->GetClientWidth() / static_cast<float>(super::g_pWindow->GetClientHeight());
	g_ProjectionMatrix = XMMatrixPerspectiveFovLH(XMConvertToRadians(g_FoV), aspectRatio, 0.001f, 1000.0f);

	// Cycle through the circular frame resource array.
	g_CurrFrameResourceIndex = (g_CurrFrameResourceIndex + 1) % NUMBER_OF_FRAME_RESOURCES;
	//g_CurrFrameResourceIndex = 0;
	g_CurrFrameResource = g_FrameResources[g_CurrFrameResourceIndex].get();


	UpdateObjectCBs();
	UpdateMainPassCB(e);

	
	
}

// Transition a resource
void DXRSample::TransitionResource(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> commandList,
	Microsoft::WRL::ComPtr<ID3D12Resource> resource,
	D3D12_RESOURCE_STATES beforeState, D3D12_RESOURCE_STATES afterState)
{
	CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
		resource.Get(),
		beforeState, afterState);

	commandList->ResourceBarrier(1, &barrier);
}

// Clear a render target.
void DXRSample::ClearRTV(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> commandList,
	D3D12_CPU_DESCRIPTOR_HANDLE rtv, FLOAT* clearColor)
{
	commandList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
}

void DXRSample::ClearDepth(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> commandList,
	D3D12_CPU_DESCRIPTOR_HANDLE dsv, FLOAT depth)
{
	commandList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, depth, 0, 0, nullptr);
}

void DXRSample::OnRender(RenderEventArgs& e)
{
	super::OnRender(e);

	

	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	ImGui::Begin("Hello, world!", nullptr, ImGuiWindowFlags_MenuBar);                // Create a window called "Hello, world!" and append into it.
	if (ImGui::BeginMenuBar())
	{
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("Open..", "Ctrl+O")) { /* Do stuff */ }
			ImGui::EndMenu();
		}
		ImGui::EndMenuBar();
	}
	if (ImGui::Button("WireFrameMode", ImVec2(150, 50)))
	{
		isWireFrameMode = !isWireFrameMode;
	}
	if (g_raster)
		ImGui::Text("Raster");
	else
		ImGui::Text("Raytracing");
	ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

	ImGui::End();
	ImGui::EndFrame();


	auto commandQueue = Application::Get().GetCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT);
	auto commandList = commandQueue->GetCommandList();

	UINT currentBackBufferIndex = g_pWindow->GetCurrentBackBufferIndex();
	auto backBuffer = g_pWindow->GetCurrentBackBuffer();
	auto rtv = g_pWindow->GetCurrentRenderTargetView();
	auto dsv = g_DSVHeap->GetCPUDescriptorHandleForHeapStart();



	if (g_raster) {
		commandList->RSSetViewports(1, &g_Viewport);
		commandList->RSSetScissorRects(1, &g_ScissorRect);

		if (isWireFrameMode)
			commandList->SetPipelineState(g_PSOs["opaque_wireframe"].Get());
		else
			commandList->SetPipelineState(g_PSOs["opaque"].Get());
		// Clear the render targets.
		{
			TransitionResource(commandList, backBuffer,
				D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

			FLOAT clearColor[] = { 0.4f, 0.6f, 0.9f, 1.0f };

			ClearRTV(commandList, rtv, clearColor);
			ClearDepth(commandList, dsv);
		}
		commandList->OMSetRenderTargets(1, &rtv, FALSE, &dsv);

		ID3D12DescriptorHeap* descriptors[] = { g_CBVHeap.Get() };
		commandList->SetDescriptorHeaps(_countof(descriptors), descriptors);

		commandList->SetGraphicsRootSignature(g_RootSignature.Get());


		int passCbvIndex = g_PassCBVOffset + g_CurrFrameResourceIndex;
		auto passCbvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(g_CBVHeap->GetGPUDescriptorHandleForHeapStart());
		passCbvHandle.Offset(passCbvIndex, Application::Get().GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
		commandList->SetGraphicsRootDescriptorTable(1, passCbvHandle);


		DrawRenderItems(commandList.Get(), g_AllOpaqueItems);

		ImGui::Render();
		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList.Get());


		TransitionResource(commandList, backBuffer,
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
	}
	else {

		ID3D12DescriptorHeap* descriptors[] = { g_CBVHeap.Get() };
		commandList->SetDescriptorHeaps(_countof(descriptors), descriptors);


		TransitionResource(commandList, backBuffer,
			D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

		FLOAT clearColor[] = { 0.9f, 0.6f, 0.2f, 1.0f };


		commandList->OMSetRenderTargets(1, &rtv, FALSE, &dsv);

		commandList->SetGraphicsRootSignature(g_RootSignature.Get());


		DoRaytracing(commandList.Get());
		CopyRaytracingOutputToBackbuffer(commandList.Get());


		ImGui::Render();
		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList.Get());

		TransitionResource(commandList, backBuffer,
			D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
	}


	// Present
	{

		g_FenceValues[currentBackBufferIndex] = commandQueue->ExecuteCommandList(commandList);

		currentBackBufferIndex = g_pWindow->Present();

		commandQueue->WaitForFenceValue(g_FenceValues[currentBackBufferIndex]);
		//commandQueue->Flush();
	}
}

void DXRSample::OnKeyPressed(KeyEventArgs& e)
{
	super::OnKeyPressed(e);

	switch (e.Key)
	{
	case KeyCode::Escape:
		Application::Get().Quit(0);
		break;
	case KeyCode::Enter:
		if (e.Alt)
		{
	case KeyCode::F11:
		g_pWindow->ToggleFullScreen();
		break;
		}
	case KeyCode::V:
		g_pWindow->ToggleVSync();
		break;
	case KeyCode::A:
		g_raster = !g_raster;
		break;
	}
}
void DXRSample::OnMouseMoved(MouseMotionEventArgs & e)
{
	if (e.LeftButton && !ImGui::IsAnyWindowFocused())
	{
		OutputDebugString(L"Left Mouse Button Clicked");
		// Make each pixel correspond to a quarter of a degree.
		float dx = XMConvertToRadians(0.25f*static_cast<float>(e.X - mLastMousePos.x));
		float dy = XMConvertToRadians(0.25f*static_cast<float>(e.Y - mLastMousePos.y));

		// Update angles based on input to orbit camera around box.
		mTheta -= dx;
		mPhi -= dy;

		// Restrict the angle mPhi.
		mPhi = MathHelper::Clamp(mPhi, 0.1f, DirectX::XM_PI - 0.1f);
	}
	else if (e.RightButton && !ImGui::IsAnyWindowFocused())
	{
		// Make each pixel correspond to 0.2 unit in the scene.
		float dx = 0.05f*static_cast<float>(e.X - mLastMousePos.x);
		float dy = 0.05f*static_cast<float>(e.Y - mLastMousePos.y);

		// Update the camera radius based on input.
		mRadius -= dx - dy;

		// Restrict the radius.
		mRadius = MathHelper::Clamp(mRadius, 5.0f, 150.0f);
	}

	mLastMousePos.x = e.X;
	mLastMousePos.y = e.Y;

}
void DXRSample::OnMouseWheel(MouseWheelEventArgs& e)
{
	g_FoV -= e.WheelDelta;
	g_FoV = clamp<float>(g_FoV, 12.0f, 90.0f);

	char buffer[256];
	sprintf_s(buffer, "FoV: %f\n", g_FoV);
	OutputDebugStringA(buffer);
}
