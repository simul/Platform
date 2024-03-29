﻿#include <stdlib.h>
#include <stdio.h>
#include <string>

#ifdef _MSC_VER
#include <windows.h>
#endif

#include "Platform/Core/RuntimeError.h"
#include "Platform/Vulkan/Effect.h"
#include "Platform/Vulkan/RenderPlatform.h"
#include "Platform/Vulkan/Framebuffer.h"
#include "Platform/CrossPlatform/Texture.h"
#include "Platform/Core/DefaultFileLoader.h"
#include "Platform/Core/StringFunctions.h"
#include "Platform/Core/Timer.h"
#include "Platform/Core/CommandLine.h"
#include "Platform/Core/EnvironmentVariables.h"

#include <filesystem>
using namespace std::literals;
using namespace std::string_literals;
using namespace std::literals::string_literals;
using namespace platform;
using namespace vulkan;

bool RewriteOutput(std::string str)
{
	std::cerr<<str.c_str();
	return true;
}

Query::Query(crossplatform::QueryType t)
	: crossplatform::Query(t)
{
	mQueryPoolCI.pNext = nullptr;
	mQueryPoolCI.flags = (vk::QueryPoolCreateFlags)0;
	mQueryPoolCI.queryType = ToVkQueryType(t);
	mQueryPoolCI.queryCount = crossplatform::Query::QueryLatency;
	mQueryPoolCI.pipelineStatistics = (vk::QueryPipelineStatisticFlags)0;
}

Query::~Query()
{
	InvalidateDeviceObjects();
}

void Query::RestoreDeviceObjects(crossplatform::RenderPlatform* r)
{
	InvalidateDeviceObjects();
	renderPlatform = r;
	if (renderPlatform)
	{
		mDevice = ((vulkan::RenderPlatform *)renderPlatform)->AsVulkanDevice();
	}
	if (!mDevice)
	{
		SIMUL_BREAK_ONCE("No valid Vulkan device available to create QueryPool.");
		return;
	}
	mQueryPool = mDevice->createQueryPool(mQueryPoolCI);
}

void Query::InvalidateDeviceObjects() 
{
	if (mDevice && mQueryPool)
	{
		mDevice->destroyQueryPool(mQueryPool,nullptr);
		*(VkQueryPool*)&mQueryPool = VK_NULL_HANDLE;
	}

	for (int i = 0; i < QueryLatency; i++)
	{
		gotResults[i] = true;
		doneQuery[i] = false;
	}
}

vk::QueryType Query::ToVkQueryType(crossplatform::QueryType t)
{
	switch (t)
	{
		case crossplatform::QUERY_OCCLUSION:
			return vk::QueryType::eOcclusion;
		case crossplatform::QUERY_TIMESTAMP_DISJOINT:
		case crossplatform::QUERY_TIMESTAMP:
			return vk::QueryType::eTimestamp;
		case crossplatform::QUERY_UNKNOWN:
		default:
			return (vk::QueryType)0;
	};
}

void Query::ResetQueries(crossplatform::DeviceContext &deviceContext)
{
	deviceContext.renderPlatform->EndRenderPass(deviceContext);

	if (!mQueryPool)
	{
		RestoreDeviceObjects(deviceContext.renderPlatform);
	}

	vk::CommandBuffer *commandBuffer = (vk::CommandBuffer *)deviceContext.platform_context;
	commandBuffer->resetQueryPool(mQueryPool, currFrame, 1);
	mCmdBuffers[currFrame] = commandBuffer;

	gotResults[currFrame] = false;
	doneQuery[currFrame] = true;
}

void Query::Begin(crossplatform::DeviceContext& deviceContext)
{
	ResetQueries(deviceContext);

	vk::CommandBuffer *commandBuffer = (vk::CommandBuffer *)deviceContext.platform_context;
	if (mQueryPoolCI.queryType == vk::QueryType::eTimestamp)
		commandBuffer->writeTimestamp(vk::PipelineStageFlagBits::eAllCommands, mQueryPool, static_cast<uint32_t>(currFrame));
}

void Query::End(crossplatform::DeviceContext& deviceContext)
{
	ResetQueries(deviceContext);

	vk::CommandBuffer *commandBuffer = (vk::CommandBuffer *)deviceContext.platform_context;
	if (mQueryPoolCI.queryType == vk::QueryType::eTimestamp)
		commandBuffer->writeTimestamp(vk::PipelineStageFlagBits::eAllCommands, mQueryPool, static_cast<uint32_t>(currFrame));
}

bool Query::GetData(crossplatform::DeviceContext &deviceContext,void *data, size_t sz)
{
	vk::CommandBuffer *commandBuffer = (vk::CommandBuffer *)deviceContext.platform_context;

	gotResults[currFrame] = true;
	if (!doneQuery[currFrame])
		return false;

	bool result = false;
	int prevFrame = (currFrame - 1 + QueryLatency) % QueryLatency;
	if (mCmdBuffers[prevFrame])
	{
		SIMUL_ASSERT(sizeof(sz) >= sizeof(uint64_t));
		vk::Result ok = mDevice->getQueryPoolResults(mQueryPool, prevFrame, 1, sizeof(uint64_t), data, 0, vk::QueryResultFlagBits::e64);

		*(uint64_t*)data /= 1000000; //convert ns to ms
		result = (ok == vk::Result::eSuccess) && (data != nullptr);
	}

	currFrame = (currFrame + 1) % QueryLatency;
	return result;
}

void Query::SetName(const char *name)
{
	platform::vulkan::SetVulkanName(renderPlatform, mQueryPool, name);
}

Effect::Effect()
{
}

Effect::~Effect()
{
	platform_effect=0;
}

bool Effect::Load(crossplatform::RenderPlatform* r, const char* filename_utf8)
{
		return crossplatform::Effect::Load(r, filename_utf8);
}

EffectTechnique* Effect::CreateTechnique()
{
	return new vulkan::EffectTechnique(renderPlatform,this);
}

crossplatform::EffectTechnique* Effect::GetTechniqueByIndex(int index)
{
    return techniques_by_index[index];
}

void Effect::Reapply(crossplatform::DeviceContext& deviceContext)
{
    crossplatform::ContextState *cs = renderPlatform->GetContextState(deviceContext);
    cs->textureAssignmentMapValid = false;
    cs->rwTextureAssignmentMapValid = false;
    crossplatform::Effect::Reapply(deviceContext);
}

void Effect::Unapply(crossplatform::DeviceContext& deviceContext)
{
    crossplatform::Effect::Unapply(deviceContext);
}

void Effect::UnbindTextures(crossplatform::DeviceContext& deviceContext)
{
    crossplatform::Effect::UnbindTextures(deviceContext);
}

crossplatform::EffectPass* EffectTechnique::AddPass(const char* name, int i)
{
	crossplatform::EffectPass* p    = new vulkan::EffectPass(renderPlatform,effect);
	p->SetName(((this->name+" ")+name).c_str());
	passes_by_name[name]            = passes_by_index[i] = p;
	return p;
}

TexHandlesUBO::TexHandlesUBO()
  //  mId(0)
{
}


TexHandlesUBO::~TexHandlesUBO()
{
    Release();
}

/*
void TexHandlesUBO::Init(size_t count, GLuint program, int index, int slot)
{
    Release();

	size=count;
    mSlot = slot;
}

void TexHandlesUBO::Bind(GLuint program)
{
}

void TexHandlesUBO::Update(GLuint64 value, size_t offset)
{
	if(offset/sizeof(GLuint64)>=size)
		SIMUL_BREAK("");
}
*/
void TexHandlesUBO::Release()
{
 //   if (mId != 0)
    {
  //      mId = 0;
    }
}
