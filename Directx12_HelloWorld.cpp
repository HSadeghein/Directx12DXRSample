////#pragma comment(lib, "dxgi.lib")
////#pragma comment(lib, "windowscodecs.lib")
////#pragma comment(lib,"d3d12.lib")
////#pragma comment(lib,"MSVCRTD.lib")
//
////#define _CRTDBG_MAP_ALLOC  
////#include <stdlib.h>  
////#include <crtdbg.h>
//
//#define WIN32_LEAN_AND_MEAN
//#include <Windows.h>
//#include <shellapi.h> // For CommandLineToArgvW
////#include "pch.h"
//// The min/max macros conflict with like-named member functions.
//// Only use std::min and std::max defined in <algorithm>.
//#if defined(min)
//#undef min
//#endif
//
//#if defined(max)
//#undef max
//#endif
//
//// In order to define a function called CreateWindow, the Windows macro needs to
//// be undefined.
//#if defined(CreateWindow)
//#undef CreateWindow
//#endif
//
//// Windows Runtime Library. Needed for Microsoft::WRL::ComPtr<> template class.
//#include <wrl.h>
//using namespace Microsoft::WRL;
//
//// DirectX 12 specific headers.
//#include <d3d12.h>
//#include <dxgi1_6.h>
//#include <d3dcompiler.h>
//#include <DirectXMath.h>
//
//// D3D12 extension library.
//#include "d3dx12.h"
//
//// STL Headers
//#include <algorithm>
//#include <cassert>
//#include <chrono>
//
//// Helper functions
//#include "Helpers.h"
//#include <string>
//#include <vector>
//#include <fstream>
//#include "GameTimer.h"
////The number of swap chain back buffers
//const uint8_t g_NumFrames = 3;
////Use WARP adapter
//bool g_UseWarp = true;
//
//
//uint32_t g_ClientWidth = 1280;
//uint32_t g_ClientHeight = 720;
//
//// Set to true once the DX12 objects have been initialized.
//bool g_IsInitialized = false;
//
////Window handle
//HWND g_hWnd;
////Window rectangle
//RECT g_WindowRect;
//
//ComPtr<ID3D12Device2> g_Device;
//ComPtr<ID3D12CommandQueue> g_CommandQueue;
//ComPtr<IDXGISwapChain4> g_SwapChain;
//ComPtr<ID3D12Resource> g_BackBuffers[g_NumFrames];
//ComPtr<ID3D12GraphicsCommandList> g_CommandList;
//ComPtr<ID3D12CommandAllocator> g_CommandAllocator[g_NumFrames];
//ComPtr<ID3D12DescriptorHeap> g_RTVDescriptorHeap;
//UINT g_RTVDescriptorSize;
//UINT g_CurrentBackBufferIndex;
//
////GPU synchronization
//ComPtr<ID3D12Fence> g_Fence;
//uint64_t g_FenceValue = 0;
//uint64_t g_FrameFenceValues[g_NumFrames] = {};
//HANDLE	g_FenceEvent;
//
//
//GameTimer g_Timer;
//
//
////Swap chain's present method
//bool g_VSync = true;
//bool g_TearingSupported = false;
//bool g_FullScreen = false;
//bool g_IsPaused = false;
//
//
//
////Window callback function
//LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
//
//
//void ParseCommandLineArguments() {
//	int argc;
//	wchar_t** argv = ::CommandLineToArgvW(::GetCommandLineW(), &argc);
//	for (size_t i = 0; i < argc; ++i)
//	{
//		if (::wcscmp(argv[i], L"-w") == 0 || wcscmp(argv[i], L"--width") == 0)
//		{
//			g_ClientWidth = ::wcstol(argv[++i], nullptr, 10);
//		}
//		if (::wcscmp(argv[i], L"-h") == 0 || ::wcscmp(argv[i], L"--height") == 0)
//		{
//			g_ClientHeight = ::wcstol(argv[++i], nullptr, 10);
//		}
//		if (::wcscmp(argv[i], L"-warp") == 0 || ::wcscmp(argv[i], L"--warp") == 0)
//		{
//			g_UseWarp = true;
//		}
//		if (::wcscmp(argv[i], L"-fs") == 0 || ::wcscmp(argv[i], L"--fullscreen") == 0)
//		{
//			g_FullScreen = true;
//		}
//		::LocalFree(argv);
//	}
//}
//
//void EnableDebugLayer()
//{
//#if defined(_DEBUG)
//	ComPtr<ID3D12Debug> debugInterface;
//	ThrowifFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debugInterface)));
//	debugInterface->EnableDebugLayer();
//#endif
//}
//
//void RegisterWindowClass(HINSTANCE hInst, const wchar_t* windowClassName)
//{
//	WNDCLASSEXW windowClass = {};
//
//	windowClass.cbSize = sizeof(WNDCLASSEXW);
//	windowClass.style = CS_HREDRAW | CS_VREDRAW;
//	windowClass.lpfnWndProc = &WndProc;
//	windowClass.cbClsExtra = 0;
//	windowClass.cbWndExtra = 0;
//	windowClass.hInstance = hInst;
//	windowClass.hIcon = ::LoadIcon(hInst, NULL);
//	windowClass.hCursor = ::LoadCursor(NULL, IDC_ARROW);
//	windowClass.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
//	windowClass.lpszClassName = windowClassName;
//	windowClass.lpszMenuName = NULL;
//	windowClass.hIconSm = ::LoadIcon(hInst, NULL);
//
//	static ATOM atom = ::RegisterClassExW(&windowClass);
//	assert(atom > 0);
//	char* log = "Windowclass registration pass successfuly";
//	char buffer[100];
//	sprintf_s(buffer, 100, log);
//	OutputDebugStringA(buffer);
//}
//
//HWND CreateWindow(const wchar_t* windowClassName, HINSTANCE hInst, const wchar_t* windowTitle, uint32_t width, uint32_t height)
//{
//	int screenWidth = ::GetSystemMetrics(SM_CXSCREEN);
//	int screenHeight = ::GetSystemMetrics(SM_CYSCREEN);
//
//	RECT windowRect = { 0,0,static_cast<LONG>(width),static_cast<LONG>(height) };
//	::AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);
//
//	int windowWidth = windowRect.right - windowRect.left;
//	int windowHeight = windowRect.bottom - windowRect.top;
//
//	// Center the window within the screen. Clamp to 0, 0 for the top-left corner.
//	int windowX = std::max<int>(0, (screenWidth - windowWidth) / 2);
//	int windowY = std::max<int>(0, (screenHeight - windowHeight) / 2);
//
//	HWND hWnd = ::CreateWindowExW(
//		NULL,
//		windowClassName,
//		windowTitle,
//		WS_OVERLAPPEDWINDOW,
//		windowX,
//		windowY,
//		windowWidth,
//		windowHeight,
//		NULL,
//		NULL,
//		hInst,
//		nullptr
//		);
//	return hWnd;
//}
//
//ComPtr<IDXGIAdapter4> GetAdapter(bool useWarp)
//{
//	ComPtr<IDXGIFactory4> dxgiFactory;
//	UINT createFactoryFlags = 0;
//#if defined(_DEBUG)
//	createFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
//#endif
//	ThrowifFailed(CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(&dxgiFactory)));
//
//	ComPtr<IDXGIAdapter1> dxgiAdapter1;
//	ComPtr<IDXGIAdapter4> dxgiAdapter4;
//	if (useWarp)
//	{
//		ThrowifFailed(dxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(&dxgiAdapter1)));
//		ThrowifFailed(dxgiAdapter1.As(&dxgiAdapter4));
//	}
//	else
//	{
//		SIZE_T maxDedicatedVideoMemory = 0;
//		for (UINT i = 0; dxgiFactory->EnumAdapters1(i, &dxgiAdapter1) != DXGI_ERROR_NOT_FOUND; ++i)
//		{
//			DXGI_ADAPTER_DESC1 dxgiAdapterDesc1;
//			dxgiAdapter1->GetDesc1(&dxgiAdapterDesc1);
//
//			// Check to see if the adapter can create a D3D12 device without actually 
//			// creating it. The adapter with the largest dedicated video memory
//			// is favored.
//			if ((dxgiAdapterDesc1.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0 &&
//				SUCCEEDED(D3D12CreateDevice(dxgiAdapter1.Get(),
//					D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), nullptr)) &&
//				dxgiAdapterDesc1.DedicatedVideoMemory > maxDedicatedVideoMemory)
//			{
//				maxDedicatedVideoMemory = dxgiAdapterDesc1.DedicatedVideoMemory;
//				ThrowifFailed(dxgiAdapter1.As(&dxgiAdapter4));
//			}
//		}
//	}
//	return dxgiAdapter4;
//
//}
//
//ComPtr<ID3D12Device2> CreateDevice(ComPtr<IDXGIAdapter4> adapter4)
//{
//	ComPtr<ID3D12Device2> d3d12Device2;
//	ThrowifFailed(D3D12CreateDevice(adapter4.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&d3d12Device2)));
//
//#if defined(_DEBUG)
//	ComPtr<ID3D12InfoQueue> pInfoQueue;
//	if (SUCCEEDED(d3d12Device2.As(&pInfoQueue)))
//	{
//		pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
//		pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE);
//		pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
//
//		// Suppress whole categories of messages
//		//D3D12_MESSAGE_CATEGORY Categories[] = {};
//
//		// Suppress messages based on their severity level
//		D3D12_MESSAGE_SEVERITY Severities[] =
//		{
//			D3D12_MESSAGE_SEVERITY_INFO
//		};
//
//		// Suppress individual messages by their ID
//		D3D12_MESSAGE_ID DenyIds[] = {
//			D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,   // I'm really not sure how to avoid this message.
//			D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE,                         // This warning occurs when using capture frame while graphics debugging.
//			D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE,                       // This warning occurs when using capture frame while graphics debugging.
//		};
//
//		D3D12_INFO_QUEUE_FILTER NewFilter = {};
//		//NewFilter.DenyList.NumCategories = _countof(Categories);
//		//NewFilter.DenyList.pCategoryList = Categories;
//		NewFilter.DenyList.NumSeverities = _countof(Severities);
//		NewFilter.DenyList.pSeverityList = Severities;
//		NewFilter.DenyList.NumIDs = _countof(DenyIds);
//		NewFilter.DenyList.pIDList = DenyIds;
//
//		ThrowifFailed(pInfoQueue->PushStorageFilter(&NewFilter));
//	}
//#endif
//
//	return d3d12Device2;
//}
//
//ComPtr<ID3D12CommandQueue> CreateCommandQueue(ComPtr<ID3D12Device2> device, D3D12_COMMAND_LIST_TYPE type)
//{
//	ComPtr<ID3D12CommandQueue> d3d12CommandQueue;
//	D3D12_COMMAND_QUEUE_DESC desc = {  };
//	desc.Type =		type;
//	desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
//	desc.Flags =	D3D12_COMMAND_QUEUE_FLAG_NONE;
//	desc.NodeMask = 0;
//	ThrowifFailed(device->CreateCommandQueue(&desc, IID_PPV_ARGS(&d3d12CommandQueue)));
//
//	return d3d12CommandQueue;
//}
//
//bool CheckTearingSupport()
//{
//	BOOL allowTearing = FALSE;
//
//	ComPtr<IDXGIFactory4> factory4;
//	if (SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory4))))
//	{
//		ComPtr<IDXGIFactory5> factory5;
//		if (SUCCEEDED(factory4.As(&factory5)))
//		{
//			if (FAILED(factory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allowTearing, sizeof(allowTearing))))
//			{
//				allowTearing = FALSE;
//			}
//		}
//	}
//
//	return allowTearing == TRUE;
//}
//
//ComPtr<IDXGISwapChain4> CreateSwapChain(HWND hWnd, ComPtr<ID3D12CommandQueue> commandQueue, uint32_t width, uint32_t height, uint32_t bufferCount)
//{
//	ComPtr<IDXGISwapChain4> dxgiSwapChain4;
//	ComPtr<IDXGIFactory4> dxgiFactory4;
//	UINT createFactoryFlags = 0;
//#if defined(_DEBUG)
//	createFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
//#endif
//
//	ThrowifFailed(CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(&dxgiFactory4)));
//
//	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
//	swapChainDesc.Width = width;
//	swapChainDesc.Height = height;
//	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
//	swapChainDesc.Stereo = FALSE;
//	swapChainDesc.SampleDesc = { 1, 0 };
//	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
//	swapChainDesc.BufferCount = bufferCount;
//	swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
//	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
//	swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
//	// It is recommended to always allow tearing if tearing support is available.
//	swapChainDesc.Flags = CheckTearingSupport() ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;
//
//	ComPtr<IDXGISwapChain1> swapChain1;
//	ThrowifFailed(dxgiFactory4->CreateSwapChainForHwnd(commandQueue.Get(), hWnd, &swapChainDesc, nullptr, nullptr, &swapChain1));
//
//	//Disabling Alt + Enter to make fullscreen
//	ThrowifFailed(dxgiFactory4->MakeWindowAssociation(hWnd, DXGI_MWA_NO_ALT_ENTER));
//
//	ThrowifFailed(swapChain1.As(&dxgiSwapChain4));
//
//	return dxgiSwapChain4;
//}
//
//ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(ComPtr<ID3D12Device2> device, D3D12_DESCRIPTOR_HEAP_TYPE type,uint32_t numDescriptors)
//{
//	ComPtr<ID3D12DescriptorHeap> descriptorHeap;
//
//	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
//	desc.NumDescriptors = numDescriptors;
//	desc.Type = type;
//
//	ThrowifFailed(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&descriptorHeap)));
//	return descriptorHeap;
//
//}
//
//void UpdateRenderTargetView(ComPtr<ID3D12Device> device, ComPtr<IDXGISwapChain4> swapChain, ComPtr<ID3D12DescriptorHeap> descriptorHeap)
//{
//	auto rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
//
//	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(descriptorHeap->GetCPUDescriptorHandleForHeapStart());
//
//	for (int i = 0; i < g_NumFrames; i++)
//	{
//		ComPtr<ID3D12Resource> backBuffer;
//		ThrowifFailed(swapChain->GetBuffer(i, IID_PPV_ARGS(&backBuffer)));
//
//		device->CreateRenderTargetView(backBuffer.Get(), nullptr, rtvHandle);
//
//		g_BackBuffers[i] = backBuffer;
//
//		rtvHandle.Offset(rtvDescriptorSize);
//	}
//
//}
//
//ComPtr<ID3D12CommandAllocator> CreateCommandAllocator(ComPtr<ID3D12Device2> device, D3D12_COMMAND_LIST_TYPE type)
//{
//	ComPtr<ID3D12CommandAllocator> commandAllocator;
//
//	ThrowifFailed(device->CreateCommandAllocator(type, IID_PPV_ARGS(&commandAllocator)));
//	
//
//	return commandAllocator;
//}
//
//ComPtr<ID3D12GraphicsCommandList> CreateCommandList(ComPtr<ID3D12Device2> device, ComPtr<ID3D12CommandAllocator> commandAllocator, D3D12_COMMAND_LIST_TYPE type)
//{
//	ComPtr<ID3D12GraphicsCommandList> commandList;
//	ThrowifFailed(device->CreateCommandList(0, type, commandAllocator.Get(), nullptr, IID_PPV_ARGS(&commandList)));
//
//
//	ThrowifFailed(commandList->Close());
//	return commandList;
//}
//
//ComPtr<ID3D12Fence> CreateFence(ComPtr<ID3D12Device2> device)
//{
//	ComPtr<ID3D12Fence> fence;
//
//	ThrowifFailed(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));
//
//	return fence;
//}
//
//HANDLE CreateEventHandle()
//{
//	HANDLE fenceEvent;
//	fenceEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);
//	assert(fenceEvent && "Failed to create fence event");
//
//	return fenceEvent;
//
//}
//
//uint64_t Signal(ComPtr<ID3D12CommandQueue> commanQueue, ComPtr<ID3D12Fence> fence, uint64_t& fenceValue)
//{
//	uint64_t fenceValueForSignal = ++fenceValue;
//	ThrowifFailed(commanQueue->Signal(fence.Get(), fenceValueForSignal));
//
//	return fenceValueForSignal;
//}
//
//void WaitForFenceValue(ComPtr<ID3D12Fence> fence, uint64_t& fenceValue, HANDLE fenceEvent, std::chrono::milliseconds duration = std::chrono::milliseconds::max())
//{
//	if (fence->GetCompletedValue() < fenceValue)
//	{
//		ThrowifFailed(fence->SetEventOnCompletion(fenceValue, fenceEvent));
//		::WaitForSingleObject(fenceEvent, static_cast<DWORD>(duration.count()));
//	}
//}
//
//void Flush(ComPtr<ID3D12CommandQueue> commandQueue, ComPtr<ID3D12Fence> fence, uint64_t& fenceValue, HANDLE fenceEvent)
//{
//	uint64_t fenceValueForSignal = Signal(commandQueue, fence, fenceValue);
//	WaitForFenceValue(fence, fenceValueForSignal, fenceEvent);
//}
//
//
//void CalculateFrameStatics()
//{
//	static int frameCount = 0;
//	static float elapsedTime = 0.0;
//	
//	frameCount++;
//	if ((g_Timer.GetTotalTime() - elapsedTime) >= 1.0f)
//	{
//		float fps = frameCount;
//		float framePeriod = (1 / fps) * 1000;
//
//		std::wstring text;
//		text += L"FPS: " + std::to_wstring(fps) + L"\n";
//		OutputDebugString(text.c_str());
//		text = L"";
//		text += L"FramePeriod: " + std::to_wstring(framePeriod) + L"\n";
//		OutputDebugString(text.c_str());
//
//		frameCount = 0;
//		elapsedTime += 1.0;
//		
//	}
//
//}
//
//void Update()
//{
//	CalculateFrameStatics();
//	/*static uint64_t frameCounter = 0;
//	static double elapsedSecond = 0.0;
//	static std::chrono::high_resolution_clock clock;
//	static auto t0 = clock.now();
//
//	frameCounter++;
//	auto t1 = clock.now();
//	auto deltaTime = t1 - t0;
//	t0 = t1;
//
//	elapsedSecond += deltaTime.count() * 1e-9;
//	if (elapsedSecond > 1.0)
//	{
//		char buffer[50];
//		auto fps = frameCounter / elapsedSecond;
//		sprintf_s(buffer, 50, "FPS: %f\n", fps);
//		OutputDebugStringA(buffer);
//
//		frameCounter = 0;
//		elapsedSecond = 0.0;
//	}*/
//}
//
//void Render()
//{
//	auto commandAllocator = g_CommandAllocator[g_CurrentBackBufferIndex];
//	auto backBuffer = g_BackBuffers[g_CurrentBackBufferIndex];
//
//	commandAllocator->Reset();
//	g_CommandList->Reset(commandAllocator.Get(), nullptr);
//
//
//	{
//		CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(backBuffer.Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
//		g_CommandList->ResourceBarrier(1, &barrier);
//
//		FLOAT clearColor[] = { 0.4f,0.6f,0.9f,1.0f };
//		CD3DX12_CPU_DESCRIPTOR_HANDLE rtv(g_RTVDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), g_CurrentBackBufferIndex, g_RTVDescriptorSize);
//
//		g_CommandList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
//
//	}
//
//	{
//		CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(backBuffer.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
//		g_CommandList->ResourceBarrier(1, &barrier);
//
//		ThrowifFailed(g_CommandList->Close());
//
//		ID3D12CommandList* const commandLists[] = { g_CommandList.Get() };
//
//		g_CommandQueue->ExecuteCommandLists(_countof(commandLists), commandLists);
//
//		UINT syncInterval = g_VSync ? 1 : 0;
//		UINT presentFlags = g_TearingSupported && !g_VSync ? DXGI_PRESENT_ALLOW_TEARING : 0;
//		ThrowifFailed(g_SwapChain->Present(syncInterval, presentFlags));
//
//		g_FrameFenceValues[g_CurrentBackBufferIndex] = Signal(g_CommandQueue, g_Fence, g_FenceValue);
//
//
//		g_CurrentBackBufferIndex = g_SwapChain->GetCurrentBackBufferIndex();
//		WaitForFenceValue(g_Fence, g_FrameFenceValues[g_CurrentBackBufferIndex], g_FenceEvent);
//	}
//
//}
//
//void Resize(uint32_t width, uint32_t height)
//{
//	if (g_ClientWidth != width || g_ClientHeight != height)
//	{
//		g_ClientWidth = std::max(1u, width);
//		g_ClientHeight = std::max(1u, height);
//
//		Flush(g_CommandQueue, g_Fence, g_FenceValue, g_FenceEvent);
//
//		for (int i =0;i<g_NumFrames;i++)
//		{
//			g_BackBuffers[i].Reset();
//			g_FrameFenceValues[i] = g_FrameFenceValues[g_CurrentBackBufferIndex];
//		}
//		DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
//		ThrowifFailed(g_SwapChain->GetDesc(&swapChainDesc));
//		ThrowifFailed(g_SwapChain->ResizeBuffers(g_NumFrames, g_ClientWidth, g_ClientHeight,
//			swapChainDesc.BufferDesc.Format, swapChainDesc.Flags));
//
//		g_CurrentBackBufferIndex = g_SwapChain->GetCurrentBackBufferIndex();
//
//		UpdateRenderTargetView(g_Device, g_SwapChain, g_RTVDescriptorHeap);
//	}
//}
//
//void SetFullscreen(bool isFullscreen)
//{
//	if (g_FullScreen != isFullscreen) {
//		g_FullScreen = isFullscreen;
//
//		if (g_FullScreen) {
//			::GetWindowRect(g_hWnd, &g_WindowRect);
//
//			// Set the window style to a borderless window so the client area fills
//			// the entire screen.
//			UINT windowStyle = WS_OVERLAPPEDWINDOW & ~(WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX);
//
//			::SetWindowLongW(g_hWnd, GWL_STYLE, windowStyle);
//
//			// Query the name of the nearest display device for the window.
//			// This is required to set the fullscreen dimensions of the window
//			// when using a multi-monitor setup.
//			HMONITOR hMonitor = ::MonitorFromWindow(g_hWnd, MONITOR_DEFAULTTONEAREST);
//			MONITORINFOEX monitorInfo = {};
//			monitorInfo.cbSize = sizeof(MONITORINFOEX);
//			::GetMonitorInfo(hMonitor, &monitorInfo);
//
//			::SetWindowPos(g_hWnd, HWND_TOPMOST,
//				monitorInfo.rcMonitor.left,
//				monitorInfo.rcMonitor.top,
//				monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left,
//				monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top,
//				SWP_FRAMECHANGED | SWP_NOACTIVATE);
//
//			::ShowWindow(g_hWnd, SW_MAXIMIZE);
//		}
//		else {
//			// Restore all the window decorators.
//			::SetWindowLong(g_hWnd, GWL_STYLE, WS_OVERLAPPEDWINDOW);
//
//			::SetWindowPos(g_hWnd, HWND_NOTOPMOST,
//				g_WindowRect.left,
//				g_WindowRect.top,
//				g_WindowRect.right - g_WindowRect.left,
//				g_WindowRect.bottom - g_WindowRect.top,
//				SWP_FRAMECHANGED | SWP_NOACTIVATE);
//
//			::ShowWindow(g_hWnd, SW_NORMAL);
//		}
//	}
//}
//
//void LogOutputsDisplayModes(IDXGIOutput* output, DXGI_FORMAT format, std::string name) 
//{
//	UINT count = 0;
//	UINT flags = 0;
//
//	//Give nullptr to get list count
//	output->GetDisplayModeList(format, flags, &count, nullptr);
//
//	std::vector<DXGI_MODE_DESC> modeList(count);
//	output->GetDisplayModeList(format, flags, &count, &modeList[0]);
//
//	std::ofstream file;
//	for (auto& x : modeList) {
//		UINT n = x.RefreshRate.Numerator;
//		UINT d = x.RefreshRate.Denominator;
//
//		std::wstring text = L"Width = " + std::to_wstring(x.Width) + L"\n";
//		text += L"Height = " + std::to_wstring(x.Height) + L"\n";
//		text += L"Refresh = " + std::to_wstring(n) + L"/" + std::to_wstring(d) + L"\n";
//
//		OutputDebugString(text.c_str());
//
//
//		std::string s(text.begin(), text.end());
//		file.open(name, std::ios::out | std::ios::app);
//		file << s;
//		file.close();
//	}
//}
//
//void LogAdaptersOutput(IDXGIAdapter1* adapter)
//{
//	IDXGIOutput* output = nullptr;
//	std::ofstream file;
//	for (size_t i = 0; adapter->EnumOutputs(i, &output) != DXGI_ERROR_NOT_FOUND; i++)
//	{
//		DXGI_OUTPUT_DESC desc;
//		output->GetDesc(&desc);
//		std::wstring text = L"***Outputs : \n";
//		text += desc.DeviceName;
//		text += L"\n";
//
//		OutputDebugString(text.c_str());
//
//		DXGI_ADAPTER_DESC adapterDesc;
//		adapter->GetDesc(&adapterDesc);
//		std::string s(text.begin(), text.end());
//		file.open(adapterDesc.Description, std::ios::out | std::ios::app);
//		file << s;
//		file.close();
//
//		
//		std::wstring ws(adapterDesc.Description);
//		std::string name(ws.begin(),ws.end());
//
//		LogOutputsDisplayModes(output, DXGI_FORMAT_R8G8B8A8_UNORM, name);
//
//		ReleaseCom(output);
//	}
//}
//
//void LogAdapters()
//{
//	ComPtr<IDXGIFactory4> dxgiFactory;
//
//	IDXGIAdapter1* adapter = nullptr;
//
//	std::vector<IDXGIAdapter1*> adapterList;
//
//	std::ofstream file;
//	file.open("AdaptersLog.txt");
//	file.clear();
//	file.close();
//	ThrowifFailed(CreateDXGIFactory2(0, IID_PPV_ARGS(&dxgiFactory)));
//
//	for (size_t i = 0; dxgiFactory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; i++)
//	{
//		DXGI_ADAPTER_DESC desc;
//		adapter->GetDesc(&desc);
//		std::wstring text = L"***Adapters : \n";
//		text += desc.Description;
//		text += L"\n";
//		OutputDebugString(text.c_str());
//
//		std::string s(text.begin(), text.end());
//		file.open("AdaptersLog.txt", std::ios::out | std::ios::app);
//		file << s;
//		file.close();
//
//		adapterList.push_back(adapter);
//
//	}
//	for (size_t i = 0; i < adapterList.size(); i++)
//	{
//		DXGI_ADAPTER_DESC desc;
//		adapterList[i]->GetDesc(&desc);
//		file.open(desc.Description);
//		file.clear();
//		file.close();
//		LogAdaptersOutput(adapterList[i]);
//		ReleaseCom(adapterList[i]);
//	}
//}
//
//
//
//
//
//LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
//{
//	if (g_IsInitialized)
//	{
//		switch (message)
//		{
//		case WM_PAINT:
//			Update();
//			Render();
//			break;
//		case WM_SYSKEYDOWN:
//		case WM_KEYDOWN:
//		{
//			bool alt = (::GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
//
//			switch (wParam)
//			{
//			case 'V':
//				g_VSync = !g_VSync;
//				break;
//			case VK_ESCAPE:
//				::PostQuitMessage(0);
//				break;
//			case VK_RETURN:
//				if (alt)
//				{
//			case VK_F11:
//				SetFullscreen(!g_FullScreen);
//				}
//				break;
//			case 'P':
//				g_IsPaused = !g_IsPaused;
//				if(g_IsPaused)
//					g_Timer.Stop();
//				else
//				{
//					g_Timer.Start();
//				}
//			}
//		}
//		break;
//		// The default window procedure will play a system notification sound 
//		// when pressing the Alt+Enter keyboard combination if this message is 
//		// not handled.
//		case WM_SYSCHAR:
//			break;
//		case WM_SIZE:
//		{
//			RECT clientRect = {};
//			
//			::GetClientRect(g_hWnd, &clientRect);
//
//			int width = clientRect.right - clientRect.left;
//			int height = clientRect.bottom - clientRect.top;
//
//			Resize(width, height);
//		}
//		break;
//		case WM_DESTROY:
//			::PostQuitMessage(0);
//			break;
//		default:
//			return ::DefWindowProcW(hwnd, message, wParam, lParam);
//		}
//	}
//	else
//	{
//		return ::DefWindowProcW(hwnd, message, wParam, lParam);
//	}
//
//	return 0;
//}
//
//
//int CALLBACK wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ PWSTR pCmdLine, _In_ int nShowCmd)
//{
//	// Windows 10 Creators update adds Per Monitor V2 DPI awareness context.
//	// Using this awareness context allows the client area of the window 
//	// to achieve 100% scaling while still allowing non-client window content to 
//	// be rendered in a DPI sensitive fashion.
//
//	LogAdapters();
//
//	SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
//
//	// Window class name. Used for registering / creating the window.
//	const wchar_t* windowClassName = L"DX12WindowClass";
//	ParseCommandLineArguments();
//
//	EnableDebugLayer();
//	
//
//	g_TearingSupported = CheckTearingSupport();
//
//	RegisterWindowClass(hInstance, windowClassName);
//	g_hWnd = CreateWindow(windowClassName, hInstance, L"Learning DirectX 12",
//		g_ClientWidth, g_ClientHeight);
//
//	// Initialize the global window rect variable.
//	::GetWindowRect(g_hWnd, &g_WindowRect);
//
//	ComPtr<IDXGIAdapter4> dxgiAdapter4 = GetAdapter(g_UseWarp);
//
//	g_Device = CreateDevice(dxgiAdapter4);
//
//	g_CommandQueue = CreateCommandQueue(g_Device, D3D12_COMMAND_LIST_TYPE_DIRECT);
//
//	g_SwapChain = CreateSwapChain(g_hWnd, g_CommandQueue,
//		g_ClientWidth, g_ClientHeight, g_NumFrames);
//
//	g_CurrentBackBufferIndex = g_SwapChain->GetCurrentBackBufferIndex();
//
//	g_RTVDescriptorHeap = CreateDescriptorHeap(g_Device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, g_NumFrames);
//	g_RTVDescriptorSize = g_Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
//
//	UpdateRenderTargetView(g_Device, g_SwapChain, g_RTVDescriptorHeap);
//
//
//	for (int i = 0; i < g_NumFrames; ++i)
//	{
//		g_CommandAllocator[i] = CreateCommandAllocator(g_Device, D3D12_COMMAND_LIST_TYPE_DIRECT);
//	}
//	g_CommandList = CreateCommandList(g_Device,
//		g_CommandAllocator[g_CurrentBackBufferIndex], D3D12_COMMAND_LIST_TYPE_DIRECT);
//
//	g_Fence = CreateFence(g_Device);
//	g_FenceEvent = CreateEventHandle();
//
//
//	g_IsInitialized = true;
//
//	::ShowWindow(g_hWnd, SW_SHOW);
//
//	g_Timer.Reset();
//	MSG msg = {};
//	while (msg.message != WM_QUIT)
//	{
//		
//		if (::PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
//		{
//			::TranslateMessage(&msg);
//			::DispatchMessage(&msg);
//		}
//		g_Timer.Tick();
//		//std::wstring text = L"***TotalTime : \n";
//		//text += std::to_wstring(g_Timer.GetTotalTime());
//		//text += L"\n";
//
//		//OutputDebugString(text.c_str());
//
//	}
//
//	// Make sure the command queue has finished all commands before closing.
//	Flush(g_CommandQueue, g_Fence, g_FenceValue, g_FenceEvent);
//
//	::CloseHandle(g_FenceEvent);
//
//	//_CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_DEBUG);
//	//_CrtDumpMemoryLeaks();
//
//	return 0;
//}
//
//
//
//// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
//// Debug program: F5 or Debug > Start Debugging menu
//
//// Tips for Getting Started: 
////   1. Use the Solution Explorer window to add/manage files
////   2. Use the Team Explorer window to connect to source control
////   3. Use the Output window to see build output and other messages
////   4. Use the Error List window to view errors
////   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
////   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <Shlwapi.h>
#include<PathCch.h>
#define _ENABLE_EXTENDED_ALIGNED_STORAGE

#include"Tutorial2.h"

#include "Application.h"
#include"Game.h"
#include <dxgidebug.h>
#include  <initguid.h> 

DEFINE_GUID(DXGI_DEBUG_ALL, 0xe48ae283, 0xda80, 0x490b, 0x87, 0xe6, 0x43, 0xe9, 0xa9, 0xcf, 0xda, 0x8);

void ReportLiveObjects()
{
	IDXGIDebug1* dxgiDebug;
	DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiDebug));

	dxgiDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_IGNORE_INTERNAL);
	dxgiDebug->Release();
}

int CALLBACK wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR lpCmdLine, int nCmdShow)
{
	int retCode = 0;

	// Set the working directory to the path of the executable.
	WCHAR path[MAX_PATH];
	HMODULE hModule = GetModuleHandleW(NULL);
	if (GetModuleFileNameW(hModule, path, MAX_PATH) > 0)
	{
		PathCchRemoveFileSpec(path, MAX_PATH);
		SetCurrentDirectoryW(path);
	}


	Application::Create(hInstance);
	{
		std::shared_ptr<Tutorial2> demo = std::make_shared<Tutorial2>(L"Learning DirectX 12 - Lesson 2", 1280, 720, false);
		retCode = Application::Get().Run(demo);
	}
	Application::Destroy();

	atexit(&ReportLiveObjects);

	return retCode;
}