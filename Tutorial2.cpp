#define NOMINMAX
#include "Tutorial2.h"
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


using namespace Microsoft::WRL;

using namespace DirectX;



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

Tutorial2::Tutorial2(const std::wstring & name, int width, int height, bool vSync) :
	super(name, width, height, vSync),
	g_Viewport(CD3DX12_VIEWPORT(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height))),
	g_ScissorRect(CD3DX12_RECT(0, 0, LONG_MAX, LONG_MAX)),
	g_FoV(45.0),
	g_ContentLoaded(false)
{
}


void Tutorial2::UpdateBufferResource(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> commandList, ID3D12Resource ** pDestinationResource, ID3D12Resource ** pIntermediateResource, size_t numElements, size_t elementSize, const void * bufferData, D3D12_RESOURCE_FLAGS flags)
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

void Tutorial2::BuildPSOs()
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

void Tutorial2::DrawRenderItems(ID3D12GraphicsCommandList * cmdList, const std::vector<RenderItem*>& ritems)
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

void Tutorial2::LoadImgui(HANDLE hWnd, ID3D12Device2* device)
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

void Tutorial2::BuildConstantBufferViews()
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

void Tutorial2::BuildRootSignature()
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
		D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;

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

void Tutorial2::CreateDescriptorHeaps(ID3D12Device * device)
{
	UINT objCount = (UINT)g_AllRenderItems.size();

	// Need a CBV descriptor for each object for each frame resource,
	// +1 for the perPass CBV for each frame resource. +1 for IMGUI
	UINT numDescriptors = (objCount + 1) * NUMBER_OF_FRAME_RESOURCES + 1;

	// Save an offset to the start of the pass CBVs.  These are the last 3 descriptors.
	g_ObjectCBVOffset = 1;
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

void Tutorial2::BuildFrameResources()
{
	auto device = Application::Get().GetDevice();
	for (size_t i = 0; i < NUMBER_OF_FRAME_RESOURCES; i++)
	{
		g_FrameResources.push_back(std::make_unique<FrameResource>(device.Get(), 1, (UINT)g_AllRenderItems.size()));
	}
}
bool Tutorial2::LoadContent()
{
	auto device = Application::Get().GetDevice();
	auto commandQueue = Application::Get().GetCommandQueue(D3D12_COMMAND_LIST_TYPE_COPY);
	auto commandList = commandQueue->GetCommandList();




	BuildRootSignature();
	BuildShadersAndInputLayout();
	BuildShapeGeometry(commandList.Get());
	BuildRenderItems();
	BuildFrameResources();
	// Create the descriptor heap for the depth-stencil view and constatn buffer
	CreateDescriptorHeaps(device.Get());
	//LoadImgui
	LoadImgui(super::g_pWindow->GetWindowHandle(), device.Get());
	BuildConstantBufferViews();
	BuildPSOs();

	//BuildConstantBuffers();
	//BuildRootSignature();
	//BuildBoxGeometry();

	// Upload vertex buffer data.
	//ComPtr<ID3D12Resource> intermediateVertexBuffer;
	//UpdateBufferResource(commandList,
	//	&g_VertexBuffer, &intermediateVertexBuffer,
	//	_countof(g_Vertices), sizeof(VertexPosColor), g_Vertices);

	//// Create the vertex buffer view.
	//g_VertexBufferView.BufferLocation = g_VertexBuffer->GetGPUVirtualAddress();
	//g_VertexBufferView.SizeInBytes = sizeof(g_Vertices);
	//g_VertexBufferView.StrideInBytes = sizeof(VertexPosColor);

	//// Upload index buffer data.
	//ComPtr<ID3D12Resource> intermediateIndexBuffer;
	//UpdateBufferResource(commandList,
	//	&g_IndexBuffer, &intermediateIndexBuffer,
	//	_countof(g_Indicies), sizeof(WORD), g_Indicies);

	//// Create index buffer view.
	//g_IndexBufferView.BufferLocation = g_IndexBuffer->GetGPUVirtualAddress();
	//g_IndexBufferView.Format = DXGI_FORMAT_R16_UINT;
	//g_IndexBufferView.SizeInBytes = sizeof(g_Indicies);


	//// Load the vertex shader.
	//ComPtr<ID3DBlob> vertexShaderBlob;
	//ThrowifFailed(D3DReadFileToBlob(L"VertexShader.cso", &vertexShaderBlob));

	//// Load the pixel shader.
	//ComPtr<ID3DBlob> pixelShaderBlob;
	//ThrowifFailed(D3DReadFileToBlob(L"PixelShader.cso", &pixelShaderBlob));

	//// Create the vertex input layout
	//D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
	//	{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	//	{ "COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	//};

	//// Create a root signature.
	//D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};
	//featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
	//if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
	//{
	//	featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
	//}

	//// Allow input layout and deny unnecessary access to certain pipeline stages.
	//D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
	//	D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
	//	D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
	//	D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
	//	D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
	//	D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;

	//// A single 32-bit constant root parameter that is used by the vertex shader.
	//CD3DX12_ROOT_PARAMETER1 rootParameters[1];
	//rootParameters[0].InitAsConstants(sizeof(XMMATRIX) / 4, 0, 0, D3D12_SHADER_VISIBILITY_VERTEX);

	//CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDescription;
	//rootSignatureDescription.Init_1_1(_countof(rootParameters), rootParameters, 0, nullptr, rootSignatureFlags);

	//// Serialize the root signature.
	//ComPtr<ID3DBlob> rootSignatureBlob;
	//ComPtr<ID3DBlob> errorBlob;
	//ThrowifFailed(D3DX12SerializeVersionedRootSignature(&rootSignatureDescription,
	//	featureData.HighestVersion, &rootSignatureBlob, &errorBlob));
	//// Create the root signature.
	//ThrowifFailed(device->CreateRootSignature(0, rootSignatureBlob->GetBufferPointer(),
	//	rootSignatureBlob->GetBufferSize(), IID_PPV_ARGS(&g_RootSignature)));

	//struct PipelineStateStream
	//{
	//	CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE pRootSignature;
	//	CD3DX12_PIPELINE_STATE_STREAM_INPUT_LAYOUT InputLayout;
	//	CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY PrimitiveTopologyType;
	//	CD3DX12_PIPELINE_STATE_STREAM_VS VS;
	//	CD3DX12_PIPELINE_STATE_STREAM_PS PS;
	//	CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL_FORMAT DSVFormat;
	//	CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS RTVFormats;
	//} pipelineStateStream;

	//D3D12_RT_FORMAT_ARRAY rtvFormats = {};
	//rtvFormats.NumRenderTargets = 1;
	//rtvFormats.RTFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;

	//pipelineStateStream.pRootSignature = g_RootSignature.Get();
	//pipelineStateStream.InputLayout = { inputLayout, _countof(inputLayout) };
	//pipelineStateStream.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	//pipelineStateStream.VS = CD3DX12_SHADER_BYTECODE(vertexShaderBlob.Get());
	//pipelineStateStream.PS = CD3DX12_SHADER_BYTECODE(pixelShaderBlob.Get());
	//pipelineStateStream.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	//pipelineStateStream.RTVFormats = rtvFormats;

	//D3D12_PIPELINE_STATE_STREAM_DESC pipelineStateStreamDesc = {
	//	sizeof(PipelineStateStream), &pipelineStateStream
	//};
	//ThrowifFailed(device->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&g_PipelineState)));

	auto fenceValue = commandQueue->ExecuteCommandList(commandList);
	commandQueue->WaitForFenceValue(fenceValue);
	commandQueue->Flush();
	
	g_ContentLoaded = true;

	// Resize/Create the depth buffer.
	ResizeDepthBuffer(super::GetWidth(), super::GetHeight());

	return true;
}

void Tutorial2::BuildShapeGeometry(ID3D12GraphicsCommandList* commandList)
{
	auto device = Application::Get().GetDevice();

	GeometryGenerator geoGen;
	GeometryGenerator::MeshData box = geoGen.CreateBox(1.5f, 0.5f, 1.5f, 3);
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

void Tutorial2::UnloadContent()
{
	g_ContentLoaded = false;
}

void Tutorial2::OnResize(ResizeEventArgs& e)
{
	std::wstring s = L"Client Width" + std::to_wstring(super::GetWidth()) + L"\n";
	OutputDebugString(s.c_str());
	s = L"Width" + std::to_wstring(e.Width) + L"\n";
	OutputDebugString(s.c_str());
	if (e.Width != super::GetWidth() || e.Height != super::GetHeight())
	{
		super::OnResize(e);

		g_Viewport = CD3DX12_VIEWPORT(0.0f, 0.0f,
			static_cast<float>(e.Width), static_cast<float>(e.Height));

		ResizeDepthBuffer(e.Width, e.Height);
	}

}

void Tutorial2::BuildRenderItems()
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


void Tutorial2::BuildShadersAndInputLayout()
{
	//// Load the vertex shader.
	//ComPtr<ID3DBlob> g_VertexShaderBlob;
	//ThrowifFailed(D3DReadFileToBlob(L"VertexShader.cso", g_VertexShaderBlob.GetAddressOf()));

	//// Load the pixel shader.
	//ComPtr<ID3DBlob> g_PixelShaderBlob;
	//ThrowifFailed(D3DReadFileToBlob(L"PixelShader.cso", &g_PixelShaderBlob));

	g_Shaders["standardVS"] = d3dUtil::CompileShader(L"color.hlsl", nullptr, "vertex", "vs_5_1");
	g_Shaders["opaquePS"] = d3dUtil::CompileShader(L"color.hlsl", nullptr, "pixel", "ps_5_1");

	// Create the vertex input layout
	g_InputLayout = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};


}

void Tutorial2::ResizeDepthBuffer(int width, int height)
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
void Tutorial2::UpdateObjectCBs()
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

void Tutorial2::UpdateMainPassCB(UpdateEventArgs& e)
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
	g_MainPassCB.RenderTargetSize = XMFLOAT2((float)super::g_pWindow->GetClientWidth(), (float)super::g_pWindow->GetClientHeight());
	g_MainPassCB.InvRenderTargetSize = XMFLOAT2(1.0f / super::g_pWindow->GetClientWidth(), 1.0f / super::g_pWindow->GetClientHeight());
	g_MainPassCB.NearZ = 1.0f;
	g_MainPassCB.FarZ = 1000.0f;
	g_MainPassCB.TotalTime = e.TotalTime;
	g_MainPassCB.DeltaTime = e.ElapsedTime;

	auto currPassCB = g_CurrFrameResource->PassCB.get();
	currPassCB->CopyData(0, g_MainPassCB);
}

void Tutorial2::OnUpdate(UpdateEventArgs& e)
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

	// Has the GPU finished processing the commands of the current frame resource?
	// If not, wait until the GPU has completed commands up to this fence point.
	//if (g_CurrFrameResource->FenceValue != 0 && mFence->GetCompletedValue() < mCurrFrameResource->Fence)
	//{
	//	HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);
	//	ThrowIfFailed(mFence->SetEventOnCompletion(mCurrFrameResource->Fence, eventHandle));
	//	WaitForSingleObject(eventHandle, INFINITE);
	//	CloseHandle(eventHandle);
	//}
	//if (g_CurrFrameResource->FenceValue != 0 && mFence->GetCompletedValue() < g_CurrFrameResource->FenceValue)
	//{
	//	HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);
	//	ThrowifFailed(mFence->SetEventOnCompletion(g_CurrFrameResource->FenceValue, eventHandle));
	//	WaitForSingleObject(eventHandle, INFINITE);
	//	CloseHandle(eventHandle);
	//}
	UpdateObjectCBs();
	UpdateMainPassCB(e);

	//// Update the model matrix.
	//float angle = static_cast<float>(e.TotalTime * 10);
	//const XMVECTOR rotationAxis = XMVectorSet(0, 1, 1, 0);
	//g_ModelMatrix = XMMatrixRotationAxis(rotationAxis, XMConvertToRadians(angle));

	//// Update the view matrix.
	//const XMVECTOR eyePosition = XMVectorSet(0, 0, -10, 1);
	//const XMVECTOR focusPoint = XMVectorSet(0, 0, 0, 1);
	//const XMVECTOR upDirection = XMVectorSet(0, 1, 0, 0);
	//g_ViewMatrix = XMMatrixLookAtLH(eyePosition, focusPoint, upDirection);

	//// Update the projection matrix.
	//float aspectRatio = super::g_pWindow->GetClientWidth() / static_cast<float>(super::g_pWindow->GetClientHeight());
	//g_ProjectionMatrix = XMMatrixPerspectiveFovLH(XMConvertToRadians(g_FoV), aspectRatio, 0.001f, 1000.0f);
}

// Transition a resource
void Tutorial2::TransitionResource(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> commandList,
	Microsoft::WRL::ComPtr<ID3D12Resource> resource,
	D3D12_RESOURCE_STATES beforeState, D3D12_RESOURCE_STATES afterState)
{
	CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
		resource.Get(),
		beforeState, afterState);

	commandList->ResourceBarrier(1, &barrier);
}

// Clear a render target.
void Tutorial2::ClearRTV(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> commandList,
	D3D12_CPU_DESCRIPTOR_HANDLE rtv, FLOAT* clearColor)
{
	commandList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
}

void Tutorial2::ClearDepth(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> commandList,
	D3D12_CPU_DESCRIPTOR_HANDLE dsv, FLOAT depth)
{
	commandList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, depth, 0, 0, nullptr);
}

void Tutorial2::OnRender(RenderEventArgs& e)
{
	super::OnRender(e);

	

	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
	
	ImGui::Begin("Hello, world!");                          // Create a window called "Hello, world!" and append into it.

	if(ImGui::Button("Button", ImVec2(50, 50)))
	ImGui::Text("This is some useful text.");               // Display some text (you can use a format strings too)
	ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
	ImGui::End();





	auto commandQueue = Application::Get().GetCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT);
	auto commandList = commandQueue->GetCommandList();

	UINT currentBackBufferIndex = g_pWindow->GetCurrentBackBufferIndex();
	auto backBuffer = g_pWindow->GetCurrentBackBuffer();
	auto rtv = g_pWindow->GetCurrentRenderTargetView();
	auto dsv = g_DSVHeap->GetCPUDescriptorHandleForHeapStart();

	commandList->RSSetViewports(1, &g_Viewport);
	commandList->RSSetScissorRects(1, &g_ScissorRect);
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

	//commandList->SetPipelineState(g_PipelineState.Get());

	//commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	//commandList->IASetVertexBuffers(0, 1, &g_VertexBufferView);
	//commandList->IASetIndexBuffer(&g_IndexBufferView);




	// Update the MVP matrix
	//XMMATRIX mvpMatrix = XMMatrixMultiply(g_ModelMatrix, g_ViewMatrix);
	//mvpMatrix = XMMatrixMultiply(mvpMatrix, g_ProjectionMatrix);
	//commandList->SetGraphicsRoot32BitConstants(0, sizeof(XMMATRIX) / 4, &mvpMatrix, 0);

	//commandList->DrawIndexedInstanced(_countof(g_Indicies), 1, 0, 0, 0);



	//commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);




	ImGui::Render();
	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList.Get());

	// Present
	{
		TransitionResource(commandList, backBuffer,
			D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);

		g_FenceValues[currentBackBufferIndex] = commandQueue->ExecuteCommandList(commandList);

		currentBackBufferIndex = g_pWindow->Present();

		commandQueue->WaitForFenceValue(g_FenceValues[currentBackBufferIndex]);
		commandQueue->Flush();
	}
}

void Tutorial2::OnKeyPressed(KeyEventArgs& e)
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
	}
}
void Tutorial2::OnMouseMoved(MouseMotionEventArgs & e)
{
	if (e.LeftButton)
	{
		OutputDebugString(L"Left Mouse Button Clicked");
		// Make each pixel correspond to a quarter of a degree.
		float dx = XMConvertToRadians(0.25f*static_cast<float>(e.X - mLastMousePos.x));
		float dy = XMConvertToRadians(0.25f*static_cast<float>(e.Y - mLastMousePos.y));

		// Update angles based on input to orbit camera around box.
		mTheta += dx;
		mPhi += dy;

		// Restrict the angle mPhi.
		mPhi = MathHelper::Clamp(mPhi, 0.1f, DirectX::XM_PI - 0.1f);
	}
	else if (e.RightButton)
	{
		// Make each pixel correspond to 0.2 unit in the scene.
		float dx = 0.05f*static_cast<float>(e.X - mLastMousePos.x);
		float dy = 0.05f*static_cast<float>(e.Y - mLastMousePos.y);

		// Update the camera radius based on input.
		mRadius += dx - dy;

		// Restrict the radius.
		mRadius = MathHelper::Clamp(mRadius, 5.0f, 150.0f);
	}

	mLastMousePos.x = e.X;
	mLastMousePos.y = e.Y;

}
void Tutorial2::OnMouseWheel(MouseWheelEventArgs& e)
{
	g_FoV -= e.WheelDelta;
	g_FoV = clamp<float>(g_FoV, 12.0f, 90.0f);

	char buffer[256];
	sprintf_s(buffer, "FoV: %f\n", g_FoV);
	OutputDebugStringA(buffer);
}
