#pragma once
#include "Game.h"
#include "Window.h"
#include <DirectXMath.h>
#include"d3dUtil.h"
#include"UploadBuffer.h"
#include"MathHelper.h"
#include"GeometryGenerator.h"
#include"FrameResource.h"
#include <d3dcompiler.h>
#include <d3dcommon.h>

#define NUMBER_OF_FRAME_RESOURCES 3


struct VertexPosColor
{
	XMFLOAT3 position;
	XMFLOAT3 color;
};

struct RenderItem
{
	RenderItem() = default;
	XMFLOAT4X4 WorldMatrix = MathHelper::Identity4x4();

	int NumFramesDirty = NUMBER_OF_FRAME_RESOURCES;

	UINT ObjCBIndex = -1;

	d3dUtil::MeshGeometry* Geo = nullptr;

	// Primitive topology.
	D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	// DrawIndexedInstanced parameters.
	UINT IndexCount = 0;
	UINT StartIndexLocation = 0;
	int BaseVertexLocation = 0;

};

class Tutorial2 : public Game
{
public:
	using super = Game;
	Tutorial2(const std::wstring& name, int width, int height, bool vSync);


	// Inherited via Game
	virtual bool LoadContent() override;

	virtual void UnloadContent() override;

	void BuildConstantBufferViews();
	void BuildRootSignature();
	void BuildShadersAndInputLayout();
	void BuildRenderItems();
	void BuildShapeGeometry(ID3D12GraphicsCommandList* commandList);
	void BuildPSOs();
	void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems);
	void BuildFrameResources();
	void UpdateObjectCBs();
	void UpdateMainPassCB(UpdateEventArgs& e);
	
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

	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12PipelineState>> g_PSOs;
	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3DBlob>> g_Shaders;

	PassConstants g_MainPassCB;
	POINT mLastMousePos;
	DirectX::XMFLOAT3 mEyePos = { 0.0f, 0.0f, 0.0f };


	float mTheta = 1.5f*DirectX::XM_PI;
	float mPhi = 0.2f*DirectX::XM_PI;
	float mRadius = 15.0f;

	bool isWireFrameMode = false;

};

