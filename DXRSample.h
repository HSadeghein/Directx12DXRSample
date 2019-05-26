#pragma once


#include "Game.h"
#include "Window.h"
#include <DirectXMath.h>
#include"d3dUtil.h"
#include"UploadBuffer.h"
#include"MathHelper.h"
#include"GeometryGenerator.h"
#include"FrameResource.h"
#include"Material.h"
#include "Shaders/RayTracingHlslCompat.h"
#include <d3dcompiler.h>
#include <d3dcommon.h>
#include <minwindef.h>
//FallbackLayer
#include <D3D12RaytracingFallback.h>
#include <DXRHelpers/DXSampleHelper.h>
#include <DXRHelpers/DirectXRaytracingHelper.h>
#include <D3D12RaytracingHelpers.hpp>


#define NUMBER_OF_FRAME_RESOURCES 3


namespace RaytraceGlobalRootSignatureParams {
	enum Value {
		OutputViewSlot = 0,
		AccelerationStructureSlot,
		ScenceConstantBufferSlot,
		Count
	};
}

namespace RaytraceLocalRootSignatureParams {
	enum Value {
		ViewportConstantSlot = 0,
		VertexBufferSlot,
		Count
	};
}
namespace RasterRootSignatureParam {
	enum Value {
		ObjectCB = 0,
		PassCB,
		MaterialCB,
		Count
	};
}
namespace RenderItemsParam {
	enum Value {
		cube0 = 0,
		sphere,
		cylinder,
		plane,
		Count
	};
}
using namespace Microsoft::WRL;




struct VertexNormal
{
	XMFLOAT3 position;
	XMFLOAT3 normal;
};

struct RenderItem
{
	RenderItem() = default;
	XMFLOAT4X4 WorldMatrix = MathHelper::Identity4x4();

	int NumFramesDirty = NUMBER_OF_FRAME_RESOURCES;

	UINT ObjCBIndex = -1;

	Material* Mat = nullptr;
	d3dUtil::MeshGeometry* Geo = nullptr;

	// Primitive topology.
	D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	// DrawIndexedInstanced parameters.
	UINT IndexCount = 0;
	UINT StartIndexLocation = 0;
	int BaseVertexLocation = 0;

};

struct ImguiData
{
	float Light0DiffuseColor[4] = { 1.0f,1.0f,1.0f,1.0f };
	float Light1DiffuseColor[4] = { 1.0f,1.0f,1.0f,1.0f };

	float Light0AmbientColor[4];
	float Light1AmbientColor[4];

	float Light0Position[3];
	float Light1Position[3];

	float Light0Power = { 1.0f };
	float Light0SpecularPower = { 50.0f };

	float CubePosition[3] = { 0.0f,0.0f,0.0f };
	float PlanePosition[3] = { 0.0f,0.0f,0.0f };
	float SpherePosition[3] = { 5.0f,0.0f,0.0f };
	float CylinderPosition[3] = { -5.0f,0.0f,0.0f };
};

struct WrapperPointer
{
	WrapperPointer() = default;
	WRAPPED_GPU_POINTER blas[2] = { NULL,NULL };
	WRAPPED_GPU_POINTER toplevel = { NULL };
};

class DXRSample : public Game
{
public:
	using super = Game;
	DXRSample(const std::wstring& name, int width, int height, bool vSync);


	// Inherited via Game
	virtual bool LoadContent() override;

	virtual void UnloadContent() override;


	void BuildConstantBufferViews();
	void BuildRootSignature();
	void BuildShadersAndInputLayout();
	void BuildRenderItems();
	void CreateRaytracingDeviceDependentResources();
	void BuildShapeGeometry(ID3D12GraphicsCommandList* commandList);
	void BuildTriangle(ID3D12GraphicsCommandList* commandList);
	void BuildRenderTriangleItem();
	UINT CreateBufferSRV(D3DBuffer* buffer, UINT numElements, UINT elementSize);
	void BuildPSOs();
	void BuildMaterials();
	void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems);
	void BuildFrameResources();
	void DoRaytracing(ID3D12GraphicsCommandList * commandList);
	void UpdateForSizeChange(UINT width, UINT height);
	void UpdateObjectCBs();
	void UpdateMainPassCB(UpdateEventArgs& e);

	void UpdateMaterialCB(UpdateEventArgs& e);
	
protected:
	/**
	 *  Update the game logic.
	 */
	virtual void OnUpdate(UpdateEventArgs& e) override;

	/**
	 *  Render stuff.
	 */
	virtual void OnRender(RenderEventArgs& e) override;

	/**
	 * Invoked by the registered window when a key is pressed
	 * while the window has focus.
	 */
	virtual void OnKeyPressed(KeyEventArgs& e) override;
	
	virtual void OnKeyReleased(KeyEventArgs& e) override;

	///**
	// * Invoked when a key on the keyboard is released.
	// */
	//virtual void OnKeyReleased(KeyEventArgs& e) override;

	/**
	 * Invoked when the mouse is moved over the registered window.
	 */
	virtual void OnMouseMoved(MouseMotionEventArgs& e) override;

	///**
	// * Invoked when a mouse button is pressed over the registered window.
	// */
	//virtual void OnMouseButtonPressed(MouseButtonEventArgs& e) override;

	///**
	// * Invoked when a mouse button is released over the registered window.
	// */
	//virtual void OnMouseButtonReleased(MouseButtonEventArgs& e) override;

	/**
	 * Invoked when the mouse wheel is scrolled while the registered window has focus.
	 */
	virtual void OnMouseWheel(MouseWheelEventArgs& e) override;

	void EnableDXR(IDXGIAdapter1* adapter);



	void CreateRaytracingWindowSizeDependentResources();

	/**
	 * Invoked when the attached window is resized.
	 */
	virtual void OnResize(ResizeEventArgs& e) override;


private:
	void CreateDescriptorHeaps(ID3D12Device* device);

	void LoadImgui(HANDLE hWnd, ID3D12Device2* device);

	void TransitionResource(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> commandList,
		Microsoft::WRL::ComPtr<ID3D12Resource> resource, D3D12_RESOURCE_STATES beforeState,
		D3D12_RESOURCE_STATES afterState);

	void ClearRTV(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> commandList, D3D12_CPU_DESCRIPTOR_HANDLE rtv,
		FLOAT* color);

	void ClearDepth(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> commandList, D3D12_CPU_DESCRIPTOR_HANDLE dsv,
		FLOAT depth = 1.0f);

	void UpdateBufferResource(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> commandList, ID3D12Resource** pDestinationResource,
		ID3D12Resource** pIntermediateResource, size_t numElements, size_t elementSize, const void* bufferData,
		D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE);

	void ResizeDepthBuffer(int width, int height);
	void CreateRaytracingDeviceCommandlist();
	void CreateRaytracingOutputResource();
	void BuildShaderTables();
	void CreateRaytracingRootSignatures();
	void SerializeAndCreateRaytracingRootSignature(D3D12_ROOT_SIGNATURE_DESC & desc, ComPtr<ID3D12RootSignature>* rootSig);
	void CreateRaytracingPipelineStateObject();
	void CreateLocalRootSignatureSubobjects(CD3D12_STATE_OBJECT_DESC * raytracingPipeline);
	AccelerationStructureBuffers BuildBottomLevelAccelerationStructure();
	AccelerationStructureBuffers BuildBottomLevelAccelerationStructure(uint32_t numGeometries, std::string name);
	AccelerationStructureBuffers CreateTopLevelAccelerationStructure(AccelerationStructureBuffers bottomLevelAS[4]);
	void UpdateTopLevelAccelerationStructure(AccelerationStructureBuffers bottomLevelAS[4], AccelerationStructureBuffers topLevel, ID3D12GraphicsCommandList* commandList);
	void BuildAccelerationStructures();
	void CopyRaytracingOutputToBackbuffer(ID3D12GraphicsCommandList * commandList);
	WRAPPED_GPU_POINTER CreateFallbackWrappedPointer(ID3D12Resource * resource, UINT bufferNumElements);
	UINT AllocateDescriptor(D3D12_CPU_DESCRIPTOR_HANDLE * cpuDescriptor, UINT descriptorIndexToUse = UINT_MAX);
	void ReleaseWindowSizeDependentResource();

	void ReleaseDeviceDependentResource();

	void UpdateUI();

	uint64_t g_FenceValues[Window::BufferCount] = {};

	Microsoft::WRL::ComPtr<ID3D12Resource> g_VertexBuffer;
	D3D12_VERTEX_BUFFER_VIEW g_VertexBufferView;

	Microsoft::WRL::ComPtr<ID3D12Resource> g_IndexBuffer;
	D3D12_INDEX_BUFFER_VIEW g_IndexBufferView;

	// Depth buffer.
	Microsoft::WRL::ComPtr<ID3D12Resource> g_DepthBuffer;
	// Descriptor heap for depth buffer.
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> g_DSVHeap;

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> g_CBVHeap;

	// Root signature
	// The root signature describes the parameters passed to the various stages of the rendering pipeline
	Microsoft::WRL::ComPtr<ID3D12RootSignature> g_RootSignature;

	// Pipeline state object.
	Microsoft::WRL::ComPtr<ID3D12PipelineState> g_PipelineState;

	//Shaders binary
	Microsoft::WRL::ComPtr<ID3DBlob> g_VertexShaderBlob;
	Microsoft::WRL::ComPtr<ID3DBlob> g_PixelShaderBlob;
	//Input Layout form
	std::vector<D3D12_INPUT_ELEMENT_DESC> g_InputLayout;


	//FallBackLayer
	ComPtr<ID3D12RaytracingFallbackDevice> g_fallbackDevice;
	ComPtr<ID3D12RaytracingFallbackCommandList> g_fallbackCommandList;
	ComPtr<ID3D12RaytracingFallbackStateObject> g_fallbackStateObject;
	WRAPPED_GPU_POINTER g_fallbackTopLevelAccelerationStructrePointer;

	//Directx Raytracing ==> DXR
	ComPtr<ID3D12Device5> g_dxrDevice;
	ComPtr<ID3D12GraphicsCommandList4> g_dxrCommandList;
	ComPtr<ID3D12StateObjectPrototype> g_dxrStateObject;
	bool g_isDxrSupported;
	bool g_isFallbackSupported;
	//Raytracing Root Signatures
	ComPtr<ID3D12RootSignature> g_raytracingGlobalRootSignature;
	ComPtr<ID3D12RootSignature> g_raytracingLocalRootSignature;

	//Acceleration Structure
	ComPtr<ID3D12Resource> g_accelerationStructure;
	AccelerationStructureBuffers g_topLevelAccelerationStructure;
	AccelerationStructureBuffers g_bottomLevelAccelerationStructure[4];

	//Raytracing Output
	ComPtr<ID3D12Resource> g_raytracingOutput;
	D3D12_GPU_DESCRIPTOR_HANDLE g_raytracingOutputResourceUAVGpuDescriptor;
	UINT g_raytracingOutputResourceUAVDescriptorHeapIndex;

	// Shader tables
	static const wchar_t* c_hitShadowGroupName;
	static const wchar_t* c_hitGroupName;
	static const wchar_t* c_raygenShaderName;
	static const wchar_t* c_closestHitShaderName;
	static const wchar_t* c_missShaderName;
	static const wchar_t* c_closestHitShadowShaderName;
	static const wchar_t* c_missShadowShaderName;

	ComPtr<ID3D12Resource> m_missShaderTable;
	ComPtr<ID3D12Resource> m_hitGroupShaderTable;
	ComPtr<ID3D12Resource> m_rayGenShaderTable;


	RaytracingAPI g_raytracingAPI = RaytracingAPI::FallbackLayer;
	bool g_forceComputeFallback;

	//For the initializing the rasterizer
	D3D12_VIEWPORT g_Viewport;
	D3D12_RECT g_ScissorRect;

	float g_FoV;

	DirectX::XMMATRIX g_ModelMatrix;
	DirectX::XMMATRIX g_ViewMatrix;
	DirectX::XMMATRIX g_ProjectionMatrix;

	bool g_ContentLoaded;


	std::unique_ptr<d3dUtil::MeshGeometry> g_MeshBox;
	std::unique_ptr<UploadBuffer<ObjectConstants>> g_ObjectCB = nullptr;

	std::vector<std::unique_ptr<RenderItem>> g_AllRenderItems;
	std::vector<RenderItem*> g_AllOpaqueItems;

	std::unordered_map<std::string, std::unique_ptr<d3dUtil::MeshGeometry>> g_Geometries;

	std::vector<std::unique_ptr<FrameResource>> g_FrameResources;
	FrameResource* g_CurrFrameResource = nullptr;
	int g_CurrFrameResourceIndex = 0;

	UINT g_PassCBVOffset = 0;
	UINT g_ObjectCBVOffset = 0;
	UINT g_RaytracingCBVOffset = 0;
	UINT g_RaytracingDescriptorsAllocated = 0;
	UINT g_hitGroupShaderTableStrideInBytes = 0;
	UINT g_missShaderTableStrideInBytes = 0;

	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12PipelineState>> g_PSOs;
	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3DBlob>> g_Shaders;
	std::unordered_map<std::string, std::unique_ptr<Material>> g_Materials;
	PassConstants g_MainPassCB;
	POINT mLastMousePos;
	DirectX::XMFLOAT3 mEyePos = { 0.0f, 0.0f, 0.0f };


	float mTheta = 1.5f*DirectX::XM_PI;
	float mPhi = 0.2f*DirectX::XM_PI;
	float mRadius = 15.0f;
	XMFLOAT4 mWASD = XMFLOAT4(0, 0, 0, 0);

	bool isWireFrameMode = false;
	bool g_raster = true;



	D3DBuffer m_localIndexBufferBox;
	D3DBuffer m_localVertexBufferBox;
	D3DBuffer m_localIndexBufferGrid;
	D3DBuffer m_localVertexBufferGrid;
	D3DBuffer m_localIndexBufferSphere;
	D3DBuffer m_localVertexBufferSphere;
	D3DBuffer m_localIndexBufferCylinder;
	D3DBuffer m_localVertexBufferCylinder;

	ImguiData uiData{};
	float lightColor[4] = { 1.0f,1.0f,1.0f,1.0f };
	float lightPosition[3] = { 0.0f,0.0f,0.0f };
	WrapperPointer mAccelerationWrapperPointer = {};
	WRAPPED_GPU_POINTER* mBLAS = new WRAPPED_GPU_POINTER[2];

};

