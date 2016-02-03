#include "GpuProfiler.h"
#include "Simul/Platform/CrossPlatform/Effect.h"
#include "Simul/Base/StringFunctions.h"
#include "Simul/Base/RuntimeError.h"
#include "Simul/Base/StringToWString.h"
#include "Simul/Platform/CrossPlatform/RenderPlatform.h"
#include "Simul/Platform/CrossPlatform/DeviceContext.h"
#include <sstream>
#include <iostream>
#include <map>


using namespace simul;
using namespace crossplatform;
using namespace std;
static std::map<void*,simul::crossplatform::GpuProfilingInterface*> gpuProfilingInterface;
typedef uint64_t UINT64;
typedef int                 BOOL;
static string rootstring("root");

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
namespace simul
{
	namespace crossplatform
	{
		void SetGpuProfilingInterface(crossplatform::DeviceContext &context,GpuProfilingInterface *p)
		{
			gpuProfilingInterface[&context]=p;
		}
		GpuProfilingInterface *GetGpuProfilingInterface(crossplatform::DeviceContext &context)
		{
			if(gpuProfilingInterface.find(&context)==gpuProfilingInterface.end())
				return NULL;
			return gpuProfilingInterface[&context];
		}
	}
}

GpuProfiler::GpuProfiler()
	:renderPlatform(NULL)
	,enabled(false)
	,level(0)
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
//	std::cout<<"Profiler::Initialize device "<<(unsigned)device<<std::endl;
}

void GpuProfiler::InvalidateDeviceObjects()
{
#ifdef SIMUL_WIN8_SDK
	SAFE_RELEASE(pUserDefinedAnnotation);
#endif
    for(auto iter = profileMap.begin(); iter != profileMap.end(); iter++)
    {
        base::ProfileData *profile = (*iter).second;
		delete profile;
	}
	profileMap.clear();
    renderPlatform = NULL;
    enabled=true;
}

void GpuProfiler::Begin(crossplatform::DeviceContext &deviceContext,const char *name)
{
	level++;
	if(level>max_level)
		return;
	std::string parent;
	if(last_name.size())
		parent=(last_name.back());
	{
		std::string qualified_name(name);
		if(last_name.size())
			qualified_name=(parent+".")+name;
		last_name.push_back(qualified_name);
		last_context.push_back(&deviceContext);
		if(!enabled||!renderPlatform)
			return;
		crossplatform::ProfileData *profileData = NULL;
		if(profileMap.find(qualified_name)==profileMap.end())
		{
			profileMap[qualified_name]=profileData=new crossplatform::ProfileData;
			profileData->unqualifiedName=name;
		}
		else
		{
			profileData=(crossplatform::ProfileData*)profileMap[qualified_name];
		}
		profileData->name=name;
		profileData->last_child_updated=0;
		crossplatform::ProfileData *parentData=NULL;
		if(parent.length())
			parentData=(crossplatform::ProfileData*)profileMap[parent];
		else
			parentData=(crossplatform::ProfileData*)profileMap["root"];
		parentData->updatedThisFrame=true;
		if(parentData)
		{
			base::ChildMap::iterator in_parent=parentData->children.end();
			if(profileData->child_index!=0)
			{
				in_parent=parentData->children.find(profileData->child_index);
				if(in_parent->second!=profileData)
					in_parent=parentData->children.end();
			}
			else
				profileData->child_index=parentData->last_child_updated+1;
			while(in_parent==parentData->children.end())
			{
				in_parent=parentData->children.find(profileData->child_index);
				// No such child? Add it.
				if(in_parent==parentData->children.end())
				{
					parentData->children[profileData->child_index]=profileData;
					break;
				}
				// This child exists, but it's not the current profile? Keep looking
				if(in_parent->second!=profileData)
				{
					in_parent=parentData->children.end();
					profileData->child_index++;
			}
				else
				{
					// Found our spot
					break;
		}
			}
		}
		parentData->last_child_updated=profileData->child_index;
		profileData->parent=parentData;
		SIMUL_ASSERT(profileData->QueryStarted == false);
		if(profileData->QueryFinished!= false)
			return;

		if(profileData->DisjointQuery == NULL)
		{
			// Create the queries
			std::string disjointName=qualified_name+"disjoint";
			std::string startName	=qualified_name+"start";
			std::string endName		=qualified_name+"end";
			profileData->DisjointQuery			=renderPlatform->CreateQuery(crossplatform::QUERY_TIMESTAMP_DISJOINT);
			profileData->DisjointQuery->RestoreDeviceObjects(deviceContext.renderPlatform);
			//	CreateQuery(device,desc,disjointName.c_str());
			profileData->DisjointQuery->SetName(disjointName.c_str());
			profileData->TimestampStartQuery	=renderPlatform->CreateQuery(crossplatform::QUERY_TIMESTAMP);
			profileData->TimestampStartQuery->RestoreDeviceObjects(deviceContext.renderPlatform);
			profileData->TimestampStartQuery->SetName(startName.c_str());
			profileData->TimestampEndQuery		=renderPlatform->CreateQuery(crossplatform::QUERY_TIMESTAMP);
			profileData->TimestampEndQuery->RestoreDeviceObjects(deviceContext.renderPlatform);
			profileData->TimestampEndQuery->SetName(endName.c_str());
		}
		if(profileData->DisjointQuery)
		{
		// Start a disjoint query first
			profileData->DisjointQuery->Begin(deviceContext);

		// Insert the start timestamp   
			profileData->TimestampStartQuery->End(deviceContext);

			profileData->QueryStarted = true;
		}
		renderPlatform->BeginEvent(deviceContext,name);
	}
}

void GpuProfiler::End()
{
	level--;
	if(level>=max_level)
		return;
	if(!last_name.size())
		return;
	std::string name		=last_name.back();
	last_name.pop_back();
	crossplatform::DeviceContext *deviceContext=last_context.back();
	ID3D11DeviceContext *context=deviceContext->asD3D11DeviceContext();
	last_context.pop_back();
    if(!enabled||!renderPlatform||!context)
        return;
	
	renderPlatform->EndEvent(*deviceContext);
    crossplatform::ProfileData *profileData=(crossplatform::ProfileData*) profileMap[name];
	
    if(profileData->QueryStarted != true)
		return;
	profileData->updatedThisFrame=true;
    SIMUL_ASSERT(profileData->QueryFinished == false);
    // Insert the end timestamp    
    //context->End(profileData->TimestampEndQuery[currFrame]);
	profileData->TimestampEndQuery->End(*deviceContext);
    // End the disjoint query
  //  context->End(profileData->DisjointQuery[currFrame]);
	profileData->DisjointQuery->End(*deviceContext);

    profileData->QueryStarted = false;
    profileData->QueryFinished = true;
}

void GpuProfiler::StartFrame(crossplatform::DeviceContext &deviceContext)
{
	level=0;
	if(profileMap.find(rootstring)==profileMap.end())
		profileMap[rootstring]=new crossplatform::ProfileData();
	
	profileMap[rootstring]->last_child_updated=0;
    for(auto iter = profileMap.begin(); iter != profileMap.end(); iter++)
    {
        base::ProfileData *profile = ((*iter).second);
		profile->updatedThisFrame=false;
	}
}

void GpuProfiler::EndFrame(crossplatform::DeviceContext &deviceContext)
{
	SIMUL_ASSERT(level==0)
    if(!enabled||!renderPlatform)
        return;

    currFrame = (currFrame + 1) % crossplatform::Query::QueryLatency;    

    queryTime = 0.0f;
    timer.StartTime();
    // Iterate over all of the profileMap
	base::ProfileMap::iterator iter;
    for(iter = profileMap.begin(); iter != profileMap.end(); iter++)
    {
        crossplatform::ProfileData *profile =(crossplatform::ProfileData*)((*iter).second);

		static float mix=0.07f;
		profile->time*=(1.f-mix);

        if(profile->QueryFinished == false)
            continue;

        profile->QueryFinished = false;

        if(profile->DisjointQuery == NULL)
            continue;

        timer.UpdateTime();

        // Get the query data
        UINT64 startTime = 0;
        if(!profile->TimestampStartQuery->GetData(deviceContext,&startTime, sizeof(startTime)))
            continue;
//       while(context->GetData(profile.TimestampStartQuery[currFrame], &startTime, sizeof(startTime), 0) != S_OK);

        UINT64 endTime = 0;
        if(!profile->TimestampEndQuery->GetData(deviceContext,&endTime, sizeof(endTime)))
            continue;
       // while(context->GetData(profile.TimestampEndQuery[currFrame], &endTime, sizeof(endTime), 0) != S_OK);
		
        crossplatform::DisjointQueryStruct disjointData;
        if(!profile->DisjointQuery->GetData(deviceContext,&disjointData, sizeof(disjointData)))
            continue;
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
        }        
        profile->time+=mix*time;
		if(profile->time>10.0f)
		{
			profile->time=10.0f;
		}
    }
	if(profileMap.find("root")==profileMap.end())
		profileMap["root"]=new crossplatform::ProfileData();
	crossplatform::ProfileData *rootProfileData=(crossplatform::ProfileData*)profileMap["root"];
	rootProfileData->time=0.0f;
	for(auto i=rootProfileData->children.begin();i!=rootProfileData->children.end();i++)
	{
		rootProfileData->time+=i->second->time;
	}
}

template<typename T> inline std::string ToString(const T& val)
{
    std::ostringstream stream;
    if (!(stream << val))
        SIMUL_BREAK("Error converting value to string");
    return stream.str();
}

const char *GpuProfiler::GetDebugText(base::TextStyle style) const
{
	static std::string str;
	str=BaseProfilingInterface::GetDebugText();
	
    str+= "Time spent waiting for queries: " + ToString(queryTime) + "ms";
	str += (style == base::HTML) ? "<br/>" : "\n";
	return str.c_str();
}

const base::ProfileData *GpuProfiler::GetEvent(const base::ProfileData *parent,int i) const
{
	if(parent==NULL)
	{
		auto u=profileMap.find("root");
		if(u==profileMap.end())
			return NULL;
		return u->second;
	}
	crossplatform::ProfileData *p=(crossplatform::ProfileData*)parent;
	int j=0;
	for(auto it=p->children.begin();it!=p->children.end();it++,j++)
	{
		if(j==i)
		{
			base::ProfileData *d=it->second;
			d->name=it->second->unqualifiedName;
			d->time=it->second->time;
			return it->second;
		}
	}
	return NULL;
}

float GpuProfiler::GetTime(const std::string &name) const
{
	if(!enabled)
		return 0.f;
	return profileMap.find(name)->second->time;
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
		profiler->End();
}

float ProfileBlock::GetTime() const
{
	return profiler->GetTime(name);
}