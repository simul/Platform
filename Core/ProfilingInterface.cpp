
#ifdef _MSC_VER
#pragma optimize("",off)
#pragma warning(disable:4748)
#include <windows.h>
#endif

#include <map>
#include "Platform/Core/ProfilingInterface.h"
#include "Platform/Core/RuntimeError.h"
#include "Platform/Core/StringFunctions.h"
#include <algorithm>

#ifndef _MSC_VER
    #ifndef UNIX
		#ifdef NN_NINTENDO_SDK
			#include <nn/util/util_FloatFormat.h>
			#define _isnanf (nn::util::FloatFormat16::IsNan)
		#else
			#define _isnanf isnan
		#endif
	#else
		#include <cmath>
		#define _isnanf ( std::isnan )
	#endif
#else
#include <math.h>
#define _isnanf _isnan
#endif

using namespace platform;
using namespace core;
using std::string;

namespace platform
{
	namespace core
	{
		THREAD_TYPE GetThreadId()
		{
#ifdef _MSC_VER
			return GetCurrentThreadId();
#else
	#ifdef NN_NINTENDO_SDK
			return nn::os::GetThreadId(nn::os::GetCurrentThread());
	#elif __ORBIS__
			return scePthreadSelf();
	#else
			return pthread_self();
	#endif
#endif
		}
		static std::map<THREAD_TYPE,ProfilingInterface*> profilingInterfaces;
		void SetProfilingInterface(THREAD_TYPE thread_id,ProfilingInterface *p)
		{
			profilingInterfaces[thread_id]=p;
		}
		ProfilingInterface *GetProfilingInterface(THREAD_TYPE thread_id)
		{
			auto i=profilingInterfaces.find(thread_id);
			if(i==profilingInterfaces.end())
				return NULL;
			return i->second;
		}
	}
}

static const string format_template("<div style=\"color:#%06x;margin-left:%d;\">%s</div>");
static string formatLine(const char *name,int tab,float number,float parent,core::TextStyle style)
{
	float proportion_of_parent=0.0f;
	if(parent>0.0f)
		proportion_of_parent=number/parent;
	string str;
	if (style!=core::HTML)
	for(int j=0;j<tab;j++)
	{
		str+="  ";
	}
	string content=name;
	content+=" ";
	content+=core::stringFormat("%4.4f",number);
	if(parent>0.0f)
		content += core::stringFormat(" (%3.3f%%)", 100.f*proportion_of_parent);
	if (style == core::HTML)
	{
		unsigned colour=0xFF0000;
		unsigned greenblue=255-(unsigned)(175.0f*proportion_of_parent);
		colour|=(greenblue<<8)|(greenblue);
		int padding=12*tab;
		content=core::stringFormat(format_template.c_str(),colour,padding,content.c_str());
	}
	else if (style == core::RICHTEXT)
	{
		unsigned colour = 0xFF0000;
		unsigned greenblue = 255 - (unsigned)(175.0f*proportion_of_parent);
		colour |= (greenblue << 8) | (greenblue);
	//	int padding = 12 * tab;
		content = core::stringFormat("<color=#%06x>%s</color>", colour,  content.c_str());
	}
	str+=content;
	str += (style == core::HTML )? "" : "\n";
	return str;
}

std::string BaseProfilingInterface::Walk(core::ProfileData *profileData,int tab,float parent_time,core::TextStyle style) const
{
	if(tab>=max_level)
		return "";
	if(profileData->children.size()==0)
		return "";
	std::string str;
	for(ChildMap::const_iterator i=profileData->children.begin();i!=profileData->children.end();i++)
	{
		for(int j=0;j<tab;j++)
			str+="  ";
		float t=i->second->time;
		if(!i->second->updatedThisFrame)
			t=0.0f;
		str += formatLine(i->second->unqualifiedName.c_str(), tab, t, parent_time, style);
		str += Walk((ProfileData*)i->second, tab + 1, i->second->time, style);
	}
	return str;
}

platform::core::DefaultProfiler::DefaultProfiler():overhead(0.0f)
{
	timer.StartTime();
	int num_per_frame=100;
	for(int i=0;i<1000;i++)
	{
		StartFrame();
		for(int k=0;k<num_per_frame;k++)
		{
			Begin("overhead");
			End();
		}
		EndFrame();
	}
	std::string name;
	GetCounter(0,name,overhead);
	overhead=(float)((double)overhead/(double)num_per_frame);
}

DefaultProfiler::~DefaultProfiler()
{
    Clear();
}

void platform::core::BaseProfilingInterface::WalkReset(ProfileData *p)
{
	p->frameTime=0.0f;
	p->overhead=0.0f;
	p->updatedThisFrame=false;
	p->last_child_updated=0;
	p->updatedThisFrame=false;
	for(auto i=p->children.begin();i!=p->children.end();i++)
	{
		ProfileData *t=(ProfileData*)i->second;
		WalkReset(t);
	}
}

void platform::core::BaseProfilingInterface::StartFrame()
{
	if(!root)
	{
		root=CreateProfileData();
	}
	root->last_child_updated=0;
	WalkReset(root);
	frame_active=true;
	level=0;
	level_in_use = 0;
	max_level_this_frame=0;
	profileStack.clear();
}

void platform::core::DefaultProfiler::Begin(const char *name)
{
	// Get time at the beginning, so that we can properly calculate the overhead!
	float t=timer.AbsoluteTimeMS();
	level++;
	if(level>max_level)
		return;
	level_in_use++;
	if(!root)
		return;
	if (level_in_use != level)
	{
		SIMUL_CERR << "Profiler level out of whack! Do you have a mismatched begin/end pair?" << std::endl;
	}
    Timing *parentData=NULL;
	if(profileStack.size())
		parentData=(Timing*)profileStack.back();
	else
	{
		parentData=(Timing*)root;
	}
    Timing *profileData = NULL;
	if(parentData->children.find(name)==parentData->children.end())
	{
		parentData->children[name]=profileData=new Timing;
		profileData->unqualifiedName=name;
		profileData->time=0.0f;
		profileData->frameTime=0.0f;
	}
	else
	{
		profileData=(Timing*)parentData->children[name];
	}
	profileStack.push_back(profileData);
	profileData->updatedThisFrame=true;
	profileData->start=t;
	profileData->name=name;
	profileData->last_child_updated=0;
	parentData->updatedThisFrame=true;
	profileData->parent=parentData;
}

void platform::core::DefaultProfiler::End()
{
	level--;
	if(level>=max_level)
		return;
	level_in_use--;
	if (level_in_use != level)
	{
		SIMUL_CERR << "Profiler level out of whack! Do you have a mismatched begin/end pair?" << std::endl;
	}
	if(!root)
		return;
	if(!profileStack.size())
		return;
	Timing *profileData		=(Timing*)profileStack.back();
	profileStack.pop_back();
	if(profileData)
	{
		profileData->overhead		+=overhead;
		float t						=timer.AbsoluteTimeMS();
		t							-=profileData->start;
		profileData->frameTime		+=std::max(0.0f,t);
	}
}

float DefaultProfiler::WalkOverhead(platform::core::DefaultProfiler::Timing *p,int level)
{
	if(level>=max_level)
		return 0.0f;
	float overhead=p->overhead;
	static float introduce=.005f;
	for(ChildMap::const_iterator i=p->children.begin();i!=p->children.end();i++)
	{
		Timing *child	=(Timing*)i->second;
		WalkOverhead(child,level+1);
		overhead		+=child->overhead;
	}
	if(_isnanf(overhead))
	{
		overhead=0.0f;
	}
	if(_isnanf(p->frameTime))
	{
		p->frameTime=0.0f;
	}
	float t		=p->frameTime-overhead;
	p->time		*=(1.f-introduce);
	p->time		+=introduce*t;
	if(_isnanf(p->time))
	{
		p->time=t;
	}
	return overhead;
}

void platform::core::DefaultProfiler::EndFrame()
{
	frame_active=false;
	if(!root)
		return;
	if(!(root)->updatedThisFrame)
		return;
	root->time=0;
	WalkOverhead((Timing*)root,0);
	for(ChildMap::const_iterator i=root->children.begin();i!=root->children.end();i++)
	{
		Timing *child	=(Timing*)i->second;
		root->time+=child->time;
	}
}

bool platform::core::DefaultProfiler::GetCounter(int ,string &,float &)
{
	/*if(i>=(int)profileMap.size())
		return false;
	static ProfileMap::iterator it;
	if(i==0)
		it=profileMap.begin();
	str	=it->first;
	t	=it->second->time;
	it++;*/
	return true;
}

const core::ProfileData *platform::core::DefaultProfiler::GetEvent(const core::ProfileData *parent,int i) const
{
	if(parent==NULL)
	{
		return root;
	}
	Timing *p=(Timing*)parent;
	int j=0;
	for(ChildMap::const_iterator it=p->children.begin();it!=p->children.end();it++,j++)
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
void DefaultProfiler::ResetMaximums()
{
//	for(Map::iterator i=timings.begin();i!=timings.end();i++)
	{
	//	i->second.max_time=0.0;
	}
}

const char *platform::core::BaseProfilingInterface::GetDebugText(core::TextStyle style) const
{
	static std::string str;
	str="";
	if(!root)
		return "";
	float total=root->time;
	str += formatLine("TOTAL", 0, total, 0.0f, style);
	for(auto i=root->children.begin();i!=root->children.end();i++)
	{
		float t=i->second->time;
		if(!i->second->updatedThisFrame)
			continue;
		str+=formatLine(i->second->unqualifiedName.c_str(),1,t,total,style);
		str += Walk(i->second, 2, i->second->time, style);
	}
	str += (style ==core::HTML)? "<br/>" : "\n";
	return str.c_str();
}

const char* platform::core::BaseProfilingInterface::GetDebugTextSimple(core::TextStyle style) const
{
	static std::string str;
	str = "";
	if (!root)
		return "";
	float total = root->time;
	str += formatLine("", 0, total, 0.0f, style); //we do force spaces within formatLine after the empty name
	return str.c_str();
}

void platform::core::BaseProfilingInterface::Clear(core::ProfileData *p)
{
	if(!p)
		p=root;
	if(!p)
		return;
	max_level=0;				// Maximum level of nesting.
	max_level_this_frame=0;
	level=0;
	for(auto i=p->children.begin();i!=p->children.end();i++)
	{
		Clear(i->second);
	}
	p->children.clear();
	delete p;
	if(p==root)
		root=nullptr;
}
