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
	//std::cout<<"Profiler::Uninitialize device was "<<(unsigned)device<<std::endl;
	profiles.clear();
    this->device = NULL;
    enabled=true;
}

void Profiler::Initialize(ID3D11Device* device)
{
    this->device = device;
    enabled=true;
//	std::cout<<"Profiler::Initialize device "<<(unsigned)device<<std::endl;
}

void Profiler::Begin(void *ctx,const char *name)
{
	IUnknown *unknown=(IUnknown *)ctx;
	ID3D11DeviceContext *context=(ID3D11DeviceContext*)ctx;
	last_name.push_back(name);
	last_context.push_back(context);
	if(!context||!enabled||!device)
        return;
    ProfileData& profileData = profiles[name];
    _ASSERT(profileData.QueryStarted == FALSE);
    if(profileData.QueryFinished!= FALSE)
        return;
    
    if(profileData.DisjointQuery[currFrame] == NULL)
    {
        // Create the queries
        D3D11_QUERY_DESC desc;
        desc.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;
        desc.MiscFlags = 0;
        DXCall(device->CreateQuery(&desc, &profileData.DisjointQuery[currFrame]));

        desc.Query = D3D11_QUERY_TIMESTAMP;
        DXCall(device->CreateQuery(&desc, &profileData.TimestampStartQuery[currFrame]));
        DXCall(device->CreateQuery(&desc, &profileData.TimestampEndQuery[currFrame]));
    }

    // Start a disjoint query first
    context->Begin(profileData.DisjointQuery[currFrame]);

    // Insert the start timestamp    
    context->End(profileData.TimestampStartQuery[currFrame]);

    profileData.QueryStarted = TRUE;
}

void Profiler::End()
{
	std::string name		=last_name.back();
	last_name.pop_back();
	ID3D11DeviceContext *context=last_context.back();
	last_context.pop_back();
    if(!enabled||!device||!context)
        return;

    ProfileData& profileData = profiles[name];
    if(profileData.QueryStarted != TRUE)
		return;
    _ASSERT(profileData.QueryFinished == FALSE);

    // Insert the end timestamp    
    context->End(profileData.TimestampEndQuery[currFrame]);

    // End the disjoint query
    context->End(profileData.DisjointQuery[currFrame]);

    profileData.QueryStarted = FALSE;
    profileData.QueryFinished = TRUE;
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
        ProfileData& profile = (*iter).second;
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
        iter->second.time=time;
    }

    output+= "Time spent waiting for queries: " + ToString(queryTime) + "ms";
}

float Profiler::GetTime(const std::string &name) const
{
    if(!enabled||!device)
		return 0.f;
	return profiles.find(name)->second.time;
}
#include "Simul/Base/StringFunctions.h"
const char *Profiler::GetDebugText() const
{
	static std::string str;
	str="";
	for(Profiler::ProfileMap::const_iterator i=profiles.begin();i!=profiles.end();i++)
	{
		str+=i->first.c_str();
		str+=" ";
		str+=simul::base::stringFormat("%4.4g\n",i->second.time);
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