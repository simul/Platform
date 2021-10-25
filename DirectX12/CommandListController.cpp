#include "Platform/DirectX12/CommandListController.h"
#include "Platform/Core/RuntimeError.h"
#include "Platform/Core/StringToWString.h"
#include "Platform/Core/StringFunctions.h"
#include "Platform/DirectX12/RenderPlatform.h"
#include "Platform/DirectX12/GpuProfiler.h"
#if SIMUL_D3D12_VIDEO_SUPPORTED
#include <d3d12video.h>
#endif
#include <algorithm>

using namespace simul;
using namespace dx12;

CommandListController::CommandListController()
	: mCommandQueue(nullptr)
	, mAllocators(nullptr)
	, mCommandList(nullptr)
	, mFences(nullptr)
	, mNumAllocators(0)
	, mIndex(0)
	, mCommandQueueOwner(false)
	, mCommandListRecording(false)
	, mWindowEvent(nullptr)
{

}

CommandListController::~CommandListController()
{
	Release();
}

void CommandListController::Initialize(RenderPlatform* renderPlatform, D3D12_COMMAND_LIST_TYPE commandListType, const char* name, ID3D12CommandQueue* commandQueue, uint32_t numAllocators)
{
	ID3D12Device* device = renderPlatform->AsD3D12Device();
	std::string commandName = name;
	if (commandQueue)
	{
		mCommandQueue = commandQueue;
		mCommandQueueOwner = false;
	}
	else
	{
		std::string queueName = commandName + "CommandQueue";
		mCommandQueue = RenderPlatform::CreateCommandQueue(device, commandListType, queueName.c_str());
		mCommandQueueOwner = true;
	}

	mNumAllocators = numAllocators;
	mIndex = 0;

	mAllocators = new ID3D12CommandAllocator*[mNumAllocators];
	mFences = new Fence*[mNumAllocators];

	for (uint32_t i = 0; i < mNumAllocators; ++i)
	{
		V_CHECK(device->CreateCommandAllocator(commandListType, SIMUL_PPV_ARGS(&mAllocators[i])));
		std::string resourceName = commandName + "Allocator" + std::to_string(i);
		mAllocators[i]->SetName(platform::core::StringToWString(resourceName).c_str());

		resourceName = commandName + "Fence" + std::to_string(i);
		mFences[i] = (Fence*)renderPlatform->CreateFence(resourceName.c_str());
	}
	
	V_CHECK(device->CreateCommandList(0, commandListType, mAllocators[0], nullptr, SIMUL_PPV_ARGS(&mCommandList)));
	
	mCommandListRecording = true;

	std::string commandListName = commandName + "CommandList";
	mCommandList->SetName(platform::core::StringToWString(commandListName).c_str());

	mWindowEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
}

void CommandListController::Release()
{
	if (mCommandListRecording)
	{
		//CloseCommandList();
	}
	SAFE_DELETE_ARRAY_MEMBERS(mFences, mNumAllocators);
	SAFE_DELETE_ARRAY(mFences);
	SAFE_RELEASE(mCommandList);
	SAFE_RELEASE_ARRAY(mAllocators, mNumAllocators);
	SAFE_DELETE_ARRAY(mAllocators);
	if (mCommandQueueOwner)
		SAFE_RELEASE(mCommandQueue);
}

void CommandListController::ResetCommandList()
{
	if (mCommandListRecording)
	{
		return;
	}

	// If the GPU is behind, wait:
	if (mFences[mIndex]->AsD3D12Fence()->GetCompletedValue() < mFences[mIndex]->value)
	{
		mFences[mIndex]->AsD3D12Fence()->SetEventOnCompletion(mFences[mIndex]->value, mWindowEvent);
		WaitForSingleObject(mWindowEvent, INFINITE);
	}
	// ExecuteCommandList will Signal this value:
	mFences[mIndex]->value++;

	auto res = mAllocators[mIndex]->Reset();
	SIMUL_ASSERT(res == S_OK);

#if SIMUL_D3D12_VIDEO_SUPPORTED
	if (mCommandList->GetType() == D3D12_COMMAND_LIST_TYPE_VIDEO_DECODE)
		((ID3D12VideoDecodeCommandList*)mCommandList)->Reset(mAllocators[mIndex]);
	else if (mCommandList->GetType() == D3D12_COMMAND_LIST_TYPE_VIDEO_ENCODE)
		((ID3D12VideoEncodeCommandList*)mCommandList)->Reset(mAllocators[mIndex]);
	else if (mCommandList->GetType() == D3D12_COMMAND_LIST_TYPE_VIDEO_PROCESS)
		((ID3D12VideoProcessCommandList*)mCommandList)->Reset(mAllocators[mIndex]);
	else
#endif
		((ID3D12GraphicsCommandList*)mCommandList)->Reset(mAllocators[mIndex], nullptr);
	mCommandListRecording = true;
	
}

void CommandListController::ExecuteCommandList()
{
	if (mCommandListRecording)
	{
		CloseCommandList();
		mCommandQueue->ExecuteCommandLists(1, (ID3D12CommandList* const*)&mCommandList);
		mCommandQueue->Signal(mFences[mIndex]->AsD3D12Fence(), mFences[mIndex]->value);

		mIndex = (mIndex + 1) % mNumAllocators;
	}
}

void CommandListController::CloseCommandList()
{
#if SIMUL_D3D12_VIDEO_SUPPORTED
	if (mCommandList->GetType() == D3D12_COMMAND_LIST_TYPE_VIDEO_DECODE)
		((ID3D12VideoDecodeCommandList*)mCommandList)->Close();
	else if (mCommandList->GetType() == D3D12_COMMAND_LIST_TYPE_VIDEO_ENCODE)
		((ID3D12VideoEncodeCommandList*)mCommandList)->Close();
	else if (mCommandList->GetType() == D3D12_COMMAND_LIST_TYPE_VIDEO_PROCESS)
		((ID3D12VideoProcessCommandList*)mCommandList)->Close();
	else
#endif
		((ID3D12GraphicsCommandList*)mCommandList)->Close();
	mCommandListRecording = false;
	
}