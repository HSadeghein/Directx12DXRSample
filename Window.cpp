#define NOMINMAX
#include<cassert>
#include "Window.h"
#include"Application.h"
#include"CommandQueue.h"
#include"Window.h"
#include"Helpers.h"
#include"d3dx12.h"
#include"Game.h"

Window::Window(HWND hWnd, const std::wstring & windowName, int clientWidth, int clientHeight, bool vSync) :
	g_hWnd(hWnd), g_WindowName(windowName), g_ClientWidth(clientWidth), g_ClientHeight(clientHeight), g_VSync(vSync)
{
	g_Fullscreen = false;
	Application& app = Application::Get();

	g_UpdateTimer = new	GameTimer("Update");
	g_RenderTimer = new	GameTimer("Render");

	g_UpdateTimer->Reset();
	g_RenderTimer->Reset();

	g_IsTearingSupported = app.IsTearingSupported();

	g_dxgiSwapChain = CreateSwapChain();
	g_d3d12RTVDescriptorHeap = app.CreateDescriptorHeap(BufferCount, D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	g_RTVDescriptorSize = app.GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	UpdateRenderTargetViews();
}

Window::~Window()
{
	// Window should be destroyed with Application::DestroyWindow before
	// the window goes out of scope.
	assert(!g_hWnd && "Use Application::DestroyWindow before destruction.");
}

HWND Window::GetWindowHandle() const
{
	return g_hWnd;
}

const std::wstring& Window::GetWindowName() const
{
	return g_WindowName;
}

void Window::Show()
{
	::ShowWindow(g_hWnd, SW_SHOW);
}

/**
* Hide the window.
*/
void Window::Hide()
{
	::ShowWindow(g_hWnd, SW_HIDE);
}

void Window::Destroy()
{
	if (auto pGame = g_pGame.lock())
	{
		// Notify the registered game that the window is being destroyed.
		pGame->OnWindowDestroy();
	}
	if (g_hWnd)
	{
		DestroyWindow(g_hWnd);
		g_hWnd = nullptr;
	}
}

int Window::GetClientWidth() const
{
	return g_ClientWidth;
}

int Window::GetClientHeight() const
{
	return g_ClientHeight;
}

bool Window::IsVSync() const
{
	return g_VSync;
}

void Window::SetVSync(bool vSync)
{
	g_VSync = vSync;
}

void Window::ToggleVSync()
{
	SetVSync(!g_VSync);
}

bool Window::IsFullScreen() const
{
	return g_Fullscreen;
}

bool Window::IsTearingSupported() const
{
	return g_IsTearingSupported;
}

// Set the fullscreen state of the window.
void Window::SetFullScreen(bool fullscreen)
{
	if (g_Fullscreen != fullscreen)
	{
		g_Fullscreen = fullscreen;

		if (g_Fullscreen) // Switching to fullscreen.
		{
			// Store the current window dimensions so they can be restored 
			// when switching out of fullscreen state.
			::GetWindowRect(g_hWnd, &g_WindowRect);

			// Set the window style to a borderless window so the client area fills
			// the entire screen.
			UINT windowStyle = WS_OVERLAPPEDWINDOW & ~(WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX);

			::SetWindowLongW(g_hWnd, GWL_STYLE, windowStyle);

			// Query the name of the nearest display device for the window.
			// This is required to set the fullscreen dimensions of the window
			// when using a multi-monitor setup.
			HMONITOR hMonitor = ::MonitorFromWindow(g_hWnd, MONITOR_DEFAULTTONEAREST);
			MONITORINFOEX monitorInfo = {};
			monitorInfo.cbSize = sizeof(MONITORINFOEX);
			::GetMonitorInfo(hMonitor, &monitorInfo);

			::SetWindowPos(g_hWnd, HWND_TOPMOST,
				monitorInfo.rcMonitor.left,
				monitorInfo.rcMonitor.top,
				monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left,
				monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top,
				SWP_FRAMECHANGED | SWP_NOACTIVATE);

			::ShowWindow(g_hWnd, SW_MAXIMIZE);
		}
		else
		{
			// Restore all the window decorators.
			::SetWindowLong(g_hWnd, GWL_STYLE, WS_OVERLAPPEDWINDOW);

			::SetWindowPos(g_hWnd, HWND_NOTOPMOST,
				g_WindowRect.left,
				g_WindowRect.top,
				g_WindowRect.right - g_WindowRect.left,
				g_WindowRect.bottom - g_WindowRect.top,
				SWP_FRAMECHANGED | SWP_NOACTIVATE);

			::ShowWindow(g_hWnd, SW_NORMAL);
		}
	}
}

void Window::ToggleFullScreen()
{
	SetFullScreen(!g_Fullscreen);
}


void Window::RegisterCallbacks(std::shared_ptr<Game> pGame)
{
	g_pGame = pGame;

	return;
}



void Window::OnUpdate(UpdateEventArgs&)
{
	g_UpdateTimer->Tick();
	//OutputDebugString( std::to_wstring(g_UpdateTimer->GetDeltaTime()).c_str());
	//OutputDebugString(L"Update\n");
	if (auto pGame = g_pGame.lock())
	{
		g_UpdateTimer->CalculateFrameStatics();

		UpdateEventArgs updateEventArgs(g_UpdateTimer->GetDeltaTime(), g_UpdateTimer->GetTotalTime());
		pGame->OnUpdate(updateEventArgs);
	}

}

void Window::OnRender(RenderEventArgs&)
{
	g_RenderTimer->Tick();
	//OutputDebugString(std::to_wstring(g_RenderTimer->GetDeltaTime()).c_str());
	//OutputDebugString(L"Render\n");

	if (auto pGame = g_pGame.lock())
	{
		//g_RenderTimer->CalculateFrameStatics();

		RenderEventArgs renderEventArgs(g_RenderTimer->GetDeltaTime(), g_RenderTimer->GetTotalTime());
		pGame->OnRender(renderEventArgs);
	}


}

void Window::OnKeyPressed(KeyEventArgs& e)
{
	if (auto pGame = g_pGame.lock())
	{
		pGame->OnKeyPressed(e);
	}
}

void Window::OnKeyReleased(KeyEventArgs& e)
{
	if (auto pGame = g_pGame.lock())
	{
		pGame->OnKeyReleased(e);
	}
}

// The mouse was moved
void Window::OnMouseMoved(MouseMotionEventArgs& e)
{
	if (auto pGame = g_pGame.lock())
	{
		pGame->OnMouseMoved(e);
	}
}

// A button on the mouse was pressed
void Window::OnMouseButtonPressed(MouseButtonEventArgs& e)
{
	if (auto pGame = g_pGame.lock())
	{
		pGame->OnMouseButtonPressed(e);
	}
}

// A button on the mouse was released
void Window::OnMouseButtonReleased(MouseButtonEventArgs& e)
{
	if (auto pGame = g_pGame.lock())
	{
		pGame->OnMouseButtonReleased(e);
	}
}

// The mouse wheel was moved.
void Window::OnMouseWheel(MouseWheelEventArgs& e)
{
	if (auto pGame = g_pGame.lock())
	{
		pGame->OnMouseWheel(e);
	}
}

void Window::OnResize(ResizeEventArgs& e)
{
	// Update the client size.
	if (g_ClientWidth != e.Width || g_ClientHeight != e.Height)
	{
		g_ClientWidth = std::max(1, e.Width);
		g_ClientHeight = std::max(1, e.Height);

		Application::Get().Flush();

		for (int i = 0; i < BufferCount; ++i)
		{
			g_d3d12BackBuffers[i].Reset();
		}

		DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
		ThrowifFailed(g_dxgiSwapChain->GetDesc(&swapChainDesc));
		ThrowifFailed(g_dxgiSwapChain->ResizeBuffers(BufferCount, g_ClientWidth,
			g_ClientHeight, swapChainDesc.BufferDesc.Format, swapChainDesc.Flags));

		g_CurrentBackBufferIndex = g_dxgiSwapChain->GetCurrentBackBufferIndex();

		UpdateRenderTargetViews();
	}

	if (auto pGame = g_pGame.lock())
	{
		pGame->OnResize(e);
	}
}

Microsoft::WRL::ComPtr<IDXGISwapChain4> Window::CreateSwapChain()
{
	Application& app = Application::Get();

	Microsoft::WRL::ComPtr<IDXGISwapChain4> dxgiSwapChain4;
	Microsoft::WRL::ComPtr<IDXGIFactory4> dxgiFactory4;
	UINT createFactoryFlags = 0;
#if defined(_DEBUG)
	createFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
#endif

	ThrowifFailed(CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(&dxgiFactory4)));

	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
	swapChainDesc.Width = g_ClientWidth;
	swapChainDesc.Height = g_ClientHeight;
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.Stereo = FALSE;
	swapChainDesc.SampleDesc = { 1, 0 };
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.BufferCount = BufferCount;
	swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
	// It is recommended to always allow tearing if tearing support is available.
	swapChainDesc.Flags = g_IsTearingSupported ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;
	ID3D12CommandQueue* pCommandQueue = app.GetCommandQueue()->GetD3D12CommandQueue().Get();

	Microsoft::WRL::ComPtr<IDXGISwapChain1> swapChain1;
	ThrowifFailed(dxgiFactory4->CreateSwapChainForHwnd(
		pCommandQueue,
		g_hWnd,
		&swapChainDesc,
		nullptr,
		nullptr,
		&swapChain1));

	// Disable the Alt+Enter fullscreen toggle feature. Switching to fullscreen
	// will be handled manually.
	ThrowifFailed(dxgiFactory4->MakeWindowAssociation(g_hWnd, DXGI_MWA_NO_ALT_ENTER));

	ThrowifFailed(swapChain1.As(&dxgiSwapChain4));

	g_CurrentBackBufferIndex = dxgiSwapChain4->GetCurrentBackBufferIndex();

	return dxgiSwapChain4;
}

// Update the render target views for the swapchain back buffers.
void Window::UpdateRenderTargetViews()
{
	auto device = Application::Get().GetDevice();

	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(g_d3d12RTVDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

	for (int i = 0; i < BufferCount; ++i)
	{
		Microsoft::WRL::ComPtr<ID3D12Resource> backBuffer;
		ThrowifFailed(g_dxgiSwapChain->GetBuffer(i, IID_PPV_ARGS(&backBuffer)));

		device->CreateRenderTargetView(backBuffer.Get(), nullptr, rtvHandle);

		g_d3d12BackBuffers[i] = backBuffer;

		rtvHandle.Offset(g_RTVDescriptorSize);
	}
}

D3D12_CPU_DESCRIPTOR_HANDLE Window::GetCurrentRenderTargetView() const
{
	return CD3DX12_CPU_DESCRIPTOR_HANDLE(g_d3d12RTVDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
		g_CurrentBackBufferIndex, g_RTVDescriptorSize);
}

Microsoft::WRL::ComPtr<ID3D12Resource> Window::GetCurrentBackBuffer() const
{
	return g_d3d12BackBuffers[g_CurrentBackBufferIndex];
}

UINT Window::GetCurrentBackBufferIndex() const
{
	return g_CurrentBackBufferIndex;
}

UINT Window::Present()
{
	UINT syncInterval = g_VSync ? 1 : 0;
	UINT presentFlags = g_IsTearingSupported && !g_VSync ? DXGI_PRESENT_ALLOW_TEARING : 0;
	ThrowifFailed(g_dxgiSwapChain->Present(syncInterval, presentFlags));
	g_CurrentBackBufferIndex = g_dxgiSwapChain->GetCurrentBackBufferIndex();

	return g_CurrentBackBufferIndex;
}