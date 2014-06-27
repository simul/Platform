//=================================================================================================
//
//	Query Profiling Sample
//  by MJP
//  http://mynameismjp.wordpress.com/
//
//  All code and content licensed under Microsoft Public License (Ms-PL)
//
//=================================================================================================

#include "Profiler.h"
#include "DX11Exception.h"
#include "Utilities.h"
#include "Simul/Base/StringFunctions.h"
using namespace simul;
using namespace dx11;
using std::string;
using std::map;
bool enabled=false;

// Throws a DXException on failing HRESULT
inline void DXCall(HRESULT hr)
{
	if (FAILED(hr))
    {
        _ASSERT(false);
		throw DX11Exception(hr);
    }
}
// == Profiler ====================================================================================

Profiler Profiler::GlobalProfiler;

Profiler &Profiler::GetGlobalProfiler()
{
	return GlobalProfiler;
}

Profiler::~Profiler()
{
	//std::cout<<"Profiler::~Profiler"<<std::endl;
	Uninitialize();
}

void Profiler::Uninitialize()
{
    for(ProfileMap::iterator iter = profileMap.begin(); iter != profileMap.end(); iter++)
    {
        ProfileData *profile = (*iter).second;
		delete profile;
	}
	profileMap.clear();
    this->device = NULL;
    enabled=true;
}

void Profiler::Initialize(ID3D11Device* device)
{
    this->device = device;
    enabled=true;
//	std::cout<<"Profiler::Initialize device "<<(unsigned)device<<std::endl;
}

ID3D11Query *CreateQuery(ID3D11Device* device,D3D11_QUERY_DESC &desc,const char *name)
{
	ID3D11Query *q=NULL;
    DXCall(device->CreateQuery(&desc, &q));
	SetDebugObjectName( q, name );
	return q;
}

void Profiler::Begin(void *ctx,const char *name)
{
	IUnknown *unknown=(IUnknown *)ctx;
	ID3D11DeviceContext *context=(ID3D11DeviceContext*)ctx;

	std::string parent;
	if(last_name.size())
		parent=(last_name.back());
	std::string qualified_name(name);
	if(last_name.size())
		qualified_name=(parent+".")+name;
	last_name.push_back(qualified_name);
	last_context.push_back(context);
	if(!context||!enabled||!device)
        return;
    ProfileData *profileData = NULL;
	if(profileMap.find(qualified_name)==profileMap.end())
	{
		profileData=profileMap[qualified_name]=new ProfileData;
		profileData->unqualifiedName=name;
	}
	else
	{
		profileData=profileMap[qualified_name];
	}
	profileData->last_child_updated=0;
	int new_child_index=0;
    ProfileData *parentData=NULL;
	if(parent.length())
		parentData=profileMap[parent];
	if(parentData)
	{
		new_child_index=++parentData->last_child_updated;
		while(parentData->children.find(new_child_index)!=parentData->children.end()&&parentData->children[new_child_index]!=profileData)
		{
			new_child_index++;
		}
		parentData->children[new_child_index]=profileData;
		if(profileData->child_index!=0&&new_child_index!=profileData->child_index&&parentData->children.find(profileData->child_index)!=parentData->children.end())
		{
			parentData->children.erase(profileData->child_index);
		}
		profileData->child_index=new_child_index;
	}
	profileData->parent=parentData;
	if(!parentData)
		rootMap[qualified_name]=profileData;
    _ASSERT(profileData->QueryStarted == FALSE);
    if(profileData->QueryFinished!= FALSE)
        return;
    
    if(profileData->DisjointQuery[currFrame] == NULL)
    {
        // Create the queries
        D3D11_QUERY_DESC desc;
        desc.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;
        desc.MiscFlags = 0;
		std::string disjointName=qualified_name+"disjoint";
		std::string startName	=qualified_name+"start";
		std::string endName		=qualified_name+"end";
		profileData->DisjointQuery[currFrame]		=CreateQuery(device,desc,disjointName.c_str());
        desc.Query = D3D11_QUERY_TIMESTAMP;
        profileData->TimestampStartQuery[currFrame]	=CreateQuery(device,desc,startName.c_str());
        profileData->TimestampEndQuery[currFrame]	=CreateQuery(device,desc,endName.c_str());
    }

    // Start a disjoint query first
    context->Begin(profileData->DisjointQuery[currFrame]);

    // Insert the start timestamp    
    context->End(profileData->TimestampStartQuery[currFrame]);

    profileData->QueryStarted = TRUE;
}

void Profiler::End()
{
	std::string name		=last_name.back();
	last_name.pop_back();
	ID3D11DeviceContext *context=last_context.back();
	last_context.pop_back();
    if(!enabled||!device||!context)
        return;

    ProfileData *profileData = profileMap[name];
    if(profileData->QueryStarted != TRUE)
		return;
    _ASSERT(profileData->QueryFinished == FALSE);

    // Insert the end timestamp    
    context->End(profileData->TimestampEndQuery[currFrame]);

    // End the disjoint query
    context->End(profileData->DisjointQuery[currFrame]);

    profileData->QueryStarted = FALSE;
    profileData->QueryFinished = TRUE;
}

template<typename T> inline std::string ToString(const T& val)
{
    std::ostringstream stream;
    if (!(stream << val))
        throw std::runtime_error("Error converting value to string");
    return stream.str();
}

void Profiler::EndFrame(void* c)
{
	ID3D11DeviceContext *context=(ID3D11DeviceContext*)c;
    if(!enabled||!device)
        return;

    currFrame = (currFrame + 1) % QueryLatency;    

    queryTime = 0.0f;
    // Iterate over all of the profileMap
    ProfileMap::iterator iter;
    for(iter = profileMap.begin(); iter != profileMap.end(); iter++)
    {
        ProfileData& profile = *((*iter).second);

		static float mix=0.01f;
		iter->second->time*=(1.f-mix);

        if(profile.QueryFinished == FALSE)
            continue;

        profile.QueryFinished = FALSE;

        if(profile.DisjointQuery[currFrame] == NULL)
            continue;

        timer.UpdateTime();

        // Get the query data
        UINT64 startTime = 0;
        while(context->GetData(profile.TimestampStartQuery[currFrame], &startTime, sizeof(startTime), 0) != S_OK);

        UINT64 endTime = 0;
        while(context->GetData(profile.TimestampEndQuery[currFrame], &endTime, sizeof(endTime), 0) != S_OK);

        D3D11_QUERY_DATA_TIMESTAMP_DISJOINT disjointData;
        while(context->GetData(profile.DisjointQuery[currFrame], &disjointData, sizeof(disjointData), 0) != S_OK);

        timer.UpdateTime();
        queryTime += timer.Time;

        float time = 0.0f;
        if(disjointData.Disjoint == FALSE)
        {
            UINT64 delta = endTime - startTime;
            float frequency = static_cast<float>(disjointData.Frequency);
            time = (delta / frequency) * 1000.0f;
        }        

        iter->second->time+=mix*time;
    }
}

float Profiler::GetTime(const std::string &name) const
{
    if(!enabled||!device)
		return 0.f;
	return profileMap.find(name)->second->time;
}

std::string Walk(Profiler::ProfileData *p,int tab,float parent_time)
{
	if(p->children.size()==0)
		return "";
	std::string str;
	for(Profiler::ChildMap::const_iterator i=p->children.begin();i!=p->children.end();i++)
	{
		for(int j=0;j<tab;j++)
			str+="  ";
		str+=i->second->unqualifiedName.c_str();
		str+=" ";
		str+=simul::base::stringFormat("%4.4f (%3.3f%%)\n",i->second->time,100.f*i->second->time/parent_time);
		str+=Walk(i->second,tab+1,i->second->time);
	}
	return str;
}

const char *Profiler::GetDebugText() const
{
	static std::string str;
	str="";
	for(Profiler::ProfileMap::const_iterator i=rootMap.begin();i!=rootMap.end();i++)
	{
		str+=i->second->unqualifiedName.c_str();
		str+=" ";
		str+=simul::base::stringFormat("%4.4f\n",i->second->time);
		str+=Walk(i->second,1,i->second->time);
	}

    str+= "Time spent waiting for queries: " + ToString(queryTime) + "ms";
	return str.c_str();
}

// == ProfileBlock ================================================================================

ProfileBlock::ProfileBlock(ID3D11DeviceContext* c,const std::string& name)
	:name(name)
	,context(c)
{
	Profiler::GetGlobalProfiler().Begin(context,name.c_str());
}

ProfileBlock::~ProfileBlock()
{
    Profiler::GetGlobalProfiler().End();
}

float ProfileBlock::GetTime() const
{
	return Profiler::GetGlobalProfiler().GetTime(name);
}