#pragma once
#include<d3d12.h>
#include<wrl.h>
#include<dxgi1_6.h>
#include "Helpers.h"
#include<vector>
#include<string>
using namespace Microsoft::WRL;
class d3dApp
{
public:
	d3dApp();
	~d3dApp();


private:
	ComPtr<ID3D12Device> m_Device;


	void LogAdapters();
	void LogAdaptersOutput(ComPtr<IDXGIAdapter>* adapter);
	void CreateDevice();
	ComPtr<IDXGIAdapter4> GetAdapter(bool useWarp);
};

