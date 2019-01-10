#include "CommandQueue.h"



//----------------------------------------------------------------------------------------------------------------
CommandQueue::CommandQueue(Microsoft::WRL::ComPtr<ID3D12Device2> device, D3D12_COMMAND_LIST_TYPE type)
	:g_FenceValue(0), g_CommanListType(type), g_d3d12Device(device)
{
	D3D12_COMMAND_QUEUE_DESC desc{};
	desc.Type = type;
	desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	desc.NodeMask = 0;
	desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;

	ThrowifFailed(g_d3d12Device->CreateCommandQueue(&desc, IID_PPV_ARGS(&g_d3d12CommandQueue)));
	ThrowifFailed(g_d3d12Device->CreateFence(g_FenceValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&g_d3d12Fence)));

	g_FenceEvent = CreateEvent(NULL, false, false, NULL);
	assert(g_FenceEvent && "Failed to Create Fence Event Handle.");

}
//----------------------------------------------------------------------------------------------------------------
CommandQueue::~CommandQueue()
{
}
//----------------------------------------------------------------------------------------------------------------
Microsoft::WRL::ComPtr<ID3D12CommandAllocator> CommandQueue::CreateCommandAllocator()
{
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator;
	ThrowifFailed(g_d3d12Device->CreateCommandAllocator(g_CommanListType, IID_PPV_ARGS(&commandAllocator)));
	return commandAllocator;
}
//----------------------------------------------------------------------------------------------------------------
Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> CommandQueue::CreateCommandList(Microsoft::WRL::ComPtr<ID3D12CommandAllocator> allocator)
{
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> commandList;
	ThrowifFailed(g_d3d12Device->CreateCommandList(0, g_CommanListType, allocator.Get(), nullptr, IID_PPV_ARGS(&commandList)));
	return commandList;
}
//----------------------------------------------------------------------------------------------------------------
Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> CommandQueue::GetCommandList()
{
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> commandList;

	if (!g_CommandAllocatorQueue.empty() && IsFenceComplete(g_CommandAllocatorQueue.front().fenceValue))
	{
		commandAllocator = g_CommandAllocatorQueue.front().commandAllocator;
		g_CommandAllocatorQueue.pop();

		ThrowifFailed(commandAllocator->Reset());
	}
	else
	{
		commandAllocator = CreateCommandAllocator();
	}

	if (!g_CommandListQueue.empty())
	{
		commandList = g_CommandListQueue.front();
		g_CommandListQueue.pop();

		ThrowifFailed(commandList->Reset(commandAllocator.Get(), nullptr));
	}
	else
	{
		CreateCommandList(commandAllocator);
	}

	//The SetPrivateDataInterface will increment the COM object reference(commandAllocator)
	ThrowifFailed(commandList->SetPrivateDataInterface(__uuidof(ID3D12CommandAllocator), commandAllocator.Get()));

	return commandList;
}
//----------------------------------------------------------------------------------------------------------------
uint64_t CommandQueue::ExecuteCommandList(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> list)
{
	list->Close();

	ID3D12CommandAllocator* commandAllocator;
	UINT dataSize = sizeof(commandAllocator);
	ThrowifFailed(list->GetPrivateData(__uuidof(ID3D12CommandAllocator), &dataSize, commandAllocator));

	ID3D12CommandList* const ppCommandList[] = { list.Get() };

	g_d3d12CommandQueue->ExecuteCommandLists(1, ppCommandList);
	uint64_t fenceValue = Signal();

	g_CommandAllocatorQueue.emplace(CommandAllocatorEntry{ fenceValue, commandAllocator });
	g_CommandListQueue.push(list);

	ReleaseCom(commandAllocator);

	return fenceValue;
}

uint64_t CommandQueue::Signal()
{
	uint64_t fenceValueForSignal = ++g_FenceValue;
	ThrowifFailed(g_d3d12CommandQueue->Signal(g_d3d12Fence.Get(), fenceValueForSignal));

	return fenceValueForSignal;
}

bool CommandQueue::IsFenceComplete(uint64_t fenceValue)
{
	return g_d3d12Fence->GetCompletedValue() >= fenceValue;
}

void CommandQueue::WaitForFenceValue(uint64_t fenceValue)
{
	std::chrono::milliseconds duration = std::chrono::milliseconds::max();
	if (g_d3d12Fence->GetCompletedValue() < fenceValue)
	{
		ThrowifFailed(g_d3d12Fence->SetEventOnCompletion(fenceValue, g_FenceEvent));
		::WaitForSingleObject(g_FenceEvent, static_cast<DWORD>(duration.count()));
	}
}
Microsoft::WRL::ComPtr<ID3D12CommandQueue> CommandQueue::GetD3D12CommandQueue() const
{
	return g_d3d12CommandQueue;
}

void CommandQueue::Flush()
{
	WaitForFenceValue(Signal());
}