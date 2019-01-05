#include "d3dApp.h"



d3dApp::d3dApp()
{
}


d3dApp::~d3dApp()
{
}

void d3dApp::LogAdapters()
{
	/*ComPtr<IDXGIFactory4> dxgiFactory;

	ComPtr<IDXGIAdapter> adapter = nullptr;

	IDXGIAdapter* a = nullptr;

	std::vector<ComPtr<IDXGIAdapter>*> adapterList;

	for (size_t i = 0;dxgiFactory->EnumAdapters(i,&adapter) != DXGI_ERROR_NOT_FOUND; i++)
	{
		DXGI_ADAPTER_DESC desc;
		adapter->GetDesc(&desc);
		std::wstring text = L"***Adapters : \n";
		text += desc.Description;
		text += L"\n";

		OutputDebugString(text.c_str());

		adapterList.push_back(&adapter);
		
	}
	for (size_t i = 0; i < adapterList.size(); i++)
	{
		LogAdaptersOutput(adapterList[i]);
	}*/
}

void d3dApp::LogAdaptersOutput(ComPtr<IDXGIAdapter>* adapter)
{
	/*IDXGIOutput* output = nullptr;
	for (size_t i = 0; i < adapter->Get()->EnumOutputs(i,&output); i++)
	{
		DXGI_OUTPUT_DESC desc;
		output->GetDesc(&desc);
		std::wstring text = L"***Outputs : \n";
		text += desc.DeviceName;
		text += L"\n";

		OutputDebugString(text.c_str());

		
		ReleaseCom(output);
	}*/
}

void d3dApp::CreateDevice()
{
//#if defined(DEBUG) || defined(_DEBUG)
//	ComPtr<ID3D12Debug> debugController;
//	ThrowifFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)));
//	debugController->EnableDebugLayer();
//#endif
//	ComPtr<IDXGIFactory4> dxgiAdapter;
//	dxgiAdapter = GetAdapter(true);
//
//	//Create Device
//	ThrowifFailed(D3D12CreateDevice(dxgiAdapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_Device)));
//}
//
//ComPtr<IDXGIAdapter4> d3dApp::GetAdapter(bool useWarp)
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
}