#include "GpuProfiler.h"
#include "Platform/CrossPlatform/Effect.h"
#include "Platform/Core/StringFunctions.h"
#include "Platform/Core/RuntimeError.h"
#include "Platform/Core/StringToWString.h"
#include "Platform/CrossPlatform/RenderPlatform.h"
#include "Platform/CrossPlatform/DeviceContext.h"
#include <sstream>
#include <iostream>
#include <map>
#include <algorithm>

using namespace platform;
using namespace platform;
using namespace crossplatform;
using namespace std;
static std::unordered_map<void*,platform::crossplatform::GpuProfilingInterface*> gpuProfilingInterface;
typedef uint64_t UINT64;
typedef int BOOL;

ProfileData::ProfileData()
				:DisjointQuery(NULL)
				,TimestampStartQuery(NULL)
				,TimestampEndQuery(NULL)
				{
				}

ProfileData::~ProfileData()
				{
					delete DisjointQuery;
					delete TimestampStartQuery;
					delete TimestampEndQuery;
				}

namespace platform
{
	namespace crossplatform
	{
		void SetGpuProfilingInterface(crossplatform::DeviceContext &context,GpuProfilingInterface *p)
		{
			gpuProfilingInterface[context.platform_context]=p;
		}
		GpuProfilingInterface *GetGpuProfilingInterface(crossplatform::DeviceContext &context)
		{
			if(gpuProfilingInterface.empty())
				return nullptr;
			auto u=gpuProfilingInterface.find(context.platform_context);
			if(u==gpuProfilingInterface.end())
				return nullptr;
			return u->second;
		}
		void ClearGpuProfilers()
		{
			for(auto i:gpuProfilingInterface)
			{
				if(i.second)
					i.second->InvalidateDeviceObjects();
			}
			gpuProfilingInterface.clear();
		}
	}
}

GpuProfiler::GpuProfiler()
	:renderPlatform(NULL)
	,enabled(false)
{
}


GpuProfiler::~GpuProfiler()
{
	InvalidateDeviceObjects();
}

void GpuProfiler::RestoreDeviceObjects(crossplatform::RenderPlatform *r)
{
	renderPlatform = r;
    enabled=true;
}

void GpuProfiler::InvalidateDeviceObjects()
{
#ifdef SIMUL_WIN8_SDK
	SAFE_RELEASE(pUserDefinedAnnotation);
#endif
    renderPlatform = NULL;
    enabled=true;
	profileStack.clear();
	BaseProfilingInterface::Clear();
}

#if PLATFORM_DEBUG_PROFILING_LEVELS
std::vector<std::string> profilingStrings;
#endif
void GpuProfiler::Begin(crossplatform::DeviceContext &deviceContext,const char *name)
{
	if (!enabled||!renderPlatform||!root||!frame_active)
		return;

	// We will use event signals irrespective of level, to better track things in external GPU tools.
	renderPlatform->BeginEvent(deviceContext,name);
	level++;
#if PLATFORM_DEBUG_PROFILING_LEVELS
	SIMUL_ASSERT_WARN_ONCE(level>0,"Profiler debugging: level was less than zero calling GpuProfiler::Begin")
	SIMUL_COUT << "Profiler debugging: begin level "<<level<<", "<<name<< std::endl;
	profilingStrings.push_back(name);
#endif
	if(level>max_level)
		return;
	level_in_use++;
	if (level_in_use != level)
	{
		SIMUL_CERR << "Profiler level out of whack! Do you have a mismatched begin/end pair?" << std::endl;
	}
	max_level_this_frame=std::max(max_level_this_frame,level);
	{
		if(!enabled||!renderPlatform)
			return;
	    crossplatform::ProfileData *parentData=NULL;
		if(profileStack.size())
			parentData=(crossplatform::ProfileData*)profileStack.back();
		else
		{
			parentData=(crossplatform::ProfileData*)root;
		}
		crossplatform::ProfileData *profileData = NULL;
		if(parentData->children.find(name)==parentData->children.end())
		{
			profileData=new crossplatform::ProfileData;
			parentData->children[name]=profileData;
			profileData->unqualifiedName=name;
		}
		else
		{
			profileData=(crossplatform::ProfileData*)parentData->children[name];
		}
		profileData->name=name;
		profileData->last_child_updated=0;
		profileStack.push_back(profileData);
		parentData->updatedThisFrame=true;

		profileData->parent=parentData;
		SIMUL_ASSERT(profileData->QueryStarted == false);
		if(profileData->QueryFinished!= false)
			return;

		if(profileData->DisjointQuery == NULL)
		{
			// Create the queries
			std::string n=name;
			profileData->DisjointQuery			=renderPlatform->CreateQuery(crossplatform::QUERY_TIMESTAMP_DISJOINT);
			InitQuery(profileData->DisjointQuery);
			profileData->DisjointQuery->RestoreDeviceObjects(deviceContext.renderPlatform);
			profileData->TimestampStartQuery	=renderPlatform->CreateQuery(crossplatform::QUERY_TIMESTAMP);
			InitQuery(profileData->TimestampStartQuery);
			profileData->TimestampStartQuery->RestoreDeviceObjects(deviceContext.renderPlatform);
			profileData->TimestampEndQuery		=renderPlatform->CreateQuery(crossplatform::QUERY_TIMESTAMP);
			InitQuery(profileData->TimestampEndQuery);
			profileData->TimestampEndQuery->RestoreDeviceObjects(deviceContext.renderPlatform);

			profileData->DisjointQuery->SetName((n+" disjoint").c_str());
			profileData->TimestampStartQuery->SetName((n+" start").c_str());
			profileData->TimestampEndQuery->SetName((n+" end").c_str());
		}
		if(profileData->DisjointQuery)
		{
		// Start a disjoint query first
			profileData->DisjointQuery->Begin(deviceContext);

		// Insert the start timestamp   
			profileData->TimestampStartQuery->End(deviceContext);

			profileData->QueryStarted = true;
		}
	}
}

void GpuProfiler::InitQuery(Query *)
{
}

void GpuProfiler::End(crossplatform::DeviceContext &deviceContext)
{
	if (!enabled||!renderPlatform||!root||!frame_active)
		return;
	
	renderPlatform->EndEvent(deviceContext);
#if PLATFORM_DEBUG_PROFILING_LEVELS
	if(profilingStrings.size())
	{
		SIMUL_COUT << "Profiler debugging: end   level "<<level<<", "<<profilingStrings.back().c_str()<< std::endl;
	
		profilingStrings.pop_back();
	}
	else
	{
		SIMUL_ASSERT_WARN_ONCE(level>0,"Profiler debugging: level zero calling GpuProfiler::End")
		SIMUL_COUT << "Profiler debugging: end   level "<<level<<", stack empty."<< std::endl;
	}
#endif
	level--;
	if(level>=max_level)
		return;
	level_in_use--;
	if (level_in_use != level)
	{
		SIMUL_CERR << "Profiler level out of whack! Do you have a mismatched begin/end pair?" << std::endl;
	}
	if(level>=max_level_this_frame)
		return;
	if(!profileStack.size())
		return;
	
    crossplatform::ProfileData *profileData=(crossplatform::ProfileData *)profileStack.back();
	
	profileStack.pop_back();
    if(profileData->QueryStarted != true)
		return;
	profileData->updatedThisFrame=true;
    SIMUL_ASSERT(profileData->QueryFinished == false);
    // Insert the end timestamp    
	profileData->TimestampEndQuery->End(deviceContext);
    //context->End(profileData->TimestampEndQuery[currFrame]);
	profileData->DisjointQuery->End(deviceContext);
    // End the disjoint query
    profileData->QueryStarted = false;
    profileData->QueryFinished = true;
}

void GpuProfiler::StartFrame(crossplatform::DeviceContext &deviceContext)
{
	if(current_framenumber==renderPlatform->GetFrameNumber())
		return;
	current_framenumber=renderPlatform->GetFrameNumber();
	if(level!=0)
	{
		SIMUL_ASSERT_WARN_ONCE(level==0,"level not zero at StartFrame")
		//level=0;
		//profileStack.clear();
		return;
	}
	core::BaseProfilingInterface::StartFrame();
}

void GpuProfiler::WalkEndFrame(crossplatform::DeviceContext &deviceContext,crossplatform::ProfileData *profile)
{
    for(auto i:profile->children)
	{
		WalkEndFrame(deviceContext,(crossplatform::ProfileData*)i.second);
	}
	if(profile->updatedThisFrame)
		profile->age=0;
	if(profile!=root)
	{
		for(auto u:profile->children)
		{
			ProfileData	*child=(ProfileData*)u.second;
			if(!child->updatedThisFrame&&child->children.size()==0)
			{
				child->age++;
				if(child->age>1000)
				{
					profile->children.erase(u.first);
					break;
				}
			}
		}
	}
	static float mix=0.9f;
	static float final_mix=0.01f;
	if (profile->parent == nullptr)
	{
		mix*=0.999f;
		mix+=0.001f*(final_mix);
	}

	if(profile->QueryFinished == false)
		return;

	profile->QueryFinished = false;

	if(profile->DisjointQuery == NULL)
		return;

	timer.UpdateTime();

	// Get the query data
	UINT64 startTime = 0;
	bool ok=profile->TimestampStartQuery->GetData(deviceContext,&startTime, sizeof(startTime));
		
//       while(context->GetData(profile.TimestampStartQuery[currFrame], &startTime, sizeof(startTime), 0) != S_OK);

    UINT64 endTime = 0;
	ok&=profile->TimestampEndQuery->GetData(deviceContext,&endTime, sizeof(endTime));
    // while(context->GetData(profile.TimestampEndQuery[currFrame], &endTime, sizeof(endTime), 0) != S_OK);
	
    crossplatform::DisjointQueryStruct disjointData;
    ok&=profile->DisjointQuery->GetData(deviceContext,&disjointData, sizeof(disjointData));
	if(!ok)
	{
		// Takes a few frames to spool up...
		//SIMUL_CERR<<"Failed to retrieve timestamp data. Can only do this from the Immediate Context."<<std::endl;
		return;
	}
    timer.UpdateTime();
    queryTime += timer.Time;

    float time = 0.0f;
    if(disjointData.Disjoint == false)
    {
        UINT64 delta = endTime - startTime;
		if(endTime>startTime)
		{
			float frequency = static_cast<float>(disjointData.Frequency);
			time = (delta / frequency) * 1000.0f;
		}
		else
		{
			time=0.0f;
		}
	}
	if ((strcmp(deviceContext.renderPlatform->GetName(), "OpenGL") == 0)
		|| (deviceContext.renderPlatform->AsVulkanDevice() != nullptr))
	{
		UINT64 delta = endTime - startTime;
		if (endTime > startTime)
		{
			time = static_cast<float>(delta);
		}
	}
	profile->time*=(1.f-mix);
	if(profile->updatedThisFrame)
		profile->time+=mix*time;
	if(profile->time>100.0f)
	{
		profile->time=100.0f;
	}
}

void GpuProfiler::EndFrame(crossplatform::DeviceContext &deviceContext)
{
	SIMUL_ASSERT_WARN_ONCE(level==0,"level not zero at EndFrame")
	if(level!=0)
	{
		level=0;
		Clear();
        return;
	}
    if(!root||!enabled||!renderPlatform||!frame_active)
        return;

    currFrame = (currFrame + 1) % crossplatform::Query::QueryLatency;    

    queryTime = 0.0f;
    timer.StartTime();
	
	WalkEndFrame(deviceContext,(crossplatform::ProfileData*)root);

	root->time=0.0f;
	for(auto i=root->children.begin();i!=root->children.end();i++)
	{
		// Only add to total time if we updated the time this frame
 		if (i->second->updatedThisFrame)
 		{
 			root->time+=i->second->time;
 		}
	}
	frame_active=false;
}

template<typename T> inline std::string ToString(const T& val)
{
    std::ostringstream stream;
    if (!(stream << val))
        SIMUL_BREAK("Error converting value to string");
    return stream.str();
}

const char *GpuProfiler::GetDebugText(core::TextStyle style) const
{
	static std::string str;
	str=BaseProfilingInterface::GetDebugText();
	
    str+= "Time spent waiting for queries: " + ToString(queryTime) + "ms";
	str += (style == core::HTML) ? "<br/>" : "\n";
	return str.c_str();
}

const core::ProfileData *GpuProfiler::GetEvent(const core::ProfileData *parent,int i) const
{
	if(parent==NULL)
	{
		return root;
	}
	crossplatform::ProfileData *p=(crossplatform::ProfileData*)parent;
	if(!p||(p!=root&&!p->updatedThisFrame))
		return NULL;
	int j=0;
	for(auto it=p->children.begin();it!=p->children.end();it++,j++)
	{
		if(j==i)
		{
			core::ProfileData *d=it->second;
			d->name=it->second->unqualifiedName;
			d->time=it->second->time;
			return it->second;
		}
	}
	return NULL;
}

float GpuProfiler::GetTime(const std::string &) const
{
	if(!enabled)
		return 0.f;
	return 0.0f;
}

// == ProfileBlock ================================================================================

ProfileBlock::ProfileBlock(crossplatform::DeviceContext &deviceContext,
	GpuProfiler *prof,const std::string& name)
	:profiler(prof)
	,context(&deviceContext)
	,name(name)
{
	if(profiler)
		profiler->Begin(deviceContext,name.c_str());
}

ProfileBlock::~ProfileBlock()
{
	if(profiler)
		profiler->End(*context);
}

float ProfileBlock::GetTime() const
{
	return profiler->GetTime(name);
}