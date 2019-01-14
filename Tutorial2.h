#pragma once
#include "Game.h"
#include "Window.h"
#include <DirectXMath.h>

class Tutorial2 : public Game
{
public:
	using super = Game;
	Tutorial2(const std::wstring& name, int width, int height, bool vSync);


	// Inherited via Game
	virtual bool LoadContent() override;

	virtual void UnloadContent() override;

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

	///**
	// * Invoked when the mouse is moved over the registered window.
	// */
	//virtual void OnMouseMoved(MouseMotionEventArgs& e) override;

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

	// Root signature
	// The root signature describes the parameters passed to the various stages of the rendering pipeline
	Microsoft::WRL::ComPtr<ID3D12RootSignature> g_RootSignature;

	// Pipeline state object.
	Microsoft::WRL::ComPtr<ID3D12PipelineState> g_PipelineState;

	D3D12_VIEWPORT g_Viewport;
	D3D12_RECT g_ScissorRect;

	float g_FoV;

	DirectX::XMMATRIX g_ModelMatrix;
	DirectX::XMMATRIX g_ViewMatrix;
	DirectX::XMMATRIX g_ProjectionMatrix;

	bool g_ContentLoaded;


};

