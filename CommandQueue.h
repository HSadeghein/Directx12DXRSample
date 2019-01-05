#pragma once

#define NOMINMAX
#include <d3d12.h>  // For ID3D12CommandQueue, ID3D12Device2, and ID3D12Fence
#include <wrl.h>    // For Microsoft::WRL::ComPtr

#include <cstdint>  // For uint64_t
#include <queue>    // For std::queue
#include <cassert>
#include<chrono>
#include "Helpers.h"



class CommandQueue
{
public:
	CommandQueue(Microsoft::WRL::ComPtr<ID3D12Device2> device, D3D12_COMMAND_LIST_TYPE type);
	~CommandQueue();


	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> GetCommandList();
	uint64_t ExecuteCommandList(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> list);
	uint64_t Signal();
	bool IsFenceComplete(uint64_t fenceValue);
	void WaitForFenceValue(uint64_t fenceValue);
	void Flush();
protected:
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> CreateCommandAllocator();
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> CreateCommandList(Microsoft::WRL::ComPtr<ID3D12CommandAllocator> allocator);


private:
	struct CommandAllocatorEntry
	{
		uint64_t fenceValue;
		Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator;
	};

	using CommandAllocatorQueue = std::queue<CommandAllocatorEntry>;
	using CommandListQueue = std::queue<Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2>>;

	D3D12_COMMAND_LIST_TYPE						g_CommanListType;
	Microsoft::WRL::ComPtr<ID3D12Device2>		g_d3d12Device;
	Microsoft::WRL::ComPtr<ID3D12CommandQueue>	g_d3d12CommandQueue;
	Microsoft::WRL::ComPtr<ID3D12Fence>			g_d3d12Fence;
	HANDLE										g_FenceEvent;
	uint64_t									g_FenceValue;

	CommandAllocatorQueue						g_CommandAllocatorQueue;
	CommandListQueue							g_CommandListQueue;

};

