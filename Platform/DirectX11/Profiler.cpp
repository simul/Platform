#include "Profiler.h"
#include "DX11Exception.h"
#include "Utilities.h"
#include "Simul/Base/StringFunctions.h"
#include "Simul/Base/RuntimeError.h"
#include "Simul/Base/StringToWString.h"
#ifdef SIMUL_ENABLE_PIX
#include "pix.h"
#endif
using namespace simul;
using namespace dx11;
using std::string;
using std::map;
#pragma optimize("",off)
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
Profiler::Profiler():pUserDefinedAnnotation(NULL)
{

}

Profiler::~Profiler()
{
	//std::cout<<"Profiler::~Profiler"<<std::endl;
	Uninitialize();
}

void Profiler::Uninitialize()
{
#ifdef SIMUL_WIN8_SDK
	SAFE_RELEASE(pUserDefinedAnnotation);
#endif
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

void Profiler::StartFrame(void* ctx)
{
	level=0;
	profileMap["root"]=&rootProfileData;
#ifdef SIMUL_WIN8_SDK
	if(enabled)
	{
		SAFE_RELEASE(pUserDefinedAnnotation);
	IUnknown *unknown=(IUnknown *)ctx;
#ifdef _XBOX_ONE
		V_CHECK(unknown->QueryInterface( __uuidof(pUserDefinedAnnotation), reinterpret_cast<void**>(&pUserDefinedAnnotation) ));
#else
		V_CHECK(unknown->QueryInterface(IID_PPV_ARGS(&pUserDefinedAnnotation)));
#endif
}
#endif
    ProfileMap::iterator iter;
    for(iter = profileMap.begin(); iter != profileMap.end(); iter++)
    {
        ProfileData& profile = *((*iter).second);
		profile.updatedThisFrame=false;
	}
}
void Profiler::Begin(void *ctx,const char *name)
{
	level++;
	if(level>max_level)
		return;
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
#ifdef SIMUL_WIN8_SDK
	if(!profileData->wUnqualifiedName.length())
		profileData->wUnqualifiedName=base::StringToWString(name);
	if(pUserDefinedAnnotation)
	pUserDefinedAnnotation->BeginEvent(profileData->wUnqualifiedName.c_str());
#endif
#ifdef SIMUL_ENABLE_PIX
	PIXBeginEvent( 0, profileData->wUnqualifiedName.c_str(), name );
#endif
	int new_child_index=0;
    ProfileData *parentData=NULL;
	if(parent.length())
		parentData=profileMap[parent];
	else
		parentData=&rootProfileData;
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
		profileData->gotResults[currFrame]=true;
		if(!profileData->DisjointQuery[currFrame])
			SIMUL_BREAK("Failed to create query");
    }
	if(profileData->DisjointQuery[currFrame])
	{
		if(!profileData->gotResults[currFrame])
		{
			return;//SIMUL_BREAK("not got query results!")
		}
    // Start a disjoint query first
		context->Begin(profileData->DisjointQuery[currFrame]);

    // Insert the start timestamp    
		context->End(profileData->TimestampStartQuery[currFrame]);

	    profileData->QueryStarted = TRUE;
	}
}
void Profiler::End()
{
	level--;
	if(level>=max_level)
		return;
	std::string name		=last_name.back();
	last_name.pop_back();
	ID3D11DeviceContext *context=last_context.back();
	last_context.pop_back();
    if(!enabled||!device||!context)
        return;

    ProfileData *profileData = profileMap[name];
	if(!profileData->gotResults[currFrame])
        return;
	
    if(profileData->QueryStarted != TRUE)
		return;
	profileData->updatedThisFrame=true;
    _ASSERT(profileData->QueryFinished == FALSE);
	if(!profileData->gotResults[currFrame])
	{
		SIMUL_BREAK("not got query results!")
	}
    // Insert the end timestamp    
    context->End(profileData->TimestampEndQuery[currFrame]);

    // End the disjoint query
    context->End(profileData->DisjointQuery[currFrame]);
	profileData->gotResults[currFrame]=false;

    profileData->QueryStarted = FALSE;
    profileData->QueryFinished = TRUE;
#ifdef SIMUL_WIN8_SDK
	if(pUserDefinedAnnotation)
		pUserDefinedAnnotation->EndEvent();
#endif
#ifdef SIMUL_ENABLE_PIX
	PIXEndEvent(  );
#endif
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
	SIMUL_ASSERT(level==0)
#ifdef SIMUL_WIN8_SDK
	SAFE_RELEASE(pUserDefinedAnnotation);
#endif
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

		static float mix=0.07f;
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
		profile.gotResults[currFrame]=true;
        timer.UpdateTime();
        queryTime += timer.Time;

        float time = 0.0f;
        if(disjointData.Disjoint == FALSE)
        {
            UINT64 delta = endTime - startTime;
            float frequency = static_cast<float>(disjointData.Frequency);
            time = (delta / frequency) * 1000.0f;
        }        

        profile.time+=mix*time;
    }
	rootProfileData.time=0.0f;
	for(auto i=rootProfileData.children.begin();i!=rootProfileData.children.end();i++)
	{
		rootProfileData.time+=i->second->time;
	}
}

float Profiler::GetTime(const std::string &name) const
{
    if(!enabled||!device)
		return 0.f;
	return profileMap.find(name)->second->time;
}

static const string format_template("<div style=\"color:#%06x;margin-left:%d;\">%s</div>");
static string formatLine(const char *name,int tab,float number,float parent,base::TextStyle style)
{
	float proportion_of_parent=0.0f;
	if(parent>0.0f)
		proportion_of_parent=number/parent;
	string str;
	if (style!=base::HTML)
	for(int j=0;j<tab;j++)
	{
		str+="  ";
	}
	string content=name;
	content+=" ";
	content+=base::stringFormat("%4.4f",number);
	if(parent>0.0f)
		content += base::stringFormat(" (%3.3f%%)", 100.f*proportion_of_parent);
	if (style == base::HTML)
	{
		unsigned colour=0xFF0000;
		unsigned greenblue=255-(unsigned)(175.0f*proportion_of_parent);
		colour|=(greenblue<<8)|(greenblue);
		int padding=12*tab;
		content=base::stringFormat(format_template.c_str(),colour,padding,content.c_str());
	}
	else if (style == base::RICHTEXT)
	{
		unsigned colour = 0xFF0000;
		unsigned greenblue = 255 - (unsigned)(175.0f*proportion_of_parent);
		colour |= (greenblue << 8) | (greenblue);
		int padding = 12 * tab;
		content = base::stringFormat("<color=#%06x>%s</color>", colour,  content.c_str());
	}
	str+=content;
	str += (style == base::HTML )? "" : "\n";
	return str;
}

std::string Profiler::Walk(Profiler::ProfileData *p,int tab,float parent_time,base::TextStyle style) const
{
	if(tab>=max_level)
		return "";
	if(p->children.size()==0)
		return "";
	std::string str;
	for(Profiler::ChildMap::const_iterator i=p->children.begin();i!=p->children.end();i++)
	{
		if(!i->second->updatedThisFrame)
			continue;
		for(int j=0;j<tab;j++)
			str+="  ";
		str += formatLine(i->second->unqualifiedName.c_str(), tab, i->second->time, parent_time, style);
		str += Walk(i->second, tab + 1, i->second->time, style);
	}
	return str;
}

const char *Profiler::GetDebugText(base::TextStyle style) const
{
	static std::string str;
	str="";
	float total=rootProfileData.time;
	str += formatLine("TOTAL", 0, total, 0.0f, style);
	for(auto i=rootProfileData.children.begin();i!=rootProfileData.children.end();i++)
	{
		if(!i->second->updatedThisFrame)
			continue;
		str+=formatLine(i->second->unqualifiedName.c_str(),1,i->second->time,total,style);
		str += Walk(i->second, 2, i->second->time, style);
	}
	str += (style ==base::HTML)? "<br/>" : "\n";
    str+= "Time spent waiting for queries: " + ToString(queryTime) + "ms";
	str += (style == base::HTML) ? "<br/>" : "\n";
	return str.c_str();
}

const base::ProfileData *Profiler::GetEvent(const base::ProfileData *parent,int i) const
{
	if(parent==NULL)
	{
		return &rootProfileData;
	}
	Profiler::ProfileData *p=(Profiler::ProfileData*)parent;
	int j=0;
	for(Profiler::ChildMap::const_iterator it=p->children.begin();it!=p->children.end();it++,j++)
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