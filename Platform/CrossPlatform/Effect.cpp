#ifdef _MSC_VER
#include <Windows.h>
#endif
#include "Simul/Base/RuntimeError.h"
#include "Simul/Platform/CrossPlatform/Effect.h"
#include "Simul/Platform/CrossPlatform/Texture.h"
#include "Simul/Platform/CrossPlatform/RenderPlatform.h"
#include <iostream>

using namespace simul;
using namespace crossplatform;
using namespace std;

bool EffectPass::usesTextureSlot(int s) const
{
	if(s>=1000)
		return usesRwTextureSlot(s-1000);
	unsigned m=((unsigned)1<<(unsigned)s);
	return (textureSlots&m)!=0;
}

bool EffectPass::usesTextureSlotForSB(int s) const
{
	if(s>=1000)
		return usesRwTextureSlotForSB(s-1000);
	unsigned m=((unsigned)1<<(unsigned)s);
	return (textureSlotsForSB&m)!=0;
}

bool EffectPass::usesRwTextureSlotForSB(int s) const
{
	unsigned m=((unsigned)1<<(unsigned)s);
	return (rwTextureSlotsForSB&m)!=0;
}

bool EffectPass::usesBufferSlot(int s) const
{
	unsigned m=((unsigned)1<<(unsigned)s);
	return (bufferSlots&m)!=0;
}

bool EffectPass::usesSamplerSlot(int s) const
{
	unsigned m=((unsigned)1<<(unsigned)s);
	return true;//(samplerSlots&m)!=0;
}

bool EffectPass::usesRwTextureSlot(int s) const
{
	unsigned m=((unsigned)1<<(unsigned)s);
	return (rwTextureSlots&m)!=0;
}


bool EffectPass::usesTextures() const
{
	return (textureSlots+rwTextureSlots)!=0;
}

bool EffectPass::usesSBs() const
{
	return (textureSlotsForSB+rwTextureSlotsForSB)!=0;
}

bool EffectPass::usesBuffers() const
{
	return bufferSlots!=0;
}

bool EffectPass::usesSamplers() const
{
	return samplerSlots!=0;
}


void EffectPass::SetUsesBufferSlots(unsigned s)
{
	bufferSlots|=s;
}

void EffectPass::SetUsesTextureSlots(unsigned s)
{
	textureSlots|=s;
}

void EffectPass::SetUsesTextureSlotsForSB(unsigned s)
{
	textureSlotsForSB|=s;
}

void EffectPass::SetUsesRwTextureSlots(unsigned s)
{
	rwTextureSlots|=s;
}

void EffectPass::SetUsesRwTextureSlotsForSB(unsigned s)
{
	rwTextureSlotsForSB|=s;
}

void EffectPass::SetUsesSamplerSlots(unsigned s)
{
	samplerSlots|=s;
}

EffectTechniqueGroup::~EffectTechniqueGroup()
{
	for (crossplatform::TechniqueMap::iterator i = techniques.begin(); i != techniques.end(); i++)
	{
		delete i->second;
	}
	techniques.clear();
	charMap.clear();
}

Effect::Effect()
	:renderPlatform(NULL)
	,apply_count(0)
	,currentPass(0)
	,currentTechnique(NULL)
	,platform_effect(NULL)
{
}

Effect::~Effect()
{
	InvalidateDeviceObjects();
	SIMUL_ASSERT(apply_count==0);
}

void Effect::InvalidateDeviceObjects()
{
	shaderResources.clear();
	for (auto i = groups.begin(); i != groups.end(); i++)
	{
		delete i->second;
	}
	groups.clear();
	groupCharMap.clear();
}

EffectTechnique::EffectTechnique()
	:platform_technique(NULL)
	,should_fence_outputs(true)
{
}

EffectTechnique::~EffectTechnique()
{
}

EffectTechnique *EffectTechniqueGroup::GetTechniqueByName(const char *name)
{
	TechniqueCharMap::iterator i=charMap.find(name);
	if(i!=charMap.end())
		return i->second;
	TechniqueMap::iterator j=techniques.find(name);
	if(j==techniques.end())
		return NULL;
	charMap[name]=j->second;
	return j->second;
}

crossplatform::EffectTechnique *Effect::GetTechniqueByName(const char *name)
{
	return groupCharMap[0]->GetTechniqueByName(name);
}


EffectTechnique *EffectTechniqueGroup::GetTechniqueByIndex(int index)
{
	return techniques_by_index[index];
}

int EffectTechnique::NumPasses() const
{
	return (int)passes_by_name.size();
}

EffectPass *EffectTechnique::AddPass(const char *name,int i)
{
	EffectPass *p=new EffectPass;
	passes_by_name[name]=passes_by_index[i]=p;
	return p;
}

EffectPass *EffectTechnique::GetPass(int i)
{
	return passes_by_index[i];
}

EffectPass *EffectTechnique::GetPass(const char *name)
{
	return passes_by_name[name];
}


EffectTechniqueGroup *Effect::GetTechniqueGroupByName(const char *name)
{
	auto i=groupCharMap.find(name);
	if(i!=groupCharMap.end())
		return i->second;
	auto j=groups.find(name);
	if(j!=groups.end())
		return j->second;
	return nullptr;
}


void Effect::SetTexture(crossplatform::DeviceContext &deviceContext,crossplatform::ShaderResource &res,crossplatform::Texture *tex,int index,int mip)
{
	// If not valid, we've already put out an error message when we assigned the resource, so fail silently. Don't risk overwriting a slot.
	if(!res.valid)
		return;
	crossplatform::ContextState *cs=renderPlatform->GetContextState(deviceContext);
	unsigned long slot=res.slot;
	unsigned long dim=res.dimensions;
#ifdef _DEBUG
	if(!tex)
	{
		return;
	}
	if(!tex->IsValid())
	{
		SIMUL_BREAK_ONCE("Invalid texture applied");
		return;
	}
#endif
	crossplatform::TextureAssignment &ta=cs->textureAssignmentMap[slot];
	ta.resourceType=res.shaderResourceType;
	ta.texture=(tex&&tex->IsValid()&&res.valid)?tex:0;
	ta.dimensions=dim;
	ta.uav=false;
	ta.index=index;
	ta.mip=mip;
	cs->textureAssignmentMapValid=false;
}

void Effect::SetTexture(crossplatform::DeviceContext &deviceContext,const char *name,crossplatform::Texture *tex,int index,int mip)
{
	crossplatform::ContextState *cs=renderPlatform->GetContextState(deviceContext);
	int slot = GetSlot(name);
	if(slot<0)
	{
		SIMUL_CERR<<"Didn't find Texture "<<name<<std::endl;
		return;
	}
	int dim = GetDimensions(name);
	crossplatform::TextureAssignment &ta=cs->textureAssignmentMap[slot];
	ta.resourceType=GetResourceType(name);
	ta.texture=(tex&&tex->IsValid())?tex:0;
	ta.dimensions=dim;
	ta.uav=false;
	ta.index=index;
	ta.mip=mip;
	cs->textureAssignmentMapValid=false;
}

void Effect::SetUnorderedAccessView(crossplatform::DeviceContext &deviceContext, crossplatform::ShaderResource &res, crossplatform::Texture *tex,int index,int mip)
{
	// If not valid, we've already put out an error message when we assigned the resource, so fail silently. Don't risk overwriting a slot.
	if(!res.valid)
		return;
	crossplatform::ContextState *cs=renderPlatform->GetContextState(deviceContext);
	unsigned long slot=res.slot+1000;//1000+((combined&(0xFFFF0000))>>16);
	unsigned long dim=res.dimensions;//combined&0xFFFF;
	auto &ta=cs->textureAssignmentMap[slot];
	ta.resourceType=res.shaderResourceType;
	ta.texture=(tex&&tex->IsValid()&&res.valid)?tex:0;
	ta.dimensions=dim;
	ta.uav=true;
	ta.mip=mip;
	ta.index=index;
	cs->textureAssignmentMapValid=false;
}

void Effect::SetUnorderedAccessView(crossplatform::DeviceContext &deviceContext, const char *name, crossplatform::Texture *t,int index, int mip)
{
	const ShaderResource *i=GetTextureDetails(name);
	if(!i)
	{
		SIMUL_CERR<<"Didn't find UAV "<<name<<std::endl;
		return;
	}
	crossplatform::ContextState *cs=renderPlatform->GetContextState(deviceContext);
	// Make sure no slot clash between uav's and srv's:
	int slot = 1000+GetSlot(name);
	{
		int dim = GetDimensions(name);
		crossplatform::TextureAssignment &ta=cs->textureAssignmentMap[slot];
		ta.resourceType=GetResourceType(name);
		ta.texture=t;
		ta.dimensions=dim;

		ta.uav=true;
		ta.mip=mip;
		ta.index=index;
	}
	cs->textureAssignmentMapValid=false;
}

EffectDefineOptions simul::crossplatform::CreateDefineOptions(const char *name,const char *option1)
{
	EffectDefineOptions o;
	o.name=name;
	o.options.push_back(std::string(option1));
	return o;
}

EffectDefineOptions simul::crossplatform::CreateDefineOptions(const char *name,const char *option1,const char *option2)
{
	EffectDefineOptions o;
	o.name=name;
	o.options.push_back(std::string(option1));
	o.options.push_back(std::string(option2));
	return o;
}
EffectDefineOptions simul::crossplatform::CreateDefineOptions(const char *name,const char *option1,const char *option2,const char *option3)
{
	EffectDefineOptions o;
	o.name=name;
	o.options.push_back(std::string(option1));
	o.options.push_back(std::string(option2));
	o.options.push_back(std::string(option3));
	return o;
}

EffectTechnique *Effect::EnsureTechniqueExists(const string &groupname,const string &techname_,const string &passname)
{
	EffectTechnique *tech=NULL;
	string techname=techname_;
	{
		if(groups.find(groupname)==groups.end())
		{
			groups[groupname]=new crossplatform::EffectTechniqueGroup;
			if(groupname.length()==0)
				groupCharMap[nullptr]=groups[groupname];
		}
		crossplatform::EffectTechniqueGroup *group=groups[groupname];
		if(group->techniques.find(techname)!=group->techniques.end())
			tech=group->techniques[techname];
		else
		{
			tech								=CreateTechnique(); 
			int index							=(int)group->techniques.size();
			group->techniques[techname]			=tech;
			group->techniques_by_index[index]	=tech;
			if(groupname.size())
				techname=(groupname+"::")+techname;
			techniques[techname]			=tech;
			techniques_by_index[index]		=tech;
			tech->name						=techname;
		}
	}
	return tech;
}

const char *Effect::GetTechniqueName(const EffectTechnique *t) const
{
	for(auto g=groups.begin();g!=groups.end();g++)
	for(crossplatform::TechniqueMap::const_iterator i=g->second->techniques.begin();i!=g->second->techniques.end();i++)
	{
		if(i->second==t)
			return i->first.c_str();
	}
	return "";
}

void Effect::Apply(DeviceContext &deviceContext,const char *tech_name,const char *pass)
{
	Apply(deviceContext,GetTechniqueByName(tech_name),pass);
}

void Effect::Apply(DeviceContext &deviceContext,const char *tech_name,int pass)
{
	Apply(deviceContext,GetTechniqueByName(tech_name),pass);
}


void Effect::Apply(crossplatform::DeviceContext &deviceContext,crossplatform::EffectTechnique *effectTechnique,int pass_num)
{
	currentTechnique				=effectTechnique;
	if(effectTechnique)
	{
		EffectPass *p				=(effectTechnique)->GetPass(pass_num>=0?pass_num:0);
		if(p)
		{
			crossplatform::ContextState *cs=renderPlatform->GetContextState(deviceContext);
			cs->invalidate();
			cs->currentEffectPass=p;
			cs->currentTechnique=effectTechnique;
			cs->currentEffect=this;
		}
		else
			SIMUL_BREAK("No pass found");
	}
	currentPass=pass_num;
}


void Effect::Apply(crossplatform::DeviceContext &deviceContext,crossplatform::EffectTechnique *effectTechnique,const char *passname)
{
	currentTechnique				=effectTechnique;
	if (effectTechnique)
	{
		EffectPass *p = NULL;
		if(passname)
			p=effectTechnique->GetPass(passname);
		else
			p=effectTechnique->GetPass(0);
		if(p)
		{
			crossplatform::ContextState *cs=renderPlatform->GetContextState(deviceContext);
			cs->invalidate();
			cs->currentTechnique=effectTechnique;
			cs->currentEffectPass=p;
			cs->currentEffect=this;
		}
		else
			SIMUL_BREAK("No pass found");
	}
}

void Effect::StoreConstantBufferLink(crossplatform::ConstantBufferBase *b)
{
	linkedConstantBuffers.insert(b);
}

bool Effect::IsLinkedToConstantBuffer(crossplatform::ConstantBufferBase*b) const
{
	return (linkedConstantBuffers.find(b)!=linkedConstantBuffers.end());
}


const ShaderResource *Effect::GetTextureDetails(const char *name)
{
	auto j=textureCharMap.find(name);
	if(j!=textureCharMap.end())
		return j->second;
	for(auto i:textureDetailsMap)
	{
		if(strcmp(i.first.c_str(),name)==0)
		{
			textureCharMap[name]=i.second;
			return i.second;
		}
	}
	return nullptr;
}

int Effect::GetSlot(const char *name)
{
	const ShaderResource *i=GetTextureDetails(name);
	if(!i)
		return -1;
	return i->slot;
}

int Effect::GetSamplerStateSlot(const char *name)
{
	auto i=samplerStates.find(std::string(name));
	if(i==samplerStates.end())
		return -1;
	return i->second->default_slot;
}

int Effect::GetDimensions(const char *name)
{
	const ShaderResource *i=GetTextureDetails(name);
	if(!i)
		return -1;
	return i->dimensions;
}

crossplatform::ShaderResourceType Effect::GetResourceType(const char *name)
{
	const ShaderResource *i=GetTextureDetails(name);
	if(!i)
		return crossplatform::ShaderResourceType::COUNT;
	return i->shaderResourceType;
}

void Effect::UnbindTextures(crossplatform::DeviceContext &deviceContext)
{
	if(!renderPlatform)
		return;
	crossplatform::ContextState *cs=renderPlatform->GetContextState(deviceContext);
	cs->textureAssignmentMap.clear();
	cs->samplerStateOverrides.clear();
	cs->applyBuffers.clear();  //Because we might otherwise have invalid data
	cs->applyVertexBuffers.clear();
	cs->invalidate();
}