#pragma once

#include<d3d12.h>
#include<dxgi1_6.h>
#include<wrl.h>

#include<memory>
#include<string>

class Window;
class Game;
class CommandQueue;



class Application
{
public:

	/**
	Create the application singelton
	**/
	static void Create(HINSTANCE hInst);
	/**
	Destroy the application instance and all windows created by this instance
	**/
	static void Destroy();
	/**
	Get the application singelton
	**/
	static Application& Get();
	/**
	Check to see if VSync-off is supported
	**/
	bool IsTearingSupported() const;
	/**

	**/
	std::shared_ptr<Window> CreateRenderWindow(std::wstring& windowName, int clientWidth, int clientHeight, bool vSync = true);

	void DestroyWindow(const std::wstring& windowName);
	void DestroyWindow(std::shared_ptr<Window> window);
	std::shared_ptr<Window> GetWindowByName(const std::wstring& windowName);


	int Run(std::shared_ptr<Game> pGame);

	void Quit(int exitCode = 0);

	Microsoft::WRL::ComPtr<ID3D12Device2> GetDevice() const;

	std::shared_ptr<CommandQueue> GetCommandQueue(D3D12_COMMAND_LIST_TYPE type = D3D12_COMMAND_LIST_TYPE_DIRECT) const;

	void Flush();


	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(UINT numDescriptors, D3D12_DESCRIPTOR_HEAP_TYPE type);
	UINT GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE type) const;

protected:

	// Create an application instance.
	Application(HINSTANCE hInst);
	// Destroy the application instance and all windows associated with this application.
	virtual ~Application();

	Microsoft::WRL::ComPtr<IDXGIAdapter4> GetAdapter(bool bUseWarp);
	Microsoft::WRL::ComPtr<ID3D12Device2> CreateDevice(Microsoft::WRL::ComPtr<IDXGIAdapter4> adapter);
	bool CheckTearingSupport();

private:
	Application(const Application& copy) = delete;
	Application& operator=(const Application& other) = delete;

	// The application instance handle that this application was created with.
	HINSTANCE g_hInstance;

	Microsoft::WRL::ComPtr<IDXGIAdapter4> g_dxgiAdapter;
	Microsoft::WRL::ComPtr<ID3D12Device2> g_d3d12Device;

	std::shared_ptr<CommandQueue> g_DirectCommandQueue;
	std::shared_ptr<CommandQueue> g_ComputeCommandQueue;
	std::shared_ptr<CommandQueue> g_CopyCommandQueue;

	bool g_TearingSupported;



};

