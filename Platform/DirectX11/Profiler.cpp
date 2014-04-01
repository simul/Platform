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
    for(ProfileMap::iterator iter = profiles.begin(); iter != profiles.end(); iter++)
    {
        ProfileData *profile = (*iter).second;
		delete profile;
	}
	profiles.clear();
    this->device=NULL;
    enabled=true;
}

void Profiler::Initialize(ID3D11Device* device)
{
    this->device	=device;
    enabled			=true;
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
	std::string full_name=name;
	ProfileData *parentProfileData=NULL;
	if(last_name.size())
	{
		parent=last_name.back();
		full_name=parent+".";
		full_name+=name;
		parentProfileData = profiles[parent];
	}
	last_name.push_back(full_name);
	last_context.push_back(context);
	if(!context||!enabled||!device)
        return;
	if(profiles.find(full_name)==profiles.end())
		profiles[full_name]=new ProfileData;
    ProfileData *profileData=profiles[full_name];
		
	profileData->name		=name;
	profileData->full_name	=full_name;
	if(parentProfileData)
		parentProfileData->children.insert(profileData);
    _ASSERT(profileData->QueryStarted == FALSE);
    if(profileData->QueryFinished!= FALSE)
        return;
	profileData->parent=parentProfileData;
    if(profileData->DisjointQuery[currFrame] == NULL)
    {
        // Create the queries
        D3D11_QUERY_DESC desc;
        desc.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;
        desc.MiscFlags = 0;
		std::string disjointName="disjoint";
		std::string startName	="start";
		std::string endName		="end";
		profileData->DisjointQuery[currFrame]		=CreateQuery(device,desc,disjointName.c_str());
        desc.Query = D3D11_QUERY_TIMESTAMP;
        profileData->TimestampStartQuery[currFrame]	=CreateQuery(device,desc, startName.c_str());
        profileData->TimestampEndQuery[currFrame]	=CreateQuery(device,desc, endName.c_str());
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

    ProfileData *profileData = profiles[name];
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

void Profiler::EndFrame(ID3D11DeviceContext* context)
{
    if(!enabled||!device)
        return;

    currFrame = (currFrame + 1) % QueryLatency;    

    float queryTime = 0.0f;
	output="";
    // Iterate over all of the profiles
    ProfileMap::iterator iter;
    for(iter = profiles.begin(); iter != profiles.end(); iter++)
    {
        ProfileData& profile = *((*iter).second);
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

        output+= (*iter).first + ": " + ToString(time) + "ms\n";
        iter->second->time=time;
    }

    output+= "Time spent waiting for queries: " + ToString(queryTime) + "ms";
}

float Profiler::GetTime(const std::string &name) const
{
    if(!enabled||!device)
		return 0.f;
	return profiles.find(name)->second->time;
}

std::string Profiler::GetChildText(const char *name,std::string tab) const
{
	std::string str;
	str="";
	Profiler::ProfileMap::const_iterator i=profiles.find(name);
	ProfileData *p=i->second;
	for(std::set<ProfileData*>::const_iterator i=p->children.begin();i!=p->children.end();i++)
	{
		str+=tab;
		str+=(*i)->name.c_str();
		str+=" ";
		str+=simul::base::stringFormat("%4.4g\n",(*i)->time);
		str+=GetChildText((*i)->full_name.c_str(),tab+"\t");
	}
	return str;
}

const char *Profiler::GetDebugText() const
{
	static std::string str;
	str="";
	for(Profiler::ProfileMap::const_iterator i=profiles.begin();i!=profiles.end();i++)
	{
		if(i->second->parent==NULL)
		{
			str+=i->first.c_str();
			str+=" ";
			str+=simul::base::stringFormat("%4.4g\n",i->second->time);
			str+=GetChildText(i->first.c_str(),"\t");
		}
	}
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