#pragma once

#define WIN32_LEAD_AND_MEAN

#include<Windows.h>
#include<wrl.h>
#include<d3d12.h>
#include<dxgi1_5.h>
#include<string>
#include<memory>

#include"Events.h"
#include"GameTimer.h"

class Game;

class Window
{
public:
	static const UINT BufferCount = 3;

	HWND GetWindowHandle() const;

	void Destroy();

	const std::wstring& GetWindowName() const;

	int GetClientWidth() const;
	int GetClientHeight() const;
	

	bool IsVSync() const;
	void SetVSync(bool vSync);
	void ToggleVSync();

	bool IsFullScreen() const;

	void SetFullScreen(bool fullScreen);
	void ToggleFullScreen();

	void Show();
	void Hide();

	UINT GetCurrentBackBufferIndex() const;

	/**
	* Present the swapchain's back buffer to the screen.
	* Returns the current back buffer index after the present.
	*/
	UINT Present();

	D3D12_CPU_DESCRIPTOR_HANDLE GetCurrentRenderTargetView() const;

	Microsoft::WRL::ComPtr<ID3D12Resource> GetCurrentBackBuffer() const;

protected:
	friend LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

	friend class Application;
	friend class Game;

	Window() = delete;
	Window(HWND hWnd, const std::wstring& windowName, int clientWidth, int clientHeight, bool vSync);
	virtual ~Window();

	void CalculateFrameStatics(GameTimer& timer);

	// Register a Game with this window. This allows
	// the window to callback functions in the Game class.
	void RegisterCallbacks(std::shared_ptr<Game> pGame);

	virtual void OnUpdate(UpdateEventArgs& e);
	virtual void OnRender(RenderEventArgs& e);

	// A keyboard key was pressed
	virtual void OnKeyPressed(KeyEventArgs& e);
	// A keyboard key was released
	virtual void OnKeyReleased(KeyEventArgs& e);

	// The mouse was moved
	virtual void OnMouseMoved(MouseMotionEventArgs& e);
	// A button on the mouse was pressed
	virtual void OnMouseButtonPressed(MouseButtonEventArgs& e);
	// A button on the mouse was released
	virtual void OnMouseButtonReleased(MouseButtonEventArgs& e);
	// The mouse wheel was moved.
	virtual void OnMouseWheel(MouseWheelEventArgs& e);

	// The window was resized.
	virtual void OnResize(ResizeEventArgs& e);

	Microsoft::WRL::ComPtr<IDXGISwapChain4> CreateSwapChain();

	void UpdateRenderTargetViews();

private:
	// Windows should not be copied.
	Window(const Window& copy) = delete;
	Window& operator=(const Window& other) = delete;

	HWND g_hWnd;

	std::wstring g_WindowName;

	int g_ClientWidth;
	int g_ClientHeight;
	bool g_VSync;
	bool g_Fullscreen;

	GameTimer g_UpdateTimer;
	GameTimer g_RenderTimer;


	uint64_t g_FrameCounter;

	std::weak_ptr<Game> g_pGame;

	Microsoft::WRL::ComPtr<IDXGISwapChain4> g_dxgiSwapChain;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> g_d3d12RTVDescriptorHeap;
	Microsoft::WRL::ComPtr<ID3D12Resource> g_d3d12BackBuffers[BufferCount];

	UINT g_RTVDescriptorSize;
	UINT g_CurrentBackBufferIndex;

	RECT g_WindowRect;
	bool g_IsTearingSupported;
};

