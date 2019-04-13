#include "FrameResource.h"

FrameResource::FrameResource(ID3D12Device * device, UINT passCount, UINT objectCount)
{
	ThrowifFailed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(commandAllocator.GetAddressOf())));

	PassCB = std::make_unique<UploadBuffer<PassConstants>>(true, device, passCount);
	ObjectCB = std::make_unique<UploadBuffer<ObjectConstants>>(true, device, objectCount);
	MaterialCB = std::make_unique<UploadBuffer<MaterialConstants>>(true, device, objectCount);

}
FrameResource::~FrameResource()
{

}