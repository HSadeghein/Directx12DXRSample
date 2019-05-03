#define NOMINMAX
#include "DXRSample.h"
#include"Application.h"
#include"Window.h"
#include"CommandQueue.h"

#include <wrl.h>
#include "imgui/imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx12.h"

#include "d3dx12.h"0
#include <algorithm>
#include<array>


#include "d3d12_1.h"
#include "CompiledShaders/Raytracing.hlsl.h"
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





DXRSample::DXRSample(const std::wstring & name, int width, int height, bool vSync) :
	super(name, width, height, vSync),
	g_Viewport(CD3DX12_VIEWPORT(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height))),
	g_ScissorRect(CD3DX12_RECT(0, 0, LONG_MAX, LONG_MAX)),
	g_FoV(45.0),
	g_ContentLoaded(false)
{
	g_raytracingAPI == RaytracingAPI::FallbackLayer;
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
	// PSO for opaque wire-frame objects.
	//
	D3D12_GRAPHICS_PIPELINE_STATE_DESC opaqueWireFramePsoDesc;
	opaqueWireFramePsoDesc = opaquePsoDesc;
	opaqueWireFramePsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
	ThrowifFailed(device->CreateGraphicsPipelineState(&opaqueWireFramePsoDesc, IID_PPV_ARGS(&g_PSOs["opaque_wireframe"])));
}

void DXRSample::BuildMaterials()
{
	auto triangleMat = std::make_unique<Material>();
	triangleMat->Name = "triangle";
	triangleMat->MatCBIndex = 0;
	triangleMat->DiffuseAlbedo = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);

	g_Materials[triangleMat->Name] = std::move(triangleMat);
}

void DXRSample::DrawRenderItems(ID3D12GraphicsCommandList * cmdList, const std::vector<RenderItem*>& ritems)
{

	UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));
	UINT matCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(MaterialConstants));

	auto objectCB = g_CurrFrameResource->ObjectCB->Resource();
	auto matCB = g_CurrFrameResource->MaterialCB->Resource();
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

		D3D12_GPU_VIRTUAL_ADDRESS matCBAddress = matCB->GetGPUVirtualAddress() + ri->Mat->MatCBIndex * matCBByteSize;


		cmdList->SetGraphicsRootDescriptorTable(RasterRootSignatureParam::ObjectCB, cbvHandle);
		cmdList->SetGraphicsRootConstantBufferView(RasterRootSignatureParam::MaterialCB, matCBAddress);

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

	//Build SRV for Index and Vertex buffers 
	//sizeof(uint16_t) * 2 because the buffer format which is used in srv is R32
	UINT descriptorIndexIB = CreateBufferSRV(&m_indexBuffer, static_cast<UINT>( m_indexBuffer.resource->GetDesc().Width / (sizeof(uint32_t))), 0);
	UINT descriptorIndexVB = CreateBufferSRV(&m_vertexBuffer, static_cast<UINT>(m_vertexBuffer.resource->GetDesc().Width / (sizeof(VertexNormal))), sizeof(VertexNormal));
	ThrowIfFalse(descriptorIndexVB == descriptorIndexIB + 1, L"Vertex Buffer descriptor index must follow that of Index Buffer descriptor index!");


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


	CD3DX12_ROOT_PARAMETER rootParameters[RasterRootSignatureParam::Count];
	rootParameters[RasterRootSignatureParam::ObjectCB].InitAsDescriptorTable(1, &cbvTable0);
	rootParameters[RasterRootSignatureParam::PassCB].InitAsDescriptorTable(1, &cbvTable1);
	rootParameters[RasterRootSignatureParam::MaterialCB].InitAsConstantBufferView(2);
	

	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(RasterRootSignatureParam::Count, rootParameters, 0, nullptr, rootSignatureFlags);
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
	else // DirectX Ray-tracing
	{
		ThrowIfFailed(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error), error ? static_cast<wchar_t*>(error->GetBufferPointer()) : nullptr);
		ThrowIfFailed(device->CreateRootSignature(1, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&(*rootSig))));
	}
}

void DXRSample::CreateRaytracingRootSignatures()
{
	// Global Root Signature
	// This is a root signature that is shared across all ray-tracing shaders invoked during a DispatchRays() call.
	{
		CD3DX12_DESCRIPTOR_RANGE ranges[3];
		ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);
		ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 1);
		ranges[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 1);
		CD3DX12_ROOT_PARAMETER rootParameters[RaytraceGlobalRootSignatureParams::Count];
		rootParameters[RaytraceGlobalRootSignatureParams::OutputViewSlot].InitAsDescriptorTable(1, &ranges[0]);
		rootParameters[RaytraceGlobalRootSignatureParams::AccelerationStructureSlot].InitAsShaderResourceView(0);
		rootParameters[RaytraceGlobalRootSignatureParams::ScenceConstantBufferSlot].InitAsDescriptorTable(1,&ranges[1]);
		rootParameters[RaytraceGlobalRootSignatureParams::VertexBufferSlot].InitAsDescriptorTable(1, &ranges[2]);

		CD3DX12_ROOT_SIGNATURE_DESC globalRootSignatureDesc(ARRAYSIZE(rootParameters), rootParameters);
		SerializeAndCreateRaytracingRootSignature(globalRootSignatureDesc, &g_raytracingGlobalRootSignature);
	}

	// Local Root Signature
	// This is a root signature that enables a shader to have unique arguments that come from shader tables.
	{
		CD3DX12_ROOT_PARAMETER rootParameters[RaytraceLocalRootSignatureParams::Count];
		rootParameters[RaytraceLocalRootSignatureParams::ViewportConstantSlot].InitAsConstants(SizeOfInUint32(MaterialConstants), 0, 0);
		CD3DX12_ROOT_SIGNATURE_DESC localRootSignatureDesc(ARRAYSIZE(rootParameters), rootParameters);
		localRootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;
		SerializeAndCreateRaytracingRootSignature(localRootSignatureDesc, &g_raytracingLocalRootSignature);
	}
}

void DXRSample::CreateDescriptorHeaps(ID3D12Device * device)
{
	UINT objCount = (UINT)g_AllRenderItems.size();

	// Need a CBV descriptor for each object for each frame resource,
	// +1 for the perPass CBV for each frame resource. +1 for IMGUI. +5 for Raytracing
	UINT numDescriptors = (objCount + 1) * NUMBER_OF_FRAME_RESOURCES + 1 + 1 + 1 + 3 + 2;

	// Save an offset to the start of the pass CBVs.  These are the last 3 descriptors.
	// Allocate a heap for 3 descriptors:
	// 4 - bottom and top level acceleration structure fallback wrapped pointers
	// 3 for bottom level for instancing and 1 for top level
	// 1 - ray-tracing output texture SRV
	// The first one is for imgui
	// The 2,3,4,5th is for DXR
	g_RaytracingCBVOffset = 1;
	g_ObjectCBVOffset = g_RaytracingCBVOffset + 1 + 1 + 3 + 2;
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

	EnableDXR(adapter1.Get());





	BuildRootSignature();
	BuildShadersAndInputLayout();
	BuildMaterials();
	//BuildShapeGeometry(commandList.Get());
	//BuildRenderItems();
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
	// Initialize ray-tracing pipeline.

	// Create ray-tracing interfaces: ray-tracing device and command list.
	CreateRaytracingDeviceCommandlist();

	// Create root signatures for the shaders.
	CreateRaytracingRootSignatures();

	// Create a ray-tracing pipeline state object which defines the binding of shaders, state and resources to be used during ray-tracing.
	CreateRaytracingPipelineStateObject();



	// Build ray-tracing acceleration structures from the generated geometry.
	BuildAccelerationStructures();

	// Build shader tables, which define shaders and their local root arguments.
	BuildShaderTables();

	// Create an output 2D texture to store the ray-tracing result to.
	CreateRaytracingOutputResource();
}


//void DXRSample::BuildShapeGeometry(ID3D12GraphicsCommandList* commandList)
//{
//	auto device = Application::Get().GetDevice();
//
//	GeometryGenerator geoGen;
//	GeometryGenerator::MeshData box = geoGen.CreateBox(1.5f, 2.0f, 1.5f, 3);
//	GeometryGenerator::MeshData grid = geoGen.CreateGrid(20.0f, 30.0f, 60, 40);
//	GeometryGenerator::MeshData sphere = geoGen.CreateSphere(0.5f, 20, 20);
//	GeometryGenerator::MeshData cylinder = geoGen.CreateCylinder(0.5f, 0.3f, 3.0f, 20, 20);
//
//	//
//	// We are concatenating all the geometry into one big vertex/index buffer.  So
//	// define the regions in the buffer each submesh covers.
//	//
//
//	// Cache the vertex offsets to each object in the concatenated vertex buffer.
//	UINT boxVertexOffset = 0;
//	UINT gridVertexOffset = (UINT)box.Vertices.size();
//	UINT sphereVertexOffset = gridVertexOffset + (UINT)grid.Vertices.size();
//	UINT cylinderVertexOffset = sphereVertexOffset + (UINT)sphere.Vertices.size();
//
//	// Cache the starting index for each object in the concatenated index buffer.
//	UINT boxIndexOffset = 0;
//	UINT gridIndexOffset = (UINT)box.Indices32.size();
//	UINT sphereIndexOffset = gridIndexOffset + (UINT)grid.Indices32.size();
//	UINT cylinderIndexOffset = sphereIndexOffset + (UINT)sphere.Indices32.size();
//
//	// Define the SubmeshGeometry that cover different 
//	// regions of the vertex/index buffers.
//
//	d3dUtil::SubmeshGeometry boxSubmesh;
//	boxSubmesh.IndexCount = (UINT)box.Indices32.size();
//	boxSubmesh.StartIndexLocation = boxIndexOffset;
//	boxSubmesh.StartVertexLocation = boxVertexOffset;
//	boxSubmesh.VertexCount = (UINT)box.Vertices.size();
//
//	/*d3dUtil::SubmeshGeometry gridSubmesh;
//	gridSubmesh.IndexCount = (UINT)grid.Indices32.size();
//	gridSubmesh.StartIndexLocation = gridIndexOffset;
//	gridSubmesh.StartVertexLocation = gridVertexOffset;
//
//	d3dUtil::SubmeshGeometry sphereSubmesh;
//	sphereSubmesh.IndexCount = (UINT)sphere.Indices32.size();
//	sphereSubmesh.StartIndexLocation = sphereIndexOffset;
//	sphereSubmesh.StartVertexLocation = sphereVertexOffset;
//
//	d3dUtil::SubmeshGeometry cylinderSubmesh;
//	cylinderSubmesh.IndexCount = (UINT)cylinder.Indices32.size();
//	cylinderSubmesh.StartIndexLocation = cylinderIndexOffset;
//	cylinderSubmesh.StartVertexLocation = cylinderVertexOffset;*/
//
//	//
//	// Extract the vertex elements we are interested in and pack the
//	// vertices of all the meshes into one vertex buffer.
//	//
//
//	auto totalVertexCount =
//		box.Vertices.size();/* +
//		grid.Vertices.size() +
//		sphere.Vertices.size() +
//		cylinder.Vertices.size();*/
//
//	std::vector<VertexPosColor> vertices(totalVertexCount);
//
//	UINT k = 0;
//	for (size_t i = 0; i < box.Vertices.size(); ++i, ++k)
//	{
//		vertices[k].position = box.Vertices[i].Position;
//		vertices[k].color = XMFLOAT3(1.0, 1.0, 0.0);
//	}
///*
//	for (size_t i = 0; i < grid.Vertices.size(); ++i, ++k)
//	{
//		vertices[k].position = grid.Vertices[i].Position;
//		vertices[k].color = XMFLOAT3(1.0, 1.0, 0.0);
//	}
//
//	for (size_t i = 0; i < sphere.Vertices.size(); ++i, ++k)
//	{
//		vertices[k].position = sphere.Vertices[i].Position;
//		vertices[k].color = XMFLOAT3(1.0, 1.0, 0.0);
//	}
//
//	for (size_t i = 0; i < cylinder.Vertices.size(); ++i, ++k)
//	{
//		vertices[k].position = cylinder.Vertices[i].Position;
//		vertices[k].color = XMFLOAT3(1.0, 1.0, 0.0);
//	}*/
//
//	std::vector<std::uint16_t> indices;
//	indices.insert(indices.end(), std::begin(box.GetIndices16()), std::end(box.GetIndices16()));
//	//indices.insert(indices.end(), std::begin(grid.GetIndices16()), std::end(grid.GetIndices16()));
//	//indices.insert(indices.end(), std::begin(sphere.GetIndices16()), std::end(sphere.GetIndices16()));
//	//indices.insert(indices.end(), std::begin(cylinder.GetIndices16()), std::end(cylinder.GetIndices16()));
//
//	const UINT vbByteSize = (UINT)vertices.size() * sizeof(VertexPosColor);
//	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);
//
//	auto geo = std::make_unique<d3dUtil::MeshGeometry>();
//	geo->Name = "shapeGeo";
//
//	ThrowifFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
//	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);
//
//	ThrowifFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
//	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);
//
//	geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(device.Get(),
//		commandList, vertices.data(), vbByteSize, geo->VertexBufferUploader);
//
//	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(device.Get(),
//		commandList, indices.data(), ibByteSize, geo->IndexBufferUploader);
//
//	geo->VertexByteStride = sizeof(VertexPosColor);
//	geo->VertexBufferByteSize = vbByteSize;
//	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
//	geo->IndexBufferByteSize = ibByteSize;
//
//	geo->DrawArgs["box"] = boxSubmesh;
//	/*geo->DrawArgs["grid"] = gridSubmesh;
//	geo->DrawArgs["sphere"] = sphereSubmesh;
//	geo->DrawArgs["cylinder"] = cylinderSubmesh;*/
//
//	g_Geometries[geo->Name] = std::move(geo);
//}

void DXRSample::BuildTriangle(ID3D12GraphicsCommandList* commandList)
{
	using Vertex = GeometryGenerator::Vertex;
	auto device = Application::Get().GetDevice();

	GeometryGenerator geoGen;
	GeometryGenerator::MeshData box = geoGen.CreateBox(1.5f, 2.0f, 1.5f, 0);
	GeometryGenerator::MeshData grid = geoGen.CreateGrid(20.0f, 20.0f, 16, 16);




	d3dUtil::SubmeshGeometry subMeshBox;
	subMeshBox.IndexCount = (UINT)box.GetIndices16().size();
	subMeshBox.StartIndexLocation = 0;
	subMeshBox.StartVertexLocation = 0;
	subMeshBox.VertexCount = (UINT)box.Vertices.size();


	d3dUtil::SubmeshGeometry subMeshPlane;
	subMeshPlane.IndexCount = (UINT)grid.Indices32.size();
	subMeshPlane.StartIndexLocation = subMeshBox.IndexCount;
	subMeshPlane.StartVertexLocation = subMeshBox.VertexCount;
	subMeshPlane.VertexCount = (UINT)grid.Vertices.size();



	std::vector<VertexNormal> totalVertices(box.Vertices.size() + grid.Vertices.size());
	std::vector<std::uint16_t> totalIndices;
	std::vector<std::uint32_t> totalIndices1;


	totalIndices.insert(totalIndices.end(), std::begin(box.GetIndices16()), std::end(box.GetIndices16()));
	totalIndices.insert(totalIndices.end(), std::begin(grid.GetIndices16()), std::end(grid.GetIndices16()));

	totalIndices1.insert(totalIndices1.end(), std::begin(box.Indices32), std::end(box.Indices32));
	totalIndices1.insert(totalIndices1.end(), std::begin(grid.Indices32), std::end(grid.Indices32));

	for (size_t i = subMeshPlane.StartIndexLocation; i < subMeshPlane.StartIndexLocation + subMeshPlane.IndexCount; i++)
	{
		//totalIndices1[i] += subMeshPlane.StartVertexLocation;
	}

	int k = 0;

	for (size_t i = 0; i < box.Vertices.size(); i++)
	{
		totalVertices[k].position = box.Vertices[i].Position;
		totalVertices[k].normal = box.Vertices[i].Normal;
		k++;
	}
	for (size_t i = 0; i < grid.Vertices.size(); i++)
	{
		totalVertices[k].position = grid.Vertices[i].Position;
		totalVertices[k].normal = grid.Vertices[i].Normal;
		k++;
	}


	const UINT vbByteSize = (UINT)totalVertices.size() * sizeof(VertexNormal);
	const UINT ibByteSize = (UINT)totalIndices1.size() * sizeof(std::uint16_t);

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

	AllocateUploadBuffer(device.Get(), totalIndices1.data(), ibByteSize * 2, m_indexBuffer.resource.GetAddressOf());
	AllocateUploadBuffer(device.Get(), totalVertices.data(), vbByteSize, m_vertexBuffer.resource.GetAddressOf());


	geo->VertexByteStride = sizeof(VertexNormal);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	geo->DrawArgs["triangle"] = subMeshBox;
	geo->DrawArgs["grid"] = subMeshPlane;
	g_Geometries[geo->Name] = std::move(geo);
}

void DXRSample::BuildRenderTriangleItem()
{
	auto triangleRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&triangleRitem->WorldMatrix, XMMatrixScaling(1.0f, 1.0f, 1.0f)*XMMatrixTranslation(0.0f, 0.0f, 0.0f));
	triangleRitem->ObjCBIndex = 0;
	triangleRitem->Mat = g_Materials["triangle"].get();
	triangleRitem->Geo = g_Geometries["triangleGeo"].get();
	triangleRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	triangleRitem->IndexCount = triangleRitem->Geo->DrawArgs["triangle"].IndexCount;
	triangleRitem->StartIndexLocation = triangleRitem->Geo->DrawArgs["triangle"].StartIndexLocation;
	triangleRitem->BaseVertexLocation = triangleRitem->Geo->DrawArgs["triangle"].StartVertexLocation;
	g_AllRenderItems.push_back(std::move(triangleRitem));

	auto triangleRitem1 = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&triangleRitem1->WorldMatrix, XMMatrixScaling(1.0f, 1.0f, 1.0f) * XMMatrixTranslation(100.0f, 0.0f, 0.0f));
	triangleRitem1->ObjCBIndex = 1;
	triangleRitem1->Mat = g_Materials["triangle"].get();
	triangleRitem1->Geo = g_Geometries["triangleGeo"].get();
	triangleRitem1->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	triangleRitem1->IndexCount = triangleRitem1->Geo->DrawArgs["triangle"].IndexCount;
	triangleRitem1->StartIndexLocation = triangleRitem1->Geo->DrawArgs["triangle"].StartIndexLocation;
	triangleRitem1->BaseVertexLocation = triangleRitem1->Geo->DrawArgs["triangle"].StartVertexLocation;
	g_AllRenderItems.push_back(std::move(triangleRitem1));

	auto planeRenderItem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&planeRenderItem->WorldMatrix, XMMatrixScaling(1.0f, 1.0f, 1.0f) * XMMatrixTranslation(-100.0f, 0.0f, 0.0f));
	planeRenderItem->ObjCBIndex = 2;
	planeRenderItem->Mat = g_Materials["triangle"].get();
	planeRenderItem->Geo = g_Geometries["triangleGeo"].get();
	planeRenderItem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	planeRenderItem->IndexCount = planeRenderItem->Geo->DrawArgs["grid"].IndexCount;
	planeRenderItem->StartIndexLocation = planeRenderItem->Geo->DrawArgs["grid"].StartIndexLocation;
	planeRenderItem->BaseVertexLocation = planeRenderItem->Geo->DrawArgs["grid"].StartVertexLocation;
	g_AllRenderItems.push_back(std::move(planeRenderItem));


	for (auto& e : g_AllRenderItems)
		g_AllOpaqueItems.push_back(e.get());
}

UINT DXRSample::CreateBufferSRV(D3DBuffer* buffer, UINT numElements, UINT elementSize)
{
	auto device = Application::Get().GetDevice();

	// SRV
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Buffer.NumElements = numElements;
	srvDesc.Buffer.FirstElement = 0;
	if (elementSize == 0)
	{
		srvDesc.Format = DXGI_FORMAT_R32_TYPELESS;
		srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
		srvDesc.Buffer.StructureByteStride = 0;
	}
	else
	{
		srvDesc.Format = DXGI_FORMAT_UNKNOWN;
		srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
		srvDesc.Buffer.StructureByteStride = elementSize;
	}
	UINT descriptorIndex = AllocateDescriptor(&buffer->cpuDescriptorHandle);
	device->CreateShaderResourceView(buffer->resource.Get(), &srvDesc, buffer->cpuDescriptorHandle);
	buffer->gpuDescriptorHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(g_CBVHeap->GetGPUDescriptorHandleForHeapStart(), descriptorIndex, Application::Get().GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
	return descriptorIndex;
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
	g_bottomLevelAccelerationStructure[0].Reset();
	g_bottomLevelAccelerationStructure[1].Reset();

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
	boxRitem->Mat = g_Materials["triangle"].get();
	boxRitem->Geo = g_Geometries["shapeGeo"].get();
	boxRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	boxRitem->IndexCount = boxRitem->Geo->DrawArgs["box"].IndexCount;
	boxRitem->StartIndexLocation = boxRitem->Geo->DrawArgs["box"].StartIndexLocation;
	boxRitem->BaseVertexLocation = boxRitem->Geo->DrawArgs["box"].StartVertexLocation;
	g_AllRenderItems.push_back(std::move(boxRitem));
	
	//auto gridRitem = std::make_unique<RenderItem>();
	//gridRitem->WorldMatrix = MathHelper::Identity4x4();
	//gridRitem->ObjCBIndex = 1;
	//gridRitem->Mat = g_Materials["triangle"].get();
	//gridRitem->Geo = g_Geometries["shapeGeo"].get();
	//gridRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	//gridRitem->IndexCount = gridRitem->Geo->DrawArgs["grid"].IndexCount;
	//gridRitem->StartIndexLocation = gridRitem->Geo->DrawArgs["grid"].StartIndexLocation;
	//gridRitem->BaseVertexLocation = gridRitem->Geo->DrawArgs["grid"].StartVertexLocation;
	//g_AllRenderItems.push_back(std::move(gridRitem));

	/*UINT objCBIndex = 2;
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
		leftCylRitem->Mat = g_Materials["triangle"].get();
		leftCylRitem->Geo = g_Geometries["shapeGeo"].get();
		leftCylRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		leftCylRitem->IndexCount = leftCylRitem->Geo->DrawArgs["cylinder"].IndexCount;
		leftCylRitem->StartIndexLocation = leftCylRitem->Geo->DrawArgs["cylinder"].StartIndexLocation;
		leftCylRitem->BaseVertexLocation = leftCylRitem->Geo->DrawArgs["cylinder"].StartVertexLocation;

		XMStoreFloat4x4(&rightCylRitem->WorldMatrix, leftCylWorld);
		rightCylRitem->ObjCBIndex = objCBIndex++;
		rightCylRitem->Mat = g_Materials["triangle"].get();
		rightCylRitem->Geo = g_Geometries["shapeGeo"].get();
		rightCylRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		rightCylRitem->IndexCount = rightCylRitem->Geo->DrawArgs["cylinder"].IndexCount;
		rightCylRitem->StartIndexLocation = rightCylRitem->Geo->DrawArgs["cylinder"].StartIndexLocation;
		rightCylRitem->BaseVertexLocation = rightCylRitem->Geo->DrawArgs["cylinder"].StartVertexLocation;

		XMStoreFloat4x4(&leftSphereRitem->WorldMatrix, leftSphereWorld);
		leftSphereRitem->ObjCBIndex = objCBIndex++;
		leftSphereRitem->Mat = g_Materials["triangle"].get();
		leftSphereRitem->Geo = g_Geometries["shapeGeo"].get();
		leftSphereRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		leftSphereRitem->IndexCount = leftSphereRitem->Geo->DrawArgs["sphere"].IndexCount;
		leftSphereRitem->StartIndexLocation = leftSphereRitem->Geo->DrawArgs["sphere"].StartIndexLocation;
		leftSphereRitem->BaseVertexLocation = leftSphereRitem->Geo->DrawArgs["sphere"].StartVertexLocation;

		XMStoreFloat4x4(&rightSphereRitem->WorldMatrix, rightSphereWorld);
		rightSphereRitem->ObjCBIndex = objCBIndex++;
		rightSphereRitem->Mat = g_Materials["triangle"].get();
		rightSphereRitem->Geo = g_Geometries["shapeGeo"].get();
		rightSphereRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		rightSphereRitem->IndexCount = rightSphereRitem->Geo->DrawArgs["sphere"].IndexCount;
		rightSphereRitem->StartIndexLocation = rightSphereRitem->Geo->DrawArgs["sphere"].StartIndexLocation;
		rightSphereRitem->BaseVertexLocation = rightSphereRitem->Geo->DrawArgs["sphere"].StartVertexLocation;

		g_AllRenderItems.push_back(std::move(leftCylRitem));
		g_AllRenderItems.push_back(std::move(rightCylRitem));
		g_AllRenderItems.push_back(std::move(leftSphereRitem));
		g_AllRenderItems.push_back(std::move(rightSphereRitem));
	}*/

	
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
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};


}

// Create 2D output texture for ray-tracing.
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
	else // DirectX Ray-tracing
	{
		ComPtr<ID3D12StateObjectPropertiesPrototype> stateObjectProperties;
		ThrowIfFailed(g_dxrStateObject.As(&stateObjectProperties));
		GetShaderIdentifiers(stateObjectProperties.Get());
		shaderIdentifierSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
	}

	// Ray gen shader table
	{
		
		UINT numShaderRecords = 1;
		UINT shaderRecordSize = shaderIdentifierSize;
		ShaderTable rayGenShaderTable(device.Get(), numShaderRecords, shaderRecordSize, L"RayGenShaderTable");
		rayGenShaderTable.push_back(ShaderRecord(rayGenShaderIdentifier, shaderIdentifierSize));
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
		struct RootArguments {
			MaterialConstants cb;
		} rootArguments1,rootArguments2, rootArguments3;
		MaterialConstants tmp;
		tmp.DiffuseAlbedo = g_Materials["triangle"].get()->DiffuseAlbedo;
		tmp.FresnelR0 = g_Materials["triangle"].get()->FresnelR0;
		tmp.Roughness = g_Materials["triangle"].get()->Roughness;
		rootArguments1.cb = tmp;
		rootArguments3.cb = tmp;
		rootArguments2.cb = tmp;

		UINT numShaderRecords = 3;
		UINT shaderRecordSize = shaderIdentifierSize + sizeof(rootArguments1);
		g_hitGroupShaderTableStrideInBytes = shaderRecordSize;
		ShaderTable hitGroupShaderTable(device.Get(), numShaderRecords, shaderRecordSize, L"HitGroupShaderTable");
		hitGroupShaderTable.push_back(ShaderRecord(hitGroupShaderIdentifier, shaderIdentifierSize, &rootArguments1, sizeof(rootArguments1)));
		hitGroupShaderTable.push_back(ShaderRecord(hitGroupShaderIdentifier, shaderIdentifierSize, &rootArguments1, sizeof(rootArguments1)));
		hitGroupShaderTable.push_back(ShaderRecord(hitGroupShaderIdentifier, shaderIdentifierSize, &rootArguments2, sizeof(rootArguments2)));
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
//Create ray-tracing command list and device

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
	// This is a root signature that is shared across all ray-tracing shaders invoked during a DispatchRays() call.
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
	else // DirectX Ray-tracing
	{
		ThrowifFailed(g_dxrDevice->CreateStateObject(raytracingPipeline, IID_PPV_ARGS(&g_dxrStateObject)));
	}
}

// Local root signature and shader association
// This is a root signature that enables a shader to have unique arguments that come from shader tables.
void DXRSample::CreateLocalRootSignatureSubobjects(CD3D12_STATE_OBJECT_DESC* raytracingPipeline)
{
	// Raygen and miss shaders in this sample are not using a local root signature and thus one is not associated with them.

	// Local root signature to be used in a ray gen shader.
	{
		auto localRootSignature = raytracingPipeline->CreateSubobject<CD3D12_LOCAL_ROOT_SIGNATURE_SUBOBJECT>();
		localRootSignature->SetRootSignature(g_raytracingLocalRootSignature.Get());
		// Shader association
		auto rootSignatureAssociation = raytracingPipeline->CreateSubobject<CD3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION_SUBOBJECT>();
		rootSignatureAssociation->SetSubobjectToAssociate(*localRootSignature);
		rootSignatureAssociation->AddExport(c_hitGroupName);
	}
}
AccelerationStructureBuffers DXRSample::BuildBottomLevelAccelerationStructure()
{
	auto device = Application::Get().GetDevice();
	auto geometry = g_Geometries.begin()->second.get();

	std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> geometryDesc;
	geometryDesc.resize(2);
	//for (size_t i = 0; i < numGeometries; i++)
	//{
	//	geometryDesc[i].Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
	//	geometryDesc[i].Triangles.IndexBuffer = geometry->IndexBufferGPU->GetGPUVirtualAddress();
	//	geometryDesc[i].Triangles.IndexCount = geometry->DrawArgs["triangle"].IndexCount;
	//	geometryDesc[i].Triangles.IndexFormat = geometry->IndexFormat;
	//	geometryDesc[i].Triangles.Transform3x4 = 0;
	//	geometryDesc[i].Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;

	//	geometryDesc[i].Triangles.VertexCount = geometry->DrawArgs["triangle"].VertexCount;
	//	geometryDesc[i].Triangles.VertexBuffer.StartAddress = geometry->VertexBufferGPU->GetGPUVirtualAddress();
	//	geometryDesc[i].Triangles.VertexBuffer.StrideInBytes = geometry->VertexByteStride;
	//	// Mark the geometry as opaque. 
	//	// PERFORMANCE TIP: mark geometry as opaque whenever applicable as it can enable important ray processing optimizations.
	//	// Note: When rays encounter opaque geometry an any hit shader will not be executed whether it is present or not.
	//	geometryDesc[i].Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;
	//}




	D3D12_RAYTRACING_GEOMETRY_DESC geoDesc;
	geometryDesc[0].Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
	geometryDesc[0].Triangles.IndexBuffer = geometry->IndexBufferGPU->GetGPUVirtualAddress() + geometry->DrawArgs["triangle"].StartIndexLocation * sizeof(UINT16);
	geometryDesc[0].Triangles.IndexCount = geometry->DrawArgs["triangle"].IndexCount;
	geometryDesc[0].Triangles.IndexFormat = geometry->IndexFormat;
	geometryDesc[0].Triangles.Transform3x4 = 0;
	geometryDesc[0].Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;

	geometryDesc[0].Triangles.VertexCount = geometry->DrawArgs["triangle"].VertexCount;
	geometryDesc[0].Triangles.VertexBuffer.StartAddress = geometry->VertexBufferGPU->GetGPUVirtualAddress() + geometry->DrawArgs["triangle"].StartVertexLocation * sizeof(VertexNormal);
	geometryDesc[0].Triangles.VertexBuffer.StrideInBytes = geometry->VertexByteStride;
	// Mark the geometry as opaque. 
	// PERFORMANCE TIP: mark geometry as opaque whenever applicable as it can enable important ray processing optimizations.
	// Note: When rays encounter opaque geometry an any hit shader will not be executed whether it is present or not.
	geometryDesc[0].Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;


	geometryDesc[1].Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
	geometryDesc[1].Triangles.IndexBuffer = geometry->IndexBufferGPU->GetGPUVirtualAddress() + geometry->DrawArgs["grid"].StartIndexLocation * sizeof(UINT16);
	geometryDesc[1].Triangles.IndexCount = geometry->DrawArgs["grid"].IndexCount;
	geometryDesc[1].Triangles.IndexFormat = geometry->IndexFormat;
	geometryDesc[1].Triangles.Transform3x4 = 0;
	geometryDesc[1].Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;

	geometryDesc[1].Triangles.VertexCount = geometry->DrawArgs["grid"].VertexCount;
	geometryDesc[1].Triangles.VertexBuffer.StartAddress = geometry->VertexBufferGPU->GetGPUVirtualAddress() + geometry->DrawArgs["grid"].StartVertexLocation * sizeof(VertexNormal);
	geometryDesc[1].Triangles.VertexBuffer.StrideInBytes = geometry->VertexByteStride;
	// Mark the geometry as opaque. 
	// PERFORMANCE TIP: mark geometry as opaque whenever applicable as it can enable important ray processing optimizations.
	// Note: When rays encounter opaque geometry an any hit shader will not be executed whether it is present or not.
	geometryDesc[1].Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;


	// Get required sizes for an acceleration structure.
	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC asDesc = {};

	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS& bottomLevelInputs = asDesc.Inputs;
	bottomLevelInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	bottomLevelInputs.Flags = buildFlags;
	bottomLevelInputs.NumDescs = 2;
	bottomLevelInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
	bottomLevelInputs.pGeometryDescs = geometryDesc.data();


	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO bottomLevelPrebuildInfo = {};
	if (g_raytracingAPI == RaytracingAPI::FallbackLayer)
	{
		g_fallbackDevice->GetRaytracingAccelerationStructurePrebuildInfo(&bottomLevelInputs, &bottomLevelPrebuildInfo);
	}
	else // DirectX Ray-tracing
	{
		g_dxrDevice->GetRaytracingAccelerationStructurePrebuildInfo(&bottomLevelInputs, &bottomLevelPrebuildInfo);
	}
	ThrowifFailed(bottomLevelPrebuildInfo.ResultDataMaxSizeInBytes > 0);


	Microsoft::WRL::ComPtr<ID3D12Resource> scratchResource;
	Microsoft::WRL::ComPtr<ID3D12Resource> bottomLevelAL;

	AllocateUAVBuffer(device.Get(), bottomLevelPrebuildInfo.ScratchDataSizeInBytes, scratchResource.GetAddressOf(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, L"ScratchResource");

	{
		D3D12_RESOURCE_STATES initialResourceState;
		if (g_raytracingAPI == RaytracingAPI::FallbackLayer)
		{
			initialResourceState = g_fallbackDevice->GetAccelerationStructureResourceState();
		}
		else // DirectX Ray-tracing
		{
			initialResourceState = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
		}
		AllocateUAVBuffer(device.Get(), bottomLevelPrebuildInfo.ResultDataMaxSizeInBytes, bottomLevelAL.GetAddressOf(), initialResourceState, L"BottomLevelAccelerationStructure");
	}


	{
		asDesc.DestAccelerationStructureData = bottomLevelAL->GetGPUVirtualAddress();
		asDesc.ScratchAccelerationStructureData = scratchResource->GetGPUVirtualAddress();
	}

	if (g_raytracingAPI == RaytracingAPI::FallbackLayer)
	{
		ID3D12DescriptorHeap* pDescriptorHeaps[] = { g_CBVHeap.Get() };
		g_fallbackCommandList->SetDescriptorHeaps(ARRAYSIZE(pDescriptorHeaps), pDescriptorHeaps);
		g_fallbackCommandList->BuildRaytracingAccelerationStructure(&asDesc, 0, nullptr);
	}
	else
	{
		g_dxrCommandList->BuildRaytracingAccelerationStructure(&asDesc, 0, nullptr);
	}

	AccelerationStructureBuffers bottomLevelASBuffers;
	bottomLevelASBuffers.accelerationStructure = bottomLevelAL;
	bottomLevelASBuffers.scratch = scratchResource;
	bottomLevelASBuffers.ResultDataMaxSizeInBytes = bottomLevelPrebuildInfo.ResultDataMaxSizeInBytes;

	return bottomLevelASBuffers;
}


AccelerationStructureBuffers DXRSample::BuildBottomLevelAccelerationStructure(uint32_t numGeometries)
{
	auto device = Application::Get().GetDevice();
	auto geometry = g_Geometries.begin()->second.get();

	std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> geometryDesc;
	geometryDesc.resize(1);
	//for (size_t i = 0; i < numGeometries; i++)
	//{
	//	geometryDesc[i].Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
	//	geometryDesc[i].Triangles.IndexBuffer = geometry->IndexBufferGPU->GetGPUVirtualAddress();
	//	geometryDesc[i].Triangles.IndexCount = geometry->DrawArgs["triangle"].IndexCount;
	//	geometryDesc[i].Triangles.IndexFormat = geometry->IndexFormat;
	//	geometryDesc[i].Triangles.Transform3x4 = 0;
	//	geometryDesc[i].Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;

	//	geometryDesc[i].Triangles.VertexCount = geometry->DrawArgs["triangle"].VertexCount;
	//	geometryDesc[i].Triangles.VertexBuffer.StartAddress = geometry->VertexBufferGPU->GetGPUVirtualAddress();
	//	geometryDesc[i].Triangles.VertexBuffer.StrideInBytes = geometry->VertexByteStride;
	//	// Mark the geometry as opaque. 
	//	// PERFORMANCE TIP: mark geometry as opaque whenever applicable as it can enable important ray processing optimizations.
	//	// Note: When rays encounter opaque geometry an any hit shader will not be executed whether it is present or not.
	//	geometryDesc[i].Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;
	//}


	int i = 0;
	for (const auto& e : geometry->DrawArgs)
	{
		if (i >= numGeometries)
			break;
		D3D12_RAYTRACING_GEOMETRY_DESC geoDesc;
		geometryDesc[i].Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
		geometryDesc[i].Triangles.IndexBuffer = m_indexBuffer.resource->GetGPUVirtualAddress() + e.second.StartIndexLocation * sizeof(uint16_t);
		geometryDesc[i].Triangles.IndexCount = e.second.IndexCount;
		geometryDesc[i].Triangles.IndexFormat = geometry->IndexFormat;
		geometryDesc[i].Triangles.Transform3x4 = 0;
		geometryDesc[i].Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;

		geometryDesc[i].Triangles.VertexCount = e.second.VertexCount;
		geometryDesc[i].Triangles.VertexBuffer.StartAddress = m_vertexBuffer.resource->GetGPUVirtualAddress() + e.second.StartVertexLocation * geometry->VertexByteStride;
		geometryDesc[i].Triangles.VertexBuffer.StrideInBytes = geometry->VertexByteStride;
		// Mark the geometry as opaque. 
		// PERFORMANCE TIP: mark geometry as opaque whenever applicable as it can enable important ray processing optimizations.
		// Note: When rays encounter opaque geometry an any hit shader will not be executed whether it is present or not.
		geometryDesc[i].Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;
		
		i++;
	}




	// Get required sizes for an acceleration structure.
	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC asDesc = {};

	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS& bottomLevelInputs = asDesc.Inputs;
	bottomLevelInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	bottomLevelInputs.Flags = buildFlags;
	bottomLevelInputs.NumDescs = 1;
	bottomLevelInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
	bottomLevelInputs.pGeometryDescs = geometryDesc.data();


	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO bottomLevelPrebuildInfo = {};
	if (g_raytracingAPI == RaytracingAPI::FallbackLayer)
	{
		g_fallbackDevice->GetRaytracingAccelerationStructurePrebuildInfo(&bottomLevelInputs, &bottomLevelPrebuildInfo);
	}
	else // DirectX Ray-tracing
	{
		g_dxrDevice->GetRaytracingAccelerationStructurePrebuildInfo(&bottomLevelInputs, &bottomLevelPrebuildInfo);
	}
	ThrowifFailed(bottomLevelPrebuildInfo.ResultDataMaxSizeInBytes > 0);


	Microsoft::WRL::ComPtr<ID3D12Resource> scratchResource;
	Microsoft::WRL::ComPtr<ID3D12Resource> bottomLevelAL;

	AllocateUAVBuffer(device.Get(), bottomLevelPrebuildInfo.ScratchDataSizeInBytes, scratchResource.GetAddressOf(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, L"ScratchResource");

	{
		D3D12_RESOURCE_STATES initialResourceState;
		if (g_raytracingAPI == RaytracingAPI::FallbackLayer)
		{
			initialResourceState = g_fallbackDevice->GetAccelerationStructureResourceState();
		}
		else // DirectX Ray-tracing
		{
			initialResourceState = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
		}
		AllocateUAVBuffer(device.Get(), bottomLevelPrebuildInfo.ResultDataMaxSizeInBytes, bottomLevelAL.GetAddressOf(), initialResourceState, L"BottomLevelAccelerationStructure");
	}


	{
		asDesc.DestAccelerationStructureData = bottomLevelAL->GetGPUVirtualAddress();
		asDesc.ScratchAccelerationStructureData = scratchResource->GetGPUVirtualAddress();
	}

	if (g_raytracingAPI == RaytracingAPI::FallbackLayer)
	{
		ID3D12DescriptorHeap* pDescriptorHeaps[] = { g_CBVHeap.Get() };
		g_fallbackCommandList->SetDescriptorHeaps(ARRAYSIZE(pDescriptorHeaps), pDescriptorHeaps);
		g_fallbackCommandList->BuildRaytracingAccelerationStructure(&asDesc, 0, nullptr);
	}
	else
	{
		g_dxrCommandList->BuildRaytracingAccelerationStructure(&asDesc, 0, nullptr);
	}

	AccelerationStructureBuffers bottomLevelASBuffers;
	bottomLevelASBuffers.accelerationStructure = bottomLevelAL;
	bottomLevelASBuffers.scratch = scratchResource;
	bottomLevelASBuffers.ResultDataMaxSizeInBytes = bottomLevelPrebuildInfo.ResultDataMaxSizeInBytes;

	return bottomLevelASBuffers;
}


AccelerationStructureBuffers DXRSample::BuildBottomLevelAccelerationStructure(uint32_t numGeometries, std::string name)
{
	auto device = Application::Get().GetDevice();
	auto geometry = g_Geometries.begin()->second.get();


	std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> geometryDesc;
	geometryDesc.resize(1);
	//for (size_t i = 0; i < numGeometries; i++)
	//{
	//	geometryDesc[i].Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
	//	geometryDesc[i].Triangles.IndexBuffer = geometry->IndexBufferGPU->GetGPUVirtualAddress();
	//	geometryDesc[i].Triangles.IndexCount = geometry->DrawArgs["triangle"].IndexCount;
	//	geometryDesc[i].Triangles.IndexFormat = geometry->IndexFormat;
	//	geometryDesc[i].Triangles.Transform3x4 = 0;
	//	geometryDesc[i].Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;

	//	geometryDesc[i].Triangles.VertexCount = geometry->DrawArgs["triangle"].VertexCount;
	//	geometryDesc[i].Triangles.VertexBuffer.StartAddress = geometry->VertexBufferGPU->GetGPUVirtualAddress();
	//	geometryDesc[i].Triangles.VertexBuffer.StrideInBytes = geometry->VertexByteStride;
	//	// Mark the geometry as opaque. 
	//	// PERFORMANCE TIP: mark geometry as opaque whenever applicable as it can enable important ray processing optimizations.
	//	// Note: When rays encounter opaque geometry an any hit shader will not be executed whether it is present or not.
	//	geometryDesc[i].Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;
	//}


	//int i = 0;
	//for (const auto& e : geometry->DrawArgs)
	//{
	//	if (i >= numGeometries)
	//		break;
	//	D3D12_RAYTRACING_GEOMETRY_DESC geoDesc;
	//	geometryDesc[i].Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
	//	geometryDesc[i].Triangles.IndexBuffer = m_indexBuffer.resource->GetGPUVirtualAddress() + e.second.StartIndexLocation * sizeof(uint16_t);
	//	geometryDesc[i].Triangles.IndexCount = e.second.IndexCount;
	//	geometryDesc[i].Triangles.IndexFormat = geometry->IndexFormat;
	//	geometryDesc[i].Triangles.Transform3x4 = 0;
	//	geometryDesc[i].Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;

	//	geometryDesc[i].Triangles.VertexCount = e.second.VertexCount;
	//	geometryDesc[i].Triangles.VertexBuffer.StartAddress = m_vertexBuffer.resource->GetGPUVirtualAddress() + e.second.StartVertexLocation * geometry->VertexByteStride;
	//	geometryDesc[i].Triangles.VertexBuffer.StrideInBytes = geometry->VertexByteStride;
	//	// Mark the geometry as opaque. 
	//	// PERFORMANCE TIP: mark geometry as opaque whenever applicable as it can enable important ray processing optimizations.
	//	// Note: When rays encounter opaque geometry an any hit shader will not be executed whether it is present or not.
	//	geometryDesc[i].Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;
	//	
	//	i++;
	//}

	auto e = geometry->DrawArgs[name];
		D3D12_RAYTRACING_GEOMETRY_DESC geoDesc;
		geometryDesc[0].Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
		geometryDesc[0].Triangles.IndexBuffer = geometry->IndexBufferGPU->GetGPUVirtualAddress() + (e.StartIndexLocation) * sizeof(uint16_t);
		geometryDesc[0].Triangles.IndexCount = e.IndexCount;
		geometryDesc[0].Triangles.IndexFormat = geometry->IndexFormat;
		geometryDesc[0].Triangles.Transform3x4 = 0;
		geometryDesc[0].Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;

		geometryDesc[0].Triangles.VertexCount = e.VertexCount;
		geometryDesc[0].Triangles.VertexBuffer.StartAddress = geometry->VertexBufferGPU->GetGPUVirtualAddress() + (UINT64)(e.StartVertexLocation) * (UINT64)(geometry->VertexByteStride);
		geometryDesc[0].Triangles.VertexBuffer.StrideInBytes = geometry->VertexByteStride;
		// Mark the geometry as opaque. 
		// PERFORMANCE TIP: mark geometry as opaque whenever applicable as it can enable important ray processing optimizations.
		// Note: When rays encounter opaque geometry an any hit shader will not be executed whether it is present or not.
		geometryDesc[0].Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;



	// Get required sizes for an acceleration structure.
	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC asDesc = {};

	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS& bottomLevelInputs = asDesc.Inputs;
	bottomLevelInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	bottomLevelInputs.Flags = buildFlags;
	bottomLevelInputs.NumDescs = 1;
	bottomLevelInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
	bottomLevelInputs.pGeometryDescs = geometryDesc.data();


	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO bottomLevelPrebuildInfo = {};
	if (g_raytracingAPI == RaytracingAPI::FallbackLayer)
	{
		g_fallbackDevice->GetRaytracingAccelerationStructurePrebuildInfo(&bottomLevelInputs, &bottomLevelPrebuildInfo);
	}
	else // DirectX Ray-tracing
	{
		g_dxrDevice->GetRaytracingAccelerationStructurePrebuildInfo(&bottomLevelInputs, &bottomLevelPrebuildInfo);
	}
	ThrowifFailed(bottomLevelPrebuildInfo.ResultDataMaxSizeInBytes > 0);


	Microsoft::WRL::ComPtr<ID3D12Resource> scratchResource;
	Microsoft::WRL::ComPtr<ID3D12Resource> bottomLevelAL;

	AllocateUAVBuffer(device.Get(), bottomLevelPrebuildInfo.ScratchDataSizeInBytes, scratchResource.GetAddressOf(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, L"ScratchResource");

	{
		D3D12_RESOURCE_STATES initialResourceState;
		if (g_raytracingAPI == RaytracingAPI::FallbackLayer)
		{
			initialResourceState = g_fallbackDevice->GetAccelerationStructureResourceState();
		}
		else // DirectX Ray-tracing
		{
			initialResourceState = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
		}
		AllocateUAVBuffer(device.Get(), bottomLevelPrebuildInfo.ResultDataMaxSizeInBytes, bottomLevelAL.GetAddressOf(), initialResourceState, L"BottomLevelAccelerationStructure");
	}


	{
		asDesc.DestAccelerationStructureData = bottomLevelAL->GetGPUVirtualAddress();
		asDesc.ScratchAccelerationStructureData = scratchResource->GetGPUVirtualAddress();
	}

	if (g_raytracingAPI == RaytracingAPI::FallbackLayer)
	{
		ID3D12DescriptorHeap* pDescriptorHeaps[] = { g_CBVHeap.Get() };
		g_fallbackCommandList->SetDescriptorHeaps(ARRAYSIZE(pDescriptorHeaps), pDescriptorHeaps);
		g_fallbackCommandList->BuildRaytracingAccelerationStructure(&asDesc, 0, nullptr);
	}
	else
	{
		g_dxrCommandList->BuildRaytracingAccelerationStructure(&asDesc, 0, nullptr);
	}

	AccelerationStructureBuffers bottomLevelASBuffers;
	bottomLevelASBuffers.accelerationStructure = bottomLevelAL;
	bottomLevelASBuffers.scratch = scratchResource;
	bottomLevelASBuffers.ResultDataMaxSizeInBytes = bottomLevelPrebuildInfo.ResultDataMaxSizeInBytes;

	return bottomLevelASBuffers;
}

AccelerationStructureBuffers DXRSample::CreateTopLevelAccelerationStructure(AccelerationStructureBuffers bottomLevelAS[2])
{
	auto device = Application::Get().GetDevice();
	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;

	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC asDesc = {};
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS& topLevelInputs = asDesc.Inputs;
	topLevelInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	topLevelInputs.Flags = buildFlags;
	topLevelInputs.NumDescs = 3;
	topLevelInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
	topLevelInputs.pGeometryDescs = nullptr;

	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO topLevelPrebuildInfo = {};
	if (g_raytracingAPI == RaytracingAPI::FallbackLayer)
	{
		g_fallbackDevice->GetRaytracingAccelerationStructurePrebuildInfo(&topLevelInputs, &topLevelPrebuildInfo);
	}
	else // DirectX Ray-tracing
	{
		g_dxrDevice->GetRaytracingAccelerationStructurePrebuildInfo(&topLevelInputs, &topLevelPrebuildInfo);
	}
	ThrowifFailed(topLevelPrebuildInfo.ResultDataMaxSizeInBytes > 0);


	Microsoft::WRL::ComPtr<ID3D12Resource> scratchResource;
	Microsoft::WRL::ComPtr<ID3D12Resource> TopLevelAL;
	AllocateUAVBuffer(device.Get(), topLevelPrebuildInfo.ScratchDataSizeInBytes, scratchResource.GetAddressOf(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, L"ScratchResource");

	{
		D3D12_RESOURCE_STATES initialResourceState;
		if (g_raytracingAPI == RaytracingAPI::FallbackLayer)
		{
			initialResourceState = g_fallbackDevice->GetAccelerationStructureResourceState();
		}
		else // DirectX Ray-tracing
		{
			initialResourceState = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
		}
		AllocateUAVBuffer(device.Get(), topLevelPrebuildInfo.ResultDataMaxSizeInBytes, TopLevelAL.GetAddressOf(), initialResourceState, L"TopLevelAccelerationStructure");

	}




	ComPtr<ID3D12Resource> instanceResource;
	if (g_raytracingAPI == RaytracingAPI::FallbackLayer)
	{
		D3D12_RAYTRACING_FALLBACK_INSTANCE_DESC* instanceDescs;

		AllocateUploadBuffer(device.Get(), &instanceDescs, sizeof(D3D12_RAYTRACING_FALLBACK_INSTANCE_DESC) * 3, &instanceResource, L"InstanceDescs");
		instanceResource->Map(0, nullptr, (void**)& instanceDescs);
		ZeroMemory(instanceDescs, sizeof(D3D12_RAYTRACING_FALLBACK_INSTANCE_DESC) * 3);
		WRAPPED_GPU_POINTER blas[] = {
			CreateFallbackWrappedPointer(bottomLevelAS[0].accelerationStructure.Get(), static_cast<UINT>(bottomLevelAS[0].ResultDataMaxSizeInBytes) / sizeof(UINT32)),
			CreateFallbackWrappedPointer(bottomLevelAS[1].accelerationStructure.Get(), static_cast<UINT>(bottomLevelAS[1].ResultDataMaxSizeInBytes) / sizeof(UINT32))
		};
		
		for (size_t i = 0; i < 3; i++)
		{
			instanceDescs[i].InstanceID = i;
			instanceDescs[i].InstanceMask = 0xFF;
			instanceDescs[i].Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
			instanceDescs[i].InstanceContributionToHitGroupIndex = 0; // This is the offset inside the shader-table. We only have a single geometry, so the offset 0
			instanceDescs[i].AccelerationStructure = blas[0];
			XMMATRIX world = XMLoadFloat4x4(&g_AllOpaqueItems.at(i)->WorldMatrix);
			memcpy(instanceDescs[i].Transform, &XMMatrixTranspose(world), sizeof(instanceDescs[i].Transform));

		}
		//instanceDescs[1].InstanceID = g_AllOpaqueItems.at(1)->ObjCBIndex;
		//instanceDescs[1].InstanceMask = 0xFF;
		//instanceDescs[1].Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
		//instanceDescs[1].InstanceContributionToHitGroupIndex = 1; // This is the offset inside the shader-table. We only have a single geometry, so the offset 0
		//instanceDescs[1].AccelerationStructure = blas[1];
		//XMMATRIX world = XMLoadFloat4x4(&g_AllOpaqueItems.at(2)->WorldMatrix);
		//memcpy(instanceDescs[1].Transform, &XMMatrixTranspose(world), sizeof(instanceDescs[1].Transform));

		instanceResource->Unmap(0, nullptr);
	}
	else // DirectX Ray-tracing needs modifications
	{
		D3D12_RAYTRACING_INSTANCE_DESC* instanceDescs;

		AllocateUploadBuffer(device.Get(), &instanceDescs, sizeof(D3D12_RAYTRACING_FALLBACK_INSTANCE_DESC) * 2, &instanceResource, L"InstanceDescs");
		instanceResource->Map(0, nullptr, (void**)& instanceDescs);
		ZeroMemory(instanceDescs, sizeof(D3D12_RAYTRACING_FALLBACK_INSTANCE_DESC) * 2);

		for (size_t i = 0; i < 2; i++)
		{
			instanceDescs[i].InstanceID = g_AllOpaqueItems.at(i)->ObjCBIndex;
			instanceDescs[i].InstanceMask = 0xFF;
			instanceDescs[i].Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
			instanceDescs[i].InstanceContributionToHitGroupIndex = i; // This is the offset inside the shader-table. We only have a single geometry, so the offset 0
			instanceDescs[i].AccelerationStructure = bottomLevelAS[0].accelerationStructure->GetGPUVirtualAddress();
			XMMATRIX world = XMLoadFloat4x4(&g_AllOpaqueItems.at(i)->WorldMatrix);
			memcpy(instanceDescs[i].Transform, &XMMatrixTranspose(world), sizeof(instanceDescs[i].Transform));

		}
		instanceDescs[2].InstanceID = g_AllOpaqueItems.at(2)->ObjCBIndex;
		instanceDescs[2].InstanceMask = 0xFF;
		instanceDescs[2].Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
		instanceDescs[2].InstanceContributionToHitGroupIndex = 2; // This is the offset inside the shader-table. We only have a single geometry, so the offset 0
		instanceDescs[2].AccelerationStructure = bottomLevelAS[1].accelerationStructure->GetGPUVirtualAddress();
		XMMATRIX world = XMLoadFloat4x4(&g_AllOpaqueItems.at(2)->WorldMatrix);
		memcpy(instanceDescs[2].Transform, &XMMatrixTranspose(world), sizeof(instanceDescs[2].Transform));

		instanceResource->Unmap(0, nullptr);
	}

	// Create a wrapped pointer to the acceleration structure.
	if (g_raytracingAPI == RaytracingAPI::FallbackLayer)
	{
		UINT numBufferElements = static_cast<UINT>(topLevelPrebuildInfo.ResultDataMaxSizeInBytes) / sizeof(UINT32);
		g_fallbackTopLevelAccelerationStructrePointer = CreateFallbackWrappedPointer(TopLevelAL.Get(), numBufferElements);
	}

	{
		asDesc.DestAccelerationStructureData = TopLevelAL->GetGPUVirtualAddress();
		asDesc.Inputs.InstanceDescs = instanceResource->GetGPUVirtualAddress();
		asDesc.ScratchAccelerationStructureData = scratchResource->GetGPUVirtualAddress();
	}
	// Build acceleration structure.
	if (g_raytracingAPI == RaytracingAPI::FallbackLayer)
	{
		ID3D12DescriptorHeap* pDescriptorHeaps[] = { g_CBVHeap.Get() };
		g_fallbackCommandList->SetDescriptorHeaps(ARRAYSIZE(pDescriptorHeaps), pDescriptorHeaps);
		g_fallbackCommandList->BuildRaytracingAccelerationStructure(&asDesc, 0, nullptr);
	}
	else // DirectX Ray-tracing
	{
		g_dxrCommandList->BuildRaytracingAccelerationStructure(&asDesc, 0, nullptr);
	}
	
	AccelerationStructureBuffers topLevelASBuffers;
	topLevelASBuffers.accelerationStructure = TopLevelAL;
	topLevelASBuffers.scratch = scratchResource;
	topLevelASBuffers.instanceDesc = instanceResource;
	topLevelASBuffers.ResultDataMaxSizeInBytes = topLevelPrebuildInfo.ResultDataMaxSizeInBytes;

	return topLevelASBuffers;
}

// Build acceleration structures needed for ray-tracing.

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
	AccelerationStructureBuffers tmpbot[2],tmptop;
	tmpbot[0] = BuildBottomLevelAccelerationStructure();
	tmpbot[1] = BuildBottomLevelAccelerationStructure(1,"grid");
	g_bottomLevelAccelerationStructure[0] = tmpbot[0].accelerationStructure;
	g_bottomLevelAccelerationStructure[1] = tmpbot[1].accelerationStructure;


	D3D12_RESOURCE_BARRIER resourceBarriers[2];
	resourceBarriers[0] = CD3DX12_RESOURCE_BARRIER::UAV(tmpbot[0].accelerationStructure.Get());
	resourceBarriers[1] = CD3DX12_RESOURCE_BARRIER::UAV(tmpbot[1].accelerationStructure.Get());

	commandList->ResourceBarrier(1, resourceBarriers);

	tmptop = CreateTopLevelAccelerationStructure(tmpbot);

	g_topLevelAccelerationStructure = tmptop.accelerationStructure;



	UINT currentBackBufferIndex = g_pWindow->GetCurrentBackBufferIndex();


	g_FenceValues[currentBackBufferIndex] = commandQueue->ExecuteCommandList(commandList);

	//currentBackBufferIndex = g_pWindow->Present();

	commandQueue->WaitForFenceValue(g_FenceValues[currentBackBufferIndex]);
	//commandQueue->Flush();
}

// Copy the ray-tracing output to the backbuffer.
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
		dispatchDesc->HitGroupTable.StrideInBytes = g_hitGroupShaderTableStrideInBytes;
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
		commandList->SetComputeRootDescriptorTable(RaytraceGlobalRootSignatureParams::OutputViewSlot, g_raytracingOutputResourceUAVGpuDescriptor);

		auto passCBIndex = g_PassCBVOffset + g_CurrFrameResourceIndex;
		auto passCBHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(g_CBVHeap->GetGPUDescriptorHandleForHeapStart());
		passCBHandle.Offset(passCBIndex, Application::Get().GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
		commandList->SetComputeRootDescriptorTable(RaytraceGlobalRootSignatureParams::ScenceConstantBufferSlot, passCBHandle);

		commandList->SetComputeRootDescriptorTable(RaytraceGlobalRootSignatureParams::VertexBufferSlot, m_indexBuffer.gpuDescriptorHandle);

		g_fallbackCommandList->SetTopLevelAccelerationStructure(RaytraceGlobalRootSignatureParams::AccelerationStructureSlot, g_fallbackTopLevelAccelerationStructrePointer);
		DispatchRays(g_fallbackCommandList.Get(), g_fallbackStateObject.Get(), &dispatchDesc);
	}
	else // DirectX Ray-tracing
	{
		D3D12_DISPATCH_RAYS_DESC dispatchDesc = {};
		commandList->SetDescriptorHeaps(1, g_CBVHeap.GetAddressOf());
		commandList->SetComputeRootDescriptorTable(RaytraceGlobalRootSignatureParams::OutputViewSlot, g_raytracingOutputResourceUAVGpuDescriptor);
		commandList->SetComputeRootShaderResourceView(RaytraceGlobalRootSignatureParams::AccelerationStructureSlot, g_topLevelAccelerationStructure->GetGPUVirtualAddress());
		DispatchRays(g_dxrCommandList.Get(), g_dxrStateObject.Get(), &dispatchDesc);
	}
}

void DXRSample::UpdateForSizeChange(UINT width, UINT height)
{
	float border = 0.01f;
	auto aspectRatio = super::GetAspectRatio();

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
	g_MainPassCB.EyePosW = mEyePos;
	g_MainPassCB.RenderTargetSize = XMFLOAT2((float)super::GetWidth(), (float)super::GetHeight());
	g_MainPassCB.InvRenderTargetSize = XMFLOAT2(1.0f / super::GetWidth(), 1.0f / super::GetHeight());
	g_MainPassCB.NearZ = 1.0f;
	g_MainPassCB.FarZ = 1000.0f;
	g_MainPassCB.TotalTime = e.TotalTime;
	g_MainPassCB.DeltaTime = e.ElapsedTime;
	//g_MainPassCB.lightPosition = XMFLOAT3(-50.0f, 10.0f, 0.0f);
	g_MainPassCB.lightDiffuseColor = XMFLOAT4(1.0f, 1.0f, 1.0f, 0.0f);
	g_MainPassCB.lightAmbientColor = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
	auto currPassCB = g_CurrFrameResource->PassCB.get();
	currPassCB->CopyData(0, g_MainPassCB);
}

void DXRSample::UpdateMaterialCB(UpdateEventArgs& e)
{
	auto currentMaterialCB = g_CurrFrameResource->MaterialCB.get();
	for (auto& e : g_Materials)
	{
		auto mat = e.second.get();
		if (mat->NumFrameDirty > 0)
		{
			MaterialConstants matConstants;
			matConstants.DiffuseAlbedo = mat->DiffuseAlbedo;
			matConstants.FresnelR0 = mat->FresnelR0;
			matConstants.Roughness = mat->Roughness;

			currentMaterialCB->CopyData(mat->MatCBIndex, matConstants);
			mat->NumFrameDirty--;
		}
	}
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
	XMVECTOR pos = XMVectorSet(mEyePos.x, mEyePos.y, mEyePos.z, 0.0f);
	XMVECTOR target = { 0.0f,0.0f,0.0f,1.0f };
	XMVECTOR right = { 1.0f, 0.0f, 0.0f, 0.0f };
	XMVECTOR direction = XMVector4Normalize(target - pos);
	//XMVECTOR up = XMVector3Normalize(XMVector3Cross(direction, right));
	XMVECTOR up = { 0.0f,1.0f,0.0f,0.0f };

	XMMATRIX view = XMMatrixLookAtLH(pos, target, up);
	g_ViewMatrix = view;
	// Update the projection matrix.
	float aspectRatio = super::GetWidth() / static_cast<float>(super::GetHeight());
	g_ProjectionMatrix = XMMatrixPerspectiveFovLH(XMConvertToRadians(g_FoV), aspectRatio, 1.00f, 1250.0f);

	// Cycle through the circular frame resource array.
	g_CurrFrameResourceIndex = (g_CurrFrameResourceIndex + 1) % NUMBER_OF_FRAME_RESOURCES;
	g_CurrFrameResource = g_FrameResources[g_CurrFrameResourceIndex].get();


	UpdateObjectCBs();
	UpdateMainPassCB(e);
	UpdateMaterialCB(e);
	
	
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
	ImGui::SliderFloat("LightPositionX", &g_MainPassCB.lightPosition.x, -100.0f, 100.0f);
	ImGui::SliderFloat("LightPositionY", &g_MainPassCB.lightPosition.y, -100.0f, 100.0f);
	ImGui::SliderFloat("LightPositionZ", &g_MainPassCB.lightPosition.z, -100.0f, 100.0f);

	if (g_raster)
		ImGui::Text("Raster");
	else
		ImGui::Text("Ray-tracing");
	ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
	if (g_isDxrSupported)
		ImGui::Text("DXR is supported");
	else
		ImGui::Text("DXR is not supported");
	if (g_isFallbackSupported)
		ImGui::Text("Fallback Layer is supported");
	else
		ImGui::Text("Fallback Layer is not supported");
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
		commandList->SetGraphicsRootDescriptorTable(RasterRootSignatureParam::PassCB, passCbvHandle);


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

		//commandList->SetGraphicsRootSignature(g_RootSignature.Get());


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
	case KeyCode::R:
		g_raster = !g_raster;
		break;
	case KeyCode::NumPad0:
		if (g_isDxrSupported)
			g_raytracingAPI = RaytracingAPI::DirectXRaytracing;
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
		mRadius = MathHelper::Clamp(mRadius, -50.0f, 1500.0f);
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

void DXRSample::EnableDXR(IDXGIAdapter1* adapter)
{
	g_isFallbackSupported = EnableComputeRaytracingFallback(adapter);

	if (!g_isFallbackSupported)
	{
		OutputDebugString(
			L"Warning: Could not enable Compute Raytracing Fallback (D3D12EnableExperimentalFeatures() failed).\n" \
			L"         Possible reasons: your OS is not in developer mode.\n\n");
	}

	g_isDxrSupported = IsDirectXRaytracingSupported(adapter);

	if (!g_isDxrSupported)
	{
		OutputDebugString(L"Warning: DirectX Raytracing is not supported by your GPU and driver.\n\n");

		ThrowIfFalse(g_isFallbackSupported,
			L"Could not enable compute based fallback raytracing support (D3D12EnableExperimentalFeatures() failed).\n"\
			L"Possible reasons: your OS is not in developer mode.\n\n");
		g_raytracingAPI = RaytracingAPI::FallbackLayer;
	}

}



