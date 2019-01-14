#include "Tutorial1.h"
#include"Window.h"
#include"Application.h"
#include"CommandQueue.h"
#include"d3dx12.h"
Tutorial1::Tutorial1(const std::wstring & name, int width, int height, bool vSync) : super(name, width, height, vSync)
{

}


Tutorial1::~Tutorial1()
{
}

bool Tutorial1::LoadContent()
{
	return true;
}

void Tutorial1::UnloadContent()
{
}

void Tutorial1::OnUpdate(UpdateEventArgs & e)
{
	super::OnUpdate(e);
	if(!g_pWindow->IsFullScreen())
		g_pWindow->SetFullScreen(true);
	//g_pWindow->SetVSync(true);
	//static uint64_t frameCount = 0;
	//static double totalTime = 0.0;

	//Game::OnUpdate(e);

	//totalTime += e.ElapsedTime;
	//frameCount++;

	//if (totalTime > 1.0)
	//{
	//	double fps = frameCount / totalTime;

	//	char buffer[512];
	//	sprintf_s(buffer, "FPS: %f\n", fps);
	//	OutputDebugStringA(buffer);

	//	frameCount = 0;
	//	totalTime = 0.0;
	//}
}

void Tutorial1::OnRender(RenderEventArgs& e)
{
	auto commandQueue = Application::Get().GetCommandQueue();
	auto commandList = commandQueue->GetCommandList();

	UINT currentBackBufferIndex = g_pWindow->GetCurrentBackBufferIndex();
	auto backBuffer = g_pWindow->GetCurrentBackBuffer();
	auto rtv = g_pWindow->GetCurrentRenderTargetView();
	//Clear RenderTarget
	{
		CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(backBuffer.Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
		commandList->ResourceBarrier(1, &barrier);

		FLOAT clearColor[] = { 0.4f,0.6f,0.9f,1.0f };
		commandList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);

	}
	{
		CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(backBuffer.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
		commandList->ResourceBarrier(1, &barrier);

		g_FenceValues[currentBackBufferIndex] = commandQueue->ExecuteCommandList(commandList);


		currentBackBufferIndex = g_pWindow->Present();

		commandQueue->WaitForFenceValue(g_FenceValues[currentBackBufferIndex]);
	}

}