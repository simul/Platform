#include <map>
#include <string>
#include <cstring>
#include <sstream>
#include <cstdio>
#include <cassert>
#include <fstream>
#include <set>
#include <regex>
#include <map>

#ifndef _MSC_VER
typedef int errno_t;
#include <errno.h>
#endif
#include <iostream>

#include "Sfx.h"
#include "SfxClasses.h"
#include "Sfx.h"
#include "SfxEffect.h"

#ifdef _MSC_VER
#define YY_NO_UNISTD_H
#endif
#include "GeneratedFiles/SfxScanner.h"
#include "SfxProgram.h"
#include "StringFunctions.h"
#include "Compiler.h"
#include "SfxErrorCheck.h"

using namespace std;
extern bool IsRW(ShaderResourceType);

void process_member_decl(string &str,string &memberDeclaration)
{
	// in square brackets [] is the definition for ONE member.
	std::regex re_member("\\[(.*)\\]");
	std::smatch match;
	if(std::regex_search(str,match,re_member))
	{
		memberDeclaration=match[1].str();
		str.replace(match[0].first,match[0].first+match[0].length(),"{members}");
	}
}

bool ShaderInstanceHasSemanic(ShaderInstance* shaderInstance, const char* semantic)
{
	bool found = false;
	Function* function = gEffect->GetFunction(shaderInstance->m_functionName, 0);
	const std::map<std::string, Declaration*>& declarations = gEffect->GetDeclarations();
	for (const auto& parameter : function->parameters)
	{
		if (!parameter.semantic.empty())
		{
			found |= parameter.semantic.compare(semantic) == 0;
		}
		else if (declarations.find(parameter.type) != declarations.end())
		{
			Declaration* declaration = declarations.at(parameter.type);
			if (declaration->declarationType == DeclarationType::STRUCT)
			{
				Struct* structure = reinterpret_cast<Struct*>(declaration);
				for (const auto& member : structure->m_structMembers)
				{
					found |= member.semantic.compare(semantic) == 0;
				}
			}
		}

	}
	return found;
}

Effect::Effect()
	: m_includes(0)
	, m_active(true)
	, last_filenumber(0)
	, current_texture_number(0)
	, current_rw_texture_number(0)
	, m_max_sampler_register_number(0)
{
}

Effect::~Effect()
{
	for(map<string,Pass*>::iterator it=m_programs.begin(); it!=m_programs.end(); ++it)
		delete it->second;
	for(auto i:m_declaredFunctions)
	{
		for(auto j:i.second)
			delete j;
	}
}

Function* Effect::DeclareFunction(const std::string &functionName, Function &buildFunction)
{
	Function* f = new Function;
	*f		  = buildFunction;

	if (sfxConfig.combineTexturesSamplers)
	{
		// Remove texture templates:
		for (auto& p : f->parameters)
		{
			if (!p.templ.empty() &&
				(p.shaderResourceType & ShaderResourceType::TEXTURE) == ShaderResourceType::TEXTURE)
			{
				find_and_replace(f->declaration, p.type+ "<" + p.templ + ">", CombinedTypeString(p.type, p.templ));
			}
		}
		// Checks
		if (!sfxConfig.combineInShader)
		{
			// If its not global (it's a param) add array:
			for (auto& p : f->parameters)
			{
				if ((p.shaderResourceType & ShaderResourceType::TEXTURE) == ShaderResourceType::TEXTURE)
				{
					bool found = false;
					for (auto& g : f->globals)
					{
						if (g == p.identifier)
						{
							found = true;
							break;
						}
					}
					if (!found)
					{
						// 24 is the maximum number of textures
						find_and_replace(f->declaration, p.identifier, p.identifier + "[24]");
					}
				}
			} 
			find_and_replace(f->declaration, "Texture2D ",		  "uint64_t ");
			find_and_replace(f->declaration, "Texture2DArray ",	 "uint64_t ");
			find_and_replace(f->declaration, "Texture3D ",		  "uint64_t ");
			find_and_replace(f->declaration, "Texture3D ",		  "uint64_t ");
			find_and_replace(f->declaration, "TextureCube ",		"uint64_t ");
			find_and_replace(f->declaration, "TextureCubeArray ",	"uint64_t ");
			find_and_replace(f->declaration, "Texture2DMS ",		"uint64_t ");
		}
	}

	m_declaredFunctions[functionName].push_back(f);
	return f;
}

void Effect::AccumulateFunctionsUsed(const Function *f,std::set<const Function *> &s) const
{
	s.insert(f);
	for(auto u=f->functionsCalled.begin();u!=f->functionsCalled.end();u++)
	{
		AccumulateFunctionsUsed(*u,s);
	}
}

const std::set<std::string> &sfx::Function::GetTypesUsed() const
{
	if(!initialized)
	{
		initialized=true;
		types_used.clear();
		if(returnType!="void")
			types_used.insert(returnType);
		for (auto i : constantBuffers)
		{
			types_used.insert(i->structureType);
		}
		for(auto i:parameters)
		{
			types_used.insert(i.type);
		}
		for(auto i:locals)
		{
			types_used.insert(i.type);
		}
		for(auto i:globals)
		{
			Declaration *d=gEffect->GetDeclaration(i);
			if(!d)
				continue;
			switch(d->declarationType)
			{
			case DeclarationType::TEXTURE:
			{
				DeclaredTexture *dt=(DeclaredTexture*)d;
				// Is is something like RWStructuredBuffer<StructureType>..?
				// Then we "use" StructureType and will need its declaration.
				if(dt->structureType.length())
				{
					types_used.insert(dt->structureType);
				}
			}
				break;
			case DeclarationType::NAMED_CONSTANT_BUFFER:
			{
				NamedConstantBuffer*dt=static_cast<NamedConstantBuffer*>(d);
				// Is is something like ConstantBuffer<StructureType>..?
				if(dt->structureType.length())
				{
					types_used.insert(dt->structureType);
				}
				Struct* st = static_cast<Struct*>(d);
				for (auto s : st->m_structMembers)
				{
					types_used.insert(s.type);
				}
			}
				break;
			case DeclarationType::STRUCT:
			{
				Struct *st=(Struct*)d;
				for(auto s:st->m_structMembers)
				{
					types_used.insert(s.type);
				}
			}
			default:
				break;
			}
		}
	}

	return types_used;
}
void Effect::AccumulateStructDeclarations(std::set<const Declaration *> &s,std::string i) const
{
	// Is the type declared?
	auto decl=declarations.find(i);
	if(decl==declarations.end())
		return;
	
	const Declaration *d=decl->second;
	if(d)
	switch(d->declarationType)
	{
	case DeclarationType::CONSTANT_BUFFER:
	{
		const ConstantBuffer* st = (const ConstantBuffer*)d;
		s.insert(d);
		for (int j = 0; j < st->m_structMembers.size(); j++)
		{
			AccumulateStructDeclarations(s, st->m_structMembers[j].type);
		}
	}
	case DeclarationType::STRUCT:
	{
		const Struct *st=(const Struct*)d;
		s.insert(d);
		for(int j=0;j<st->m_structMembers.size();j++)
		{
			AccumulateStructDeclarations(s,st->m_structMembers[j].type);
		}
	}
	default:
		break;
	}
	
}

void Effect::AccumulateDeclarationsUsed(const Function *f,set<const Declaration *> &s,std::set<std::string>& rwLoad) const
{
	// Accumulate rw textures used in load operations:
	for (const auto& l : f->rwTexturesLoaded)
	{
		rwLoad.insert(l);
	}
	for (auto u = f->globals.begin(); u != f->globals.end(); u++)
	{
		auto v=declarations.find(*u);
		// Probably member of an anonymous constant buffer, so don't fail. Should find which buffer?
		if(v==declarations.end())
			continue;
		v->second->name = v->first;
		const Declaration *d=v->second;
		s.insert(d);
	}
	const auto types_used=f->GetTypesUsed();
	for(auto i:types_used)
	{
		AccumulateStructDeclarations(s,i);
	}
	for(auto u=f->functionsCalled.begin();u!=f->functionsCalled.end();u++)
	{
		AccumulateDeclarationsUsed(*u,s,rwLoad);
	}
}

void Effect::AccumulateGlobalsAsStrings(const Function* f, std::set<std::string>& s) const
{
	if(!f)
		return;
	for (auto i : f->globals)
	{
		s.insert(i);
	}
	for (auto u = f->functionsCalled.begin(); u != f->functionsCalled.end(); u++)
	{
		AccumulateGlobalsAsStrings(*u, s);
	}
}

void Effect::AccumulateGlobals(const Function *f,std::set<const Variable *> &s) const
{
	for (auto i : f->declarations)
	{
		auto d= declarations.find(i);
		if(d!=declarations.end()&&d->second->declarationType==DeclarationType::VARIABLE)
		{
			const Variable*D=(const Variable*)d->second;
			s.insert(D);
		}
	}
	for(auto u=f->functionsCalled.begin();u!=f->functionsCalled.end();u++)
	{
		AccumulateGlobals(*u,s);
	}
}

void Effect::AccumulateConstantBuffersUsed(const Function *f, std::set<ConstantBuffer*> &s) const
{
	for (auto i : f->constantBuffers)
		s.insert(i);
	for (auto u = f->functionsCalled.begin(); u != f->functionsCalled.end(); u++)
	{
		AccumulateConstantBuffersUsed(*u, s);
	}
}

void Effect::DeclareStruct(const string &name,const Struct &ts,const string &original)
{
	Struct *rs					=new Struct;
	*rs							=ts;
	rs->name=name;
	rs->original=original;
	m_structs[name]	=rs;
	declarations[name]=rs;
}

void Effect::DeclareConstantBuffer(const std::string &name, int slot, const Struct &ts,const string &original)
{
	ConstantBuffer *cb = new ConstantBuffer;
	m_constantBuffers[name] = cb;
	*(static_cast<Struct*>(cb)) = ts;
	cb->original=original;
	cb->declarationType=DeclarationType::CONSTANT_BUFFER;
	cb->slot = slot;
	cb->name=name;
	cb->structureType = name;
	declarations[name]=cb;
}

void Effect::DeclareVariable(const Variable* v)
{
	Variable *variable=new Variable;
	*variable=*v;
	declarations[variable->name] = variable;
}

Function *Effect::GetFunction(const std::string &functionName,int i)
{
	auto f=m_declaredFunctions.find(functionName);
	if(f==m_declaredFunctions.end())
		return nullptr;
	if(i<0||i>=f->second.size())
		return nullptr;
	return f->second[i];
}

int Effect::GenerateSamplerSlot(int s,bool offset)
{
	const SfxConfig *config					=GetConfig();
	if(offset&&!config->sharedSlots)
		return s+300;
	return s;
}
int Effect::GenerateTextureSlot(int s,bool offset)
{
	const SfxConfig *config					=GetConfig();
	if(offset&&!config->sharedSlots)
		return s+100;
	return s;
}
int Effect::GenerateTextureWriteSlot(int s,bool offset)
{
	const SfxConfig *config					=GetConfig();
	if(offset&&!config->sharedSlots)
		return s+200;
	return s;
}
int Effect::GenerateConstantBufferSlot(int s,bool offset)
{
	const SfxConfig *config					=GetConfig();
	if(offset&&!config->sharedSlots)
		return s;
	return s;
}

int Effect::GetSlot(const std::string &variableName) const
{
	auto i=declarations.find(variableName);
	if(i!=declarations.end())
	{
		if(i->second->declarationType==DeclarationType::TEXTURE)
		{
			DeclaredTexture *dt=static_cast<DeclaredTexture*>(i->second);
			return dt->slot;
		}
		else if(i->second->declarationType==DeclarationType::SAMPLER)
		{
			SamplerState *ss=static_cast<SamplerState*>(i->second);
			return ss->register_number;
		}
	}
	return -1;
}

std::string Effect::GetFilenameOrNumber(const std::string &fn) 
{
	if (sfxConfig.lineDirectiveFilenames)
		return "\""+fn+"\"";
	return stringFormat("%d",GetFilenumber(fn));
}

std::string Effect::GetFilenameOrNumber(int filenumber)
{
	string fn;
	if (sfxConfig.lineDirectiveFilenames)
	{
		fn=fileList[filenumber];
		return "\""+fn+"\"";
	}
	return stringFormat("%d",filenumber);
}

std::string Effect::GetFilename(int filenumber)
{
	string fn=fileList[filenumber];
	return fn;
}

int Effect::GetFilenumber(const std::string &fn) 
{
	auto i=filenumbers.find(fn);
	if(i!=filenumbers.end())
		return i->second;
	last_filenumber++;
	filenumbers[fn]=last_filenumber;
	fileList[last_filenumber]=fn;
	return last_filenumber;
}

bool& Effect::Active()
{
	return m_active;
}

void Effect::SetConfig(const SfxConfig *config)
{
	if(config)
		sfxConfig=*config;
	else
		sfxConfig=SfxConfig();
}; 

void Effect::SetOptions(const SfxOptions *opts)
{
	if(opts)
		sfxOptions=*opts;
	else
		sfxOptions=SfxOptions();
};

string& Effect::Filename()
{
	return m_filename;
}

string& Effect::Dir()
{
	return m_dir;
}

unsigned Effect::CompileAllShaders(string sfxoFilename,const string &sharedCode,string& log, BinaryMap &binaryMap)
{
	ostringstream sLog;
	int res=1;
	PixelOutputFormat pixelOutputFormat=FMT_UNKNOWN;
	std::ofstream combinedBinary;
	if (sfxOptions.wrapOutput)
	{
		std::string sfxbFilename = sfxoFilename;
		find_and_replace(sfxbFilename, ".sfxo", ".sfxb");
		combinedBinary.open(sfxbFilename, std::ios_base::binary);
	}
	for(ShaderInstanceMap::iterator i=m_shaderInstances.begin();i!=m_shaderInstances.end();i++)
	{
	// Hold on, is this even supported?
		std::string profile_text=i->second->m_profile;
		find_and_replace(profile_text, "ps_", "");
		find_and_replace(profile_text, "vs_", "");
		find_and_replace(profile_text, "cs_", "");
		find_and_replace(profile_text, "_", ".");
		double profile_number=atof(profile_text.c_str());
		if(profile_number>sfxConfig.maxShaderModel)
		{
			std::cout<<"Skipping "<<i->first.c_str()<<" as profile "<<i->second->m_profile.c_str()<<" is not supported.\n";
			continue;
		}
		ConstructSource(i->second);
		if(i->second->shaderType==FRAGMENT_SHADER)
		{
			if(sfxConfig.multiplePixelOutputFormats)
			{
				// TO-DO: implement this better, we could just set before hand the rt state name 
				// to the pass
				const Declaration* rtFormat = nullptr;
				for (map<string, TechniqueGroup>::const_iterator g = m_techniqueGroups.begin(); g != m_techniqueGroups.end(); ++g)
				{
					for (map<string, Technique*>::const_iterator it = g->second.m_techniques.begin(); it != g->second.m_techniques.end(); ++it)
					{
						const vector<Pass> &passes = it->second->GetPasses();
						vector< Pass>::const_iterator j = passes.begin();
						for (; j != passes.end(); j++)
						{
							const Pass *pass = &(*j);
							if (pass->passState.renderTargetFormatState.objectName.empty())
							{
								continue;
							}
							if (pass->HasShader(ShaderType::FRAGMENT_SHADER) && pass->GetShader(ShaderType::FRAGMENT_SHADER) == i->first)
							{
								// We found a rt format state!
								rtFormat = declarations[pass->passState.renderTargetFormatState.objectName];
								// We cache the name so when we save to the SFXO we don't have to search it again
								i->second->rtFormatStateName = rtFormat->name;
								break;
							}
						}
						if (rtFormat) { break; }
					}
					if (rtFormat) { break; }
				}
				
				// Just output generic formats that we may use
				if (!rtFormat)
				{
					res&=Compile(i->second,Filename(),sfxoFilename,i->second->shaderType,FMT_32_ABGR,sharedCode, sLog,sfxConfig,sfxOptions,fileList ,combinedBinary,binaryMap);
					res&=Compile(i->second,Filename(),sfxoFilename,i->second->shaderType,FMT_FP16_ABGR,sharedCode, sLog,sfxConfig,sfxOptions, fileList, combinedBinary, binaryMap);
					res&=Compile(i->second,Filename(),sfxoFilename,i->second->shaderType,FMT_UNORM16_ABGR,sharedCode, sLog,sfxConfig,sfxOptions, fileList, combinedBinary, binaryMap);
					res&=Compile(i->second,Filename(),sfxoFilename,i->second->shaderType,FMT_SNORM16_ABGR,sharedCode, sLog,sfxConfig,sfxOptions, fileList, combinedBinary, binaryMap);
				}
				// We know the output format
				else
				{
					res&=Compile(i->second,Filename(),sfxoFilename,i->second->shaderType,FMT_UNKNOWN,sharedCode,sLog,sfxConfig,sfxOptions, fileList, combinedBinary, binaryMap,rtFormat);
				}
			}
			else
			{
				res&=Compile(i->second,Filename(),sfxoFilename,i->second->shaderType,FMT_UNKNOWN,sharedCode, sLog,sfxConfig,sfxOptions, fileList, combinedBinary, binaryMap);
			}
			if(!res)
				return 0;
		}
		else if(i->second->shaderType==VERTEX_SHADER)
		{
			res&=Compile(i->second,Filename(),sfxoFilename,VERTEX_SHADER,pixelOutputFormat,sharedCode, sLog,sfxConfig,sfxOptions, fileList, combinedBinary, binaryMap);
			if(!res)
				return 0;
			res&=Compile(i->second,Filename(),sfxoFilename,EXPORT_SHADER,pixelOutputFormat,sharedCode, sLog,sfxConfig,sfxOptions, fileList, combinedBinary, binaryMap);
			if(!res)
				return 0;
		}
		else if(i->second->shaderType==UNKNOWN_SHADER_TYPE)
		{
			std::cerr<<"Warning: shader is not used "<<i->first<<std::endl;
		}
		else
		{
			res&=Compile(i->second,Filename(),sfxoFilename,i->second->shaderType,pixelOutputFormat,sharedCode, sLog,sfxConfig,sfxOptions, fileList, combinedBinary, binaryMap);
			if(!res)
				return 0;
		}
	}
	log=sLog.str();
	return res;
}

ostringstream& Effect::Log()
{
	return m_log;
}

TechniqueGroup &Effect::GetTechniqueGroup(const std::string &name)
{
	return m_techniqueGroups[name];
}

const vector<string>& Effect::GetProgramList() const
{
	return m_programNames;
}

const vector<string>& Effect::GetFilenameList() const
{
	return m_filenames;
}


void Effect::SetFilenameList(const char **filenamesUtf8)
{
	m_filenames.clear();
	const char **f=filenamesUtf8;
	while(*f!=NULL)
	{
		string str=*f;
		int pos=(int)str.find("\\");
		while(pos>0)
		{
			str.replace(str.begin()+pos,str.begin()+pos+1,"/");
			pos=(int)str.find("\\");
		}
		m_filenames.push_back(str);
		f++;
	}
}

void Effect::PopulateProgramList()
{
	m_programNames.clear();
	for(map<string,Pass*>::const_iterator it=m_programs.begin(); it!=m_programs.end(); ++it)
		m_programNames.push_back(it->first);
}

string stringOf(ShaderCommand t)
{
	switch(t)
	{
	case SetVertexShader:
		return "vertex";
	case SetHullShader:
		return "hull";
	case SetDomainShader:
		return "domain";
	case SetGeometryShader:
		return "geometry";
	case SetPixelShader:
		return "pixel";
	case SetComputeShader:
		return "compute";
	case SetRayGenerationShader:
		return "raygeneration";
	case SetAnyHitShader:
		return "anyhit";
	case SetMissShader:
		return "miss";
	case SetClosestHitShader:
		return "closesthit";
	case SetCallableShader:
		return "callable";
	case SetIntersectionShader:
		return "intersection";
	case SetExportShader:
		return "export";
	default:
		return "";
	};
}

string stringOf(Topology t)
{
	switch(t)
	{		
		case POINTLIST:	
			return "PointList";
		case LINELIST:
			return "LineList";
		case LINESTRIP:
			return "LineStrip";
		case TRIANGLELIST:
			return "TriangleList";
		case TRIANGLESTRIP:
			return "TriangleStrip";
		case LINELIST_ADJACENCY:
			return "LineListAdjacency";
		case LINESTRIP_ADJACENCY:
			return "LineStripAdjacency";
		case TRIANGLELIST_ADJACENCY:
			return "TriangleListAdjacency";
		case TRIANGLESTRIP_ADJACENCY:
			return "TriangleStripAdjacency";
	default:
		break;
	};
	return "undefined";
}
extern string ToString(PixelOutputFormat pixelOutputFormat);

void Effect::CalculateResourceSlots(ShaderInstance *shaderInstance,set<int> &textureSlots,set<int> &uavSlots,set<int> &textureSlotsForSB,set<int> &uavTextureSlotsForSB,set<int> &constantBufferSlots,set<int> &samplerSlots)
{
	// Note there may be some API-dependence in how we do this.
	for(auto d:shaderInstance->declarations)
	{
		if(d->declarationType==DeclarationType::TEXTURE)
		{
			DeclaredTexture *td=(DeclaredTexture *)(d);
			int slot=td->slot;
			if(IsTexture(td->shaderResourceType))
			{
				if(IsRW(td->shaderResourceType))
					uavSlots.insert(slot);
				else
					textureSlots.insert(slot);
			}
			else if(IsStructuredBuffer(td->shaderResourceType))
			{
				if(IsRWStructuredBuffer(td->shaderResourceType))
					uavTextureSlotsForSB.insert(slot);
				else
					textureSlotsForSB.insert(slot);
			}
			else if(td->shaderResourceType==ShaderResourceType::RAYTRACE_ACCELERATION_STRUCT)
			{
				textureSlots.insert(slot);	
			}
			else
				std::cerr<<"Warning: unknown resource type "<<(int)td->shaderResourceType<<std::endl;
		}
		else if(d->declarationType==DeclarationType::SAMPLER)
		{
			SamplerState *ss=(SamplerState *)(d);
			samplerSlots.insert(ss->register_number);
		}
		else if(d->declarationType==DeclarationType::CONSTANT_BUFFER)
		{
			constantBufferSlots.insert(d->slot);
		}
		else if (d->declarationType == DeclarationType::NAMED_CONSTANT_BUFFER)
		{
			constantBufferSlots.insert(d->slot);
		}
		else
		{
			std::cerr<< GetFilenameOrNumber(d->file_number)<<"("<<d->line_number<<")" <<": Unhandled resource "<<d->name.c_str()<<" of type "<<(int)d->declarationType<<"."<<std::endl;
		}
	}
	// older-style anonymous constant buffers.
	for (auto c : shaderInstance->constantBuffers)
	{
		constantBufferSlots.insert(c->slot);
	}
}

ShaderInstance *Effect::AddShaderInstance(const std::string &shaderInstanceName,const std::string &functionName,ShaderType type,const std::string &profileName,int lineno)
{
	auto i=m_shaderInstances.find(shaderInstanceName);
	if(i!=m_shaderInstances.end())
		return i->second;
	Function *function=gEffect->GetFunction(functionName,0);
	if(!function)
		return nullptr;
	std::set<Declaration *> declarations;
	std::set<string> globals;
	gEffect->AccumulateGlobalsAsStrings(function,globals);
	// find out what slots the function uses by interrogating its global variables.
	// At this point we're not interested in params, just globals.
	for(auto i:globals)
	{
		string type=gEffect->GetDeclaredType(i);
		auto d=gEffect->GetDeclarations().find(i);
		if (d==gEffect->GetDeclarations().end())
			continue;
		Declaration *dec=d->second;
		declarations.insert(dec);
	}

	ShaderInstance *shaderInstance=new ShaderInstance(declarations);
	m_shaderInstances[shaderInstanceName]	=shaderInstance;
	
	if(GetConfig()->entryPointOption.length()>0)
		shaderInstance->entryPoint					=functionName;
	else
		shaderInstance->entryPoint					="main";
	shaderInstance->m_functionName					=functionName;
	shaderInstance->shaderType						=type;
	shaderInstance->m_profile						=profileName;
	shaderInstance->global_line_number				=lineno;

	for(auto d:declarations)
	{
		d->ref_count++;
	}
	AccumulateConstantBuffersUsed(function, shaderInstance->constantBuffers);
	return shaderInstance;
}

std::size_t hash(const char *s)
{
	size_t len = strlen(s);
	//s[0] * (31 ^ (n - 1) + s[1] * 31 ^ (n - 2) + ... + s[n - 1]
	std::size_t h1 = std::hash<std::string>{}(s);
	return h1;
}

ShaderInstance *Effect::GetShaderInstance(const std::string &instanceName,ShaderType type)
{
	auto s=m_shaderInstances.find(instanceName);
	if(s!=m_shaderInstances.end())
	{
		if(s->second->shaderType==UNKNOWN_SHADER_TYPE||type==UNKNOWN_SHADER_TYPE||type==s->second->shaderType)
		{	
			if(s->second->shaderType==UNKNOWN_SHADER_TYPE&&type!=UNKNOWN_SHADER_TYPE)
				s->second->shaderType=type;
			return s->second;
		}
		else
		{
			std::string instanceName1=instanceName+stringOf((ShaderCommand)type);
			return AddShaderInstance(instanceName1,s->second->m_functionName,type,s->second->m_profile,s->second->global_line_number);
		}
	}
	std::string functionName=instanceName;
	Function *function=gEffect->GetFunction(functionName,0);
	if(!function)
		return nullptr;
	return AddShaderInstance(instanceName,functionName,type,"",function->main_linenumber);
}

bool Effect::Save(string sfxFilename,string sfxoFilename)
{
	PopulateProgramList();
	string log;
	std::set<int> usedTextureSlots;
	std::set<int> rwTextureSlots;
	std::set<int> usedSamplerSlots;
	std::set<int> usedConstantBufferSlots;

	// Write used declarations:
	for(auto t:declarations)
	{
		// t.second->ref_count;
		if(t.second->declarationType==DeclarationType::TEXTURE)
		{
			DeclaredTexture *dt=(DeclaredTexture*)t.second;
			if (IsRW(dt->shaderResourceType))
			{
				rwTextureSlots.insert(dt->slot);
			}
			else
			{
				usedTextureSlots.insert(dt->slot);
			}
		}
		else if(t.second->declarationType==DeclarationType::SAMPLER)
		{
			SamplerState *ss=(SamplerState*)t.second;
			usedSamplerSlots.insert(ss->register_number);
		}

	}
	string sharedCode=m_sharedCode.str();
	for(auto t:declarations)
	{
		if(t.second->declarationType==DeclarationType::TEXTURE)
		{
			DeclaredTexture *dt=(DeclaredTexture*)t.second;
			bool writeable=IsRW(dt->shaderResourceType);
			string type=string(writeable?"u":"t");
			if(t.second->ref_count>0)
			{
				if(dt->slot<0)
				{
					dt->slot=0;
					while(dt->slot<32)
					{
						if(writeable)
						{
							if(rwTextureSlots.find(dt->slot)==rwTextureSlots.end())
							{
								rwTextureSlots.insert(dt->slot);
								break;
							}
						}
						else
						{	
							if(usedTextureSlots.find(dt->slot)==usedTextureSlots.end())
							{
								usedTextureSlots.insert(dt->slot);
								break;
							}
						}
						dt->slot++;
					}
				}
				find_and_replace(sharedCode,type+string("####")+t.first+"####",type+std::to_string(dt->slot));
			}
			else
			{
				find_and_replace(sharedCode,string(": register(")+type+string("####")+t.first+"####)","");
			}
		}
		else if(t.second->declarationType==DeclarationType::SAMPLER)
		{
			if(!t.second->ref_count)
				continue;
			SamplerState *ss=(SamplerState*)t.second;
			if(ss->register_number<0)
			{
				ss->register_number=0;
				while(ss->register_number<32)
				{
					if(usedSamplerSlots.find(ss->register_number)==usedSamplerSlots.end())
					{
						usedSamplerSlots.insert(ss->register_number);
						break;
					}
					ss->register_number++;
				}
			}
			string type="s";
			find_and_replace(sharedCode,type+string("####")+t.first+"####",type+std::to_string(ss->register_number));
		}
	}
	BinaryMap binaryMap;
	int res			=CompileAllShaders(sfxoFilename,sharedCode,log, binaryMap);
	if(!res)
		return 0;
	// Now we will write a sfxo definition file that enumerates all the techniques and their shader filenames.
	ofstream outstr(sfxoFilename);
	outstr<<"SFX:"<<sfxConfig.api<<"\n";

	for (auto b : m_constantBuffers)
	{
		outstr << "constant_buffer " << b.first << " "<<GenerateConstantBufferSlot(b.second->slot,false)<<std::endl;
		usedConstantBufferSlots.insert(b.second->slot);
	}

	vector<std::string>  m_techniqueNames;
	// Add textures to the effect file
	for (auto t = declarations.begin(); t != declarations.end(); ++t)
	{
		if (!t->second->ref_count)
			continue;
		if (t->second->declarationType == DeclarationType::TEXTURE)
		{
			DeclaredTexture *dt = (DeclaredTexture*)t->second;
			bool writeable = IsRW(dt->shaderResourceType);
			bool is_array = IsArrayTexture(dt->shaderResourceType);
			bool is_cubemap = IsCubemap(dt->shaderResourceType);
			bool is_msaa = IsMSAATexture(dt->shaderResourceType);
			int dimensions = GetTextureDimension(dt->shaderResourceType, true);
			string rw = writeable ? "read_write" : "read_only";
			string ar = is_array ? "array" : "single";
			if(dt->shaderResourceType==ShaderResourceType::RAYTRACE_ACCELERATION_STRUCT)
			{
				outstr<< "accelerationstructure ";
			}
			else
			{
				outstr<< "texture ";
			}
			outstr << t->first << " ";
		
			if (is_cubemap)
				outstr << "cubemap";
			else
				outstr << dimensions << "d";
			if (is_msaa)
				outstr << "ms";
		
			outstr << " " << rw << " " << (writeable?GenerateTextureWriteSlot(dt->slot,false):GenerateTextureSlot(dt->slot,false))<< " " << ar << std::endl;
			if (dt->slot >= 32)
			{
				std::cerr << sfxFilename.c_str() << "(0): error: by default, only 16 texture slots are enabled in Gnmx." << std::endl;
				std::cerr << sfxoFilename.c_str() << "(0): warning: See output." << std::endl;
				exit(31);
			}
		}
	}
	// Named constant buffers.
	for (auto t = declarations.begin(); t != declarations.end(); ++t)
	{
		if (t->second->declarationType != DeclarationType::NAMED_CONSTANT_BUFFER)
			continue;
		if (!t->second->ref_count)
			continue;
		NamedConstantBuffer *dt = static_cast<NamedConstantBuffer*>(t->second);
		outstr << "namedconstantbuffer " << t->first << " ";
		outstr << " " <<dt->slot<< std::endl;
	}
	// Add samplers to the effect file
	for (auto t = declarations.begin(); t != declarations.end(); ++t)
	{
		if (!t->second->ref_count)
			continue;
		if (t->second->declarationType != DeclarationType::SAMPLER)
			continue;
		SamplerState *ss = (SamplerState *)t->second;
		outstr << "SamplerState " << t->first << " "
			<< GenerateSamplerSlot(ss->register_number,false)
			<< "," << ToString(ss->Filter)
			<< "," << ToString(ss->AddressU)
			<< "," << ToString(ss->AddressV)
			<< "," << ToString(ss->AddressW)
			<< "," << ToString(ss->depthComparison)
			<< "," << "\n";
	}
	
	// Add the render target format states to the effect file
	for (auto t = declarations.begin(); t != declarations.end(); ++t)
	{
		if (!t->second->ref_count)
			continue;
		if (t->second->declarationType != DeclarationType::RENDERTARGETFORMAT_STATE)
			continue;
		RenderTargetFormatState* s = (RenderTargetFormatState *)t->second;
		outstr << "RenderTargetFormatState " << t->first << " (";
		for (int i = 0; i < 8; i++)
		{
			outstr << s->formats[i];
			if (i < 7)
			{
				outstr << ",";
			}
		}
		outstr << std::dec << ")" << "\n";
	}
	// Add rasterizer states to the effect file
	for(auto t=declarations.begin();t!=declarations.end();++t)
	{
		if(!t->second->ref_count)
			continue;
		if(t->second->declarationType!=DeclarationType::RASTERIZERSTATE)
			continue;
		RasterizerState *b=(RasterizerState *)t->second;
		outstr<<"RasterizerState "<<t->first<<" ("
			<<ToString(b->AntialiasedLineEnable)
			<<","<<ToString(b->cullMode)
			<<","<<ToString(b->DepthBias)
			<<","<<ToString(b->DepthBiasClamp)
			<<","<<ToString(b->DepthClipEnable)
			<<","<<ToString(b->fillMode)
			<<","<<ToString(b->FrontCounterClockwise)
			<<","<<ToString(b->MultisampleEnable)
			<<","<<ToString(b->ScissorEnable)
			<<","<<ToString(b->SlopeScaledDepthBias)
			;
		outstr<<std::dec<<")"<<"\n";
	}
	// Add blend states to the effect file
	for(auto t=declarations.begin();t!=declarations.end();++t)
	{
		if(!t->second->ref_count)
			continue;
		if(t->second->declarationType!=DeclarationType::BLENDSTATE)
			continue;
		BlendState *b=(BlendState *)t->second;
		outstr<<"BlendState "<<t->first<<" "
			<<ToString(b->AlphaToCoverageEnable)
			<<",(";
		for(auto u=b->BlendEnable.begin();u!=b->BlendEnable.end();u++)
		{
			if(u!=b->BlendEnable.begin())
				outstr<<",";
			outstr<<ToString(u->second);
		}
		outstr<<"),"<<ToString(b->BlendOp)
			<<","<<ToString(b->BlendOpAlpha)
			<<","<<ToString(b->SrcBlend)
			<<","<<ToString(b->DestBlend)
			<<","<<ToString(b->SrcBlendAlpha)
			<<","<<ToString(b->DestBlendAlpha)<<",("<<std::hex;
		for(auto v=b->RenderTargetWriteMask.begin();v!=b->RenderTargetWriteMask.end();v++)
		{
			if(v!=b->RenderTargetWriteMask.begin())
				outstr<<",";
			outstr<<ToString(v->second);
		}
		outstr<<std::dec<<")"<<"\n";
	}
	// Add depth states to the effect file
	for(auto t=declarations.begin();t!=declarations.end();++t)
	{
		if(!t->second->ref_count)
			continue;
		if(t->second->declarationType!=DeclarationType::DEPTHSTATE)
			continue;
		DepthStencilState *d=(DepthStencilState *)t->second;
		outstr<<"DepthStencilState "<<t->first<<" "
			<<ToString(d->DepthTestEnable)
			<<","<<ToString(d->DepthWriteMask)
			<<","<<ToString((int)d->DepthFunc);
		outstr<<"\n";
	}

	std::vector<unsigned char> binBuffer;
	std::map<string, size_t> fileSizes;
	std::map<string, size_t> fileOffsets;
	// Add the group/technique/pass combinations to the effect file
	for (map<string, TechniqueGroup>::const_iterator g = m_techniqueGroups.begin(); g != m_techniqueGroups.end(); ++g)
	{
		const TechniqueGroup &group=g->second;
		outstr<<"group "<<g->first<<"\n{\n";
		for (map<string, Technique*>::const_iterator it =group.m_techniques.begin(); it !=group.m_techniques.end(); ++it)
		{
			std::string techName=it->first;
			const Technique *tech=it->second;
			outstr<<"\ttechnique "<<techName<<"\n\t{\n";
			const std::vector< Pass> &passes=tech->GetPasses();
			vector<Pass>::const_iterator j=passes.begin();
			for(;j!=passes.end();j++)
			{
				const Pass *pass=&(*j);
				string passName=j->name;
				outstr<<"\t\tpass "<<passName<<"\n\t\t{\n"; 
				if(pass->passState.rasterizerState.objectName.length()>0)
				{
					outstr<<"\t\t\trasterizer: "<<pass->passState.rasterizerState.objectName<<"\n";
				}
				if (pass->passState.renderTargetFormatState.objectName.length() > 0)
				{
					outstr << "\t\t\ttargetformat: " << pass->passState.renderTargetFormatState.objectName << "\n";
				}
				if(pass->passState.depthStencilState.objectName.length()>0)
				{
					outstr<<"\t\t\tdepthstencil: "<<pass->passState.depthStencilState.objectName<<" "<<pass->passState.depthStencilState.stencilRef<<"\n";
				}
				if(pass->passState.blendState.objectName.length()>0)
				{
					outstr<<"\t\t\tblend: "<<pass->passState.blendState.objectName<<" (";
					outstr<<pass->passState.blendState.blendFactor[0]<<",";
					outstr<<pass->passState.blendState.blendFactor[1]<<",";
					outstr<<pass->passState.blendState.blendFactor[2]<<",";
					outstr<<pass->passState.blendState.blendFactor[3]<<") ";
					outstr<<pass->passState.blendState.sampleMask<<"\n";
				}
				if (pass->HasShader(sfx::ShaderType::COMPUTE_SHADER))
				{
					const string &shaderInstanceName=pass->GetShader(sfx::ShaderType::COMPUTE_SHADER);
					ShaderInstance *shaderInstance=GetShaderInstance(shaderInstanceName, sfx::ShaderType::COMPUTE_SHADER);
					Function* function = gEffect->GetFunction(shaderInstance->m_functionName, 0);
					outstr << "\t\t\tnumthreads: " << function->numThreads[0] << " " << function->numThreads[1] <<" "<< function->numThreads[2] << "\n";
				}
				if(pass->passState.topologyState.apply)
				{
					outstr<<"\t\t\ttopology: "<<stringOf(pass->passState.topologyState.topology)<<"\n";
				}
				
				auto writeSb=[&] (ofstream &outstr,ShaderInstance *shaderInstance,const std::string &sbFilename,std::string pfm="")
					{
						if(!sbFilename.size())
							return;
						std::set<int> textureSlots,uavSlots,cbufferSlots,samplerSlots,textureSlotsForSB,uavTextureSlotsForSB;
						CalculateResourceSlots(shaderInstance,textureSlots,uavSlots,textureSlotsForSB,uavTextureSlotsForSB,cbufferSlots,samplerSlots);
						outstr<<stringOf((ShaderCommand)shaderInstance->shaderType);
						if (!shaderInstance->rtFormatStateName.empty())
						{
							outstr << "(" << shaderInstance->rtFormatStateName << ")";
						}
						else if(shaderInstance->shaderType==FRAGMENT_SHADER&&pfm.length())
							outstr<<"("<<pfm<<")";
						outstr << ": " << sbFilename;
						outstr << "("<<shaderInstance->entryPoint.c_str()<<")";
						if (sfxOptions.wrapOutput)
						{
							if(binaryMap.find(sbFilename)!=binaryMap.end())
							{
							// Write the offset and size.
								std::streampos pos;
								size_t sz;
								std::tie(pos, sz) = binaryMap[sbFilename];
								if((int)pos<0)
								{
									SFX_CERR << sbFilename.c_str()<< " has bad entry in binary map."<< std::endl;
									exit(153);
								}
								outstr << " inline:(0x" << std::hex << pos <<",0x"<< sz <<std::dec<<")";
							}
							else
							{
								SFX_CERR << sbFilename.c_str()<< "wasn't found in the binary map."<< std::endl;
							}

						}
						// Print texture slots
						outstr<<", t:(";
						for(auto w:textureSlots)
						{
							if(w!=*(textureSlots.begin()))
								outstr<<",";
							outstr<<GenerateTextureSlot(w,false);
						}
						// Print uav slots
						outstr<<"), u:(";
						for(auto w:uavSlots)
						{
							if(w!=*(uavSlots.begin()))
								outstr<<",";
							outstr<<GenerateTextureWriteSlot(w,false);
						}
						// Print structured buffer slots
						outstr<<"), b:(";
						for(auto w:textureSlotsForSB)
						{
							if(w!=*(textureSlotsForSB.begin()))
								outstr<<",";
							outstr<<GenerateTextureSlot(w,false);
						}
						// Print structured buffer slots
						outstr<<"), z:(";
						for(auto w:uavTextureSlotsForSB)
						{
							if(w!=*(uavTextureSlotsForSB.begin()))
								outstr<<",";
							outstr<<GenerateTextureWriteSlot(w,false);
						}
						// Print constant buffer slots
						outstr<<"), c:(";
						for(auto w: cbufferSlots)
						{
							if(w!=*(cbufferSlots.begin()))
								outstr<<",";
							outstr<<GenerateConstantBufferSlot(w,false);
						}
						// Print sampler slots
						outstr<<"), s:(";
						for(auto w:samplerSlots)
						{
							if(w!=*(samplerSlots.begin()))
								outstr<<",";
							outstr<<GenerateSamplerSlot(w,false);
						}
						outstr<<")\n";
					};
				
				bool multiviewDeclared = false;
				for(int s=0;s<NUM_OF_SHADER_TYPES;s++)
				{
					ShaderType shaderType=(ShaderType)s;
					if(!pass->HasShader(shaderType))
						continue;
					if(shaderType==ShaderType::EXPORT_SHADER)	// either/or.
						continue;

					if(shaderType==ShaderType::VERTEX_SHADER&&pass->HasShader(GEOMETRY_SHADER))
						shaderType=EXPORT_SHADER;
					const string &shaderInstanceName=pass->GetShader(shaderType);
					ShaderInstance *shaderInstance=GetShaderInstance(shaderInstanceName,shaderType);
					int vertex_or_export=0;
					if(shaderType==EXPORT_SHADER)
						vertex_or_export=1;

					Function *function = gEffect->GetFunction(shaderInstance->m_functionName, 0);

					bool multiview = false;
					multiview |= ShaderInstanceHasSemanic(shaderInstance, "SV_ViewID");
					multiview |= ShaderInstanceHasSemanic(shaderInstance, "SV_ViewId");
					if (multiview && !multiviewDeclared)
					{
						outstr << "\t\t\tmultiview: 1\n";
						multiviewDeclared = true;
					}

					if(shaderType==VERTEX_SHADER&&function->parameters.size())
					{
						outstr<<"\t\t\tlayout:\n";
						outstr<<"\t\t\t{\n";
						
						for(auto p:function->parameters)
						{
							auto D = declarations.find(p.type);
							Declaration *d=nullptr;
							if (D != declarations.end())
								d=D->second;
							// It is a struct and we are in a vertex shader
							if (d  && d->declarationType == DeclarationType::STRUCT)
							{
								Struct *s=(Struct *)d;
								if(s)
								{
									for(auto m:s->m_structMembers)
									{
										// ignore automatic semantics.
										size_t sem_pos=m.semantic.find("SV_");
										if(sem_pos<m.semantic.length())
											continue;
										outstr<<"\t\t\t\t"<<m.type<<" "<<m.name<<";\n";
									}
								}
							}
						}
						outstr<<"\t\t\t}\n";
					}
					for(auto v=shaderInstance->sbFilenames.begin();v!=shaderInstance->sbFilenames.end();v++)
					{
						string sbFilename=v->second;
						PixelOutputFormat pixelOutputFormat=(PixelOutputFormat)v->first;
						string pfm=ToString(pixelOutputFormat);
						outstr<<"\t\t\t";
						if(shaderType==FRAGMENT_SHADER||(int)pixelOutputFormat==vertex_or_export)
							writeSb(outstr,shaderInstance,sbFilename,pfm);
					}
				}
				
				// If it's raytrace, go through the hitgroups.
				for(auto h:pass->passState.raytraceHitGroups)
				{
					outstr<<"\t\t\thitgroup: "<<h.first<<"\n\t\t\t{\n";
					if(h.second.closestHit.length())
					{
						ShaderInstance *shaderInstance=GetShaderInstance(h.second.closestHit,CLOSEST_HIT_SHADER);
						outstr<<"\t\t\t\t";
						writeSb(outstr,shaderInstance,shaderInstance->sbFilenames[0]);
					}
					if(h.second.anyHit.length())
					{
						ShaderInstance *shaderInstance=GetShaderInstance(h.second.anyHit,ANY_HIT_SHADER);
						outstr<<"\t\t\t\t";
						writeSb(outstr,shaderInstance,shaderInstance->sbFilenames[0]);
					}
					if(h.second.intersection.length())
					{
						ShaderInstance *shaderInstance=GetShaderInstance(h.second.intersection,INTERSECTION_SHADER);
						outstr<<"\t\t\t\t";
						writeSb(outstr,shaderInstance,shaderInstance->sbFilenames[0]);
					}
					outstr<<"\t\t\t}\n";
				}

				//If it's raytrace, go through the miss shaders.
				if (!pass->passState.missShaders.empty())
				{
					outstr << "\t\t\tMissShaders:" << "\n\t\t\t{\n";
					for (auto m : pass->passState.missShaders)
					{
						if (m.length())
						{
							ShaderInstance* shaderInstance = GetShaderInstance(m, MISS_SHADER);
							outstr << "\t\t\t\t";
							writeSb(outstr, shaderInstance, shaderInstance->sbFilenames[0]);
						}
					}
					outstr << "\t\t\t}\n";
				}

				//If it's raytrace, go through the callable shaders.
				if (!pass->passState.callableShaders.empty())
				{
					outstr << "\t\t\tCallableShaders:" << "\n\t\t\t{\n";
					for (auto c : pass->passState.callableShaders)
					{
						if (c.length())
						{
							ShaderInstance* shaderInstance = GetShaderInstance(c, CALLABLE_SHADER);
							outstr << "\t\t\t\t";
							writeSb(outstr, shaderInstance, shaderInstance->sbFilenames[0]);
						}
					}
					outstr << "\t\t\t}\n";
				}

				//If it's raytrace, define the Shader and Pipeline configs
				if (!pass->passState.raytraceHitGroups.empty())
				{
					//---RayTracingShaderConfig---
					outstr << "\t\t\tRayTracingShaderConfig:" << "\n\t\t\t{\n";
					
					//Default is sizeof(float) * 8.
					int maxPayloadSize = pass->passState.maxPayloadSize != 0 ? pass->passState.maxPayloadSize : 32; 
					outstr << "\t\t\t\t" << "MaxPayloadSize: " << std::to_string(maxPayloadSize) << "\n";

					//Default is sizeof(BuiltInTriangleIntersectionAttributes).
					int maxAttributeSize = pass->passState.maxAttributeSize != 0 ? pass->passState.maxAttributeSize : 8; 
					outstr << "\t\t\t\t" << "MaxAttributeSize: " << std::to_string(maxAttributeSize) << "\n";

					outstr << "\t\t\t}\n";
					
					//---RayTracingPipelineConfig---
					outstr << "\t\t\tRayTracingPipelineConfig:" << "\n\t\t\t{\n";

					//Default is 2 for inital hit and shadow.
					int maxTraceRecursionDepth = pass->passState.maxTraceRecursionDepth != 0 ? pass->passState.maxTraceRecursionDepth : 2; 
					outstr << "\t\t\t\t" << "MaxTraceRecursionDepth: " << std::to_string(maxTraceRecursionDepth) << "\n";

					outstr << "\t\t\t}\n";
				}

				outstr<<"\t\t}\n";
			}
			outstr<<"\t}\n";
		}
		outstr<<"}\n";
	}
	// write a zero to terminate the text.
	char c = 0;
	outstr.write(&c, 1);
	std::streampos pos = outstr.tellp();

	if (sfxOptions.wrapOutput)
	{
	// TODO: binary is here, sfxb not needed??
		outstr.write((const char *)binBuffer.data(), binBuffer.size());
	}
	if(sfxOptions.verbose)
	{
		std::cout<<sfxoFilename.c_str()<<": info: output effect file."<<std::endl;
	}
	return res!=0;
}

bool Effect::IsDeclared(const string &name)
{
	if (declarations.find(name) != declarations.end())
		return true;
	return false;
}

bool Effect::IsConstantBufferMemberAlignmentValid(const Declaration* d)
{
	const Struct* structure = static_cast<const Struct*>(d);
	int offset = 0;
	for (const StructMember& member : structure->m_structMembers)
	{
		std::regex digits("\\d");
		std::smatch match;
		int size = 4;
		if (std::regex_search(member.type, match, digits))
		{
			size *= atoi(match[0].str().c_str());
		}
		if (member.type.find(match[0].str() + "x" + match[0].str()) != std::string::npos
			|| member.type.find("mat") != std::string::npos)
		{
			size *= atoi(match[0].str().c_str());
		}

		//std::cout << offset << ": " << structure->name << "::" << member.name << "\n";

		if (((offset % 16) && (size > 8))
			|| ((offset % 8) && (size == 8)))
		{
			std::cerr << fileList[d->file_number] << "(" << d->line_number << ") : error : Constant Buffer has misaligned members. ";
			std::cerr << d->name << "::" << member.name << " can not be at an offset of " << offset << " with a size of " << size << "." << std::endl;
			return false;
		}

		offset += size;
	}

	return true;
}

string Effect::GetTypeOfParameter(std::vector<sfxstype::variable>& parameters, string keyName)
{
	for (auto p : parameters)
	{
		if (p.identifier == keyName)
		{
			return p.type;
		}
	}
	return "";
}

string Effect::GetDeclaredType(const string &name) const
{
	auto i = declarations.find(name);
	if (i != declarations.end())
	{
		if(i->second->declarationType==DeclarationType::TEXTURE)
			return ((DeclaredTexture*)(i->second))->type;
		if(i->second->declarationType==DeclarationType::SAMPLER)
			return "SamplerState";
		if(i->second->declarationType==DeclarationType::BLENDSTATE)
			return "BlendState";
		if(i->second->declarationType==DeclarationType::RASTERIZERSTATE)
			return "RasterizerState";
		if(i->second->declarationType==DeclarationType::DEPTHSTATE)
			return "DepthState";
		if(i->second->declarationType==DeclarationType::STRUCT)
			return "struct";
	}
	return "unknown";
}

NamedConstantBuffer* Effect::DeclareNamedConstantBuffer(const string &name,int slot,int space,const string &structureType, const Struct& ts,const string &original)
{
	NamedConstantBuffer* t	  = new NamedConstantBuffer();
	*(static_cast<Struct*>(t)) = ts;
	t->declarationType		=DeclarationType::NAMED_CONSTANT_BUFFER;
	t->structureType		= structureType;
	t->original			 = original;
	t->name				 = name;
	t->slot				=slot;
	t->space			=space;
	t->instance_name = name;
	declarations[name] = (t);
	declarations[structureType] = (t);
	if (!IsConstantBufferMemberAlignmentValid(declarations[name]))
	{
		delete t;
		return nullptr;
	}
	return t;
}

DeclaredTexture* Effect::DeclareTexture(const string &name,ShaderResourceType shaderResourceType,int slot,int space,const string &structureType,const string &original)
{
	DeclaredTexture* t	  = new DeclaredTexture();
	t->structureType		= structureType;
	t->original			 = original;
	t->name				 = name;
	t->shaderResourceType	= shaderResourceType;

	bool write = IsRW(shaderResourceType);
	bool rw = IsRW(shaderResourceType);
	int num = 0;
	if (sfxConfig.generateSlots)
	{
		num	= -1;
		// scene
		if(shaderResourceType==ShaderResourceType::RAYTRACE_ACCELERATION_STRUCT)
		{
			//keep declared slot?...
			num=slot;
		}
		// RW texture
		else if (shaderResourceType==ShaderResourceType::NAMED_CONSTANT_BUFFER)
		{
			//find a good buffer slot later...
		}
		else if (rw)
		{
			if (rwTextureNumberMap.find(name) != rwTextureNumberMap.end())
			{
				num = rwTextureNumberMap[name];
			}
			else
			{
				rwTextureNumberMap[name] = current_rw_texture_number;
				num = current_rw_texture_number++;
			}
		}
		// R texture
		else
		{
			// REALLY what we want here is to find the lowest slot number
			//    that isn't used by another texture in the same shader.
			// so we must know which shaders use the texture, and which other textures they use.
			if (textureNumberMap.find(name) != textureNumberMap.end())
			{
				num = textureNumberMap[name];
			}
			else
			{
				if (current_texture_number < sfxConfig.numTextureSlots)
				{
					textureNumberMap[name] = current_texture_number;
					num = current_texture_number++;
				}
				else // TRY using the slot that's specified, if one IS specified...
				{
					std::cerr << this->Filename().c_str()<< ": error: ran out of texture slots."<< std::endl;
					exit(1);
					num = GetTextureNumber(name.c_str(), slot);
					textureNumberMap[name] = num;
				}
			}
		}
		t->slot = num;
	}
	else
	{
		if (write)
		{
			num = GetRWTextureNumber(name.c_str(), slot);
		}
		else
		{
			num = GetTextureNumber(name.c_str(), slot);
		}

		if (slot < 0)
		{
			slot = num;
		}
		else
		{
			assert(slot == num);
		}
		if (num >= 0)
		{
			t->slot = num;
			if (write)
				num += 1000;
			if (m_declaredTexturesByNumber.find(num) != m_declaredTexturesByNumber.end())
			{
				std::cerr << "Already declared " << (write ? "RW " : "") << "texture number " << (num % 1000) << std::endl;
			}
			else
			{
				m_declaredTexturesByNumber[num] = t;
			}
		}
		else
			t->slot = -1;
	}
	if (num >= sfxConfig.numTextureSlots)
	{
		if (num >= 1000)
			num -= 1000;
		if (num >= sfxConfig.numTextureSlots)
		{
			std::cerr << "Error: slot " << num << " too big, maximum is " << sfxConfig.numTextureSlots << "." << std::endl;
			return nullptr;
		}
	}
	declarations[name] = (t);
	return t;
}
SamplerState *Effect::DeclareSamplerState(const string &name,int register_number,const SamplerState &templateSS)
{
	SamplerState *s=new SamplerState();
	*s= templateSS;
	if(register_number<0)
	{
		register_number=m_max_sampler_register_number+1;
	}
	s->register_number=register_number;
	declarations[name]=s;
	m_max_sampler_register_number=std::max(register_number,m_max_sampler_register_number);
	return s;
}

RasterizerState *Effect::DeclareRasterizerState(const string &name)
{
	RasterizerState *s=new RasterizerState();
	declarations[name]=s;
	return s;
}

RenderTargetFormatState *Effect::DeclareRenderTargetFormatState(const std::string &name)
{
	RenderTargetFormatState *s  = new RenderTargetFormatState();
	declarations[name]		  = s;
	return s;
}

BlendState *Effect::DeclareBlendState(const string &name)
{
	BlendState *s=new BlendState();
	declarations[name]=s;
	return s;
}
DepthStencilState *Effect::DeclareDepthStencilState(const string &name)
{
	DepthStencilState *s=new DepthStencilState();
	declarations[name]=s;
	return s;
}

int Effect::GetRWTextureNumber(string n, int specified_slot)
{
	int texture_number = current_rw_texture_number;
	if(rwTextureNumberMap.find(n) != rwTextureNumberMap.end())
		texture_number = rwTextureNumberMap[n];
	else
	{
		if(specified_slot>=0)
			texture_number = specified_slot;
		else
		{
			if (specified_slot >= 0)
				texture_number = specified_slot;
			else return -1;
	
			rwTextureNumberMap[n] = texture_number;
		}
	}
	return texture_number;
}

int Effect::GetTextureNumber(string n,int specified_slot)
{
	int texture_number = current_texture_number;
	if (textureNumberMap.find(n) != textureNumberMap.end())
		texture_number = textureNumberMap[n];
	else
	{
		if (specified_slot >= 0)
			texture_number = specified_slot;
		else return -1;
		textureNumberMap[n] = texture_number;
	}
	return texture_number;
}

 void errSem(const string& str, int lex_linenumber);


 std::string Effect::CombinedTypeString(const std::string& type, const std::string& memberType)
 {
	 if (memberType.length() == 0)
		 return type;
	 std::string key = type + "<";
	 key += memberType;
	 key += ">";
	auto i= sfxConfig.templateTypes.find(key);
	if (i != sfxConfig.templateTypes.end())
	{
		return i->second;
	}
	return type;
 }
 bool Effect::CheckDeclaredGlobal(const Function* func, const std::string toCheck)
 {
	 for (const auto &g : func->globals)
	 {
		 if (g == toCheck)
		 {
			 return true;
		 }
	 }
	 for (const auto cf : func->functionsCalled)
	 {
		 if (CheckDeclaredGlobal(cf, toCheck))
		 {
			 return true;
		 }
	 }
	 return false;
 }

 void Effect::Declare(ShaderType shaderType,ostream &os,const Declaration *d, ConstantBuffer& texCB, ConstantBuffer& sampCB, const std::set<std::string>& rwLoad, std::set<const SamplerState*>& declaredSS, const Function* mainFunction)
{
	switch(d->declarationType)
	{
		case DeclarationType::TEXTURE:
		{
			std::string dec = "";
			DeclaredTexture* td	= (DeclaredTexture*)d;
			if ((td->shaderResourceType & ShaderResourceType::MS)== ShaderResourceType::MS)
			{
				dec = "";
			}
			// It is a (RW)StructuredBuffer
			if (( td->type == "RWStructuredBuffer" || td->type == "StructuredBuffer")
			
				&&!sfxConfig.structuredBufferDeclaration.empty())
				{
					dec = sfxConfig.structuredBufferDeclaration;
					static int ssbouid = 0;
					find_and_replace(dec, "{name}", td->name + "_ssbo");
					int temp_slot=td->slot;
					if(IsRW(td->shaderResourceType))
						temp_slot=GenerateTextureWriteSlot(td->slot);
					else
						temp_slot=GenerateTextureSlot(td->slot);
					find_and_replace(dec, "{slot}", ToString(temp_slot));
					find_and_replace(dec, "{content}", td->structureType + " " + td->name + "[];");
					ssbouid++;
				
			}
			// Its an Image or Texture
			else
			{
				if ( (td->type == "RWStructuredBuffer" || td->type == "StructuredBuffer"))
				{
					std::cout<<"";
				}
				// Check the type of this texture (we want to know if will be used to load/store operations)
				bool isImage = false;
				bool isImageArray = false;
				ShaderResourceType type = td->shaderResourceType & sfx::ShaderResourceType::RW;
				if (type == sfx::ShaderResourceType::RW)
				{
					isImage = true;
					type = td->shaderResourceType & sfx::ShaderResourceType::ARRAY;
					if (type == sfx::ShaderResourceType::ARRAY)
					{
						isImageArray = true;
					}
				}
				// Image
				if (isImage)
				{
					string str = sfxConfig.imageDeclaration;
					// Not used in load operations
					if (rwLoad.find(td->name) == rwLoad.end())
					{
						str = sfxConfig.imageDeclarationWriteOnly;
					}
					if (str.length() == 0)
					{
						str = "{pass_through}";
					}
					// Find the correct image type image,uimage,iimage etc
					string ntype = td->structureType;
					if (!sfxConfig.toImageType.empty())
					{
						auto ele = sfxConfig.toImageType.find(td->structureType);
						if (ele == sfxConfig.toImageType.end())
						{
							// error;
							std::cerr << "Could not find the combination of this image type! \n";
							ntype = "null";
						}
						else
						{
							ntype = ele->second;
							if ((td->shaderResourceType & ShaderResourceType::TEXTURE_1D) == ShaderResourceType::TEXTURE_1D)
							{
								ntype += "1D";
							}
							else if ((td->shaderResourceType & ShaderResourceType::TEXTURE_2D) == ShaderResourceType::TEXTURE_2D)
							{
								ntype += "2D";
							}
							else if ((td->shaderResourceType & ShaderResourceType::TEXTURE_3D) == ShaderResourceType::TEXTURE_3D)
							{
								ntype += "3D";
							}
							else if ((td->shaderResourceType & ShaderResourceType::TEXTURE_CUBE) == ShaderResourceType::TEXTURE_CUBE)
							{
								ntype += "Cube";
							}
							if ((td->shaderResourceType & ShaderResourceType::ARRAY) == ShaderResourceType::ARRAY)
							{
								ntype += "Array";
							}
						}
					}

					find_and_replace(str, "{pass_through}", td->original);
					find_and_replace(str, "{type}", ntype);
					find_and_replace(str, "{name}", td->name);
					int slot=td->slot;
					if(shaderType==FRAGMENT_SHADER)
						slot++;
					slot=GenerateTextureWriteSlot(slot);
					find_and_replace(str, "{slot}", ToString(slot));

					if (!sfxConfig.toImageFormat.empty())
					{
						auto fmt = sfxConfig.toImageFormat.find(td->structureType);
						string iFmt = "";
						if (!sfxConfig.toImageType.empty() && fmt == sfxConfig.toImageFormat.end())
						{
							std::cerr << "Couldn't find the image format:" << td->structureType << "\n";
							iFmt = "null";
						}
						else
						{
							iFmt = fmt->second;
						}
						find_and_replace(str, "{fqualifier}", iFmt);
					}

					dec = str;
				}
				// Texture
				else
				{
					string str = sfxConfig.textureDeclaration;
					if (str.length() == 0)
					{
						str = "{pass_through}";
					}
					// Add it to the texture constant buffer
					if (sfxConfig.combineTexturesSamplers&&!sfxConfig.combineInShader)
					{
						StructMember texMember = { "uint64_t",td->name + "[24]", "" };
						texCB.m_structMembers.push_back(texMember);
					}
					else
					{
						find_and_replace(str, "{pass_through}", td->original);
						find_and_replace(str, "{type}", CombinedTypeString(td->type,td->structureType));
						find_and_replace(str, "{name}", td->name);
						find_and_replace(str, "{slot}", ToString(GenerateTextureSlot(td->slot)));
						dec = str;
					}
				}
			}
			os << dec.c_str() << endl;
		}
		break;
		case DeclarationType:: SAMPLER:
		{
			SamplerState* ss = (SamplerState*)d;
			if (sfxConfig.combineTexturesSamplers && sfxConfig.combineInShader)
			{
				bool found = false;
				// We dont want to add duplicates
				for (auto& memb : sampCB.m_structMembers)
				{
					if (memb.name == ss->name)
					{
						found = true;
						break;
					}
				}
				if (!found)
				{
					StructMember sMember = { "uint64_t",ss->name, "" };
					sampCB.m_structMembers.push_back(sMember);
				}
			}
		/*	else if(!sfxConfig.passThroughSamplers && !sfxConfig.maintainSamplerDeclaration)
			{
				string str = sfxConfig.samplerDeclaration;
				find_and_replace(str, "{name}", ss->name);
				find_and_replace(str, "{slot}", ToString(ss->register_number));
				os<<str.c_str()<<endl;
			}*/
		}
		break;
		case DeclarationType:: BLENDSTATE:
			break;
		case DeclarationType:: RASTERIZERSTATE:
			break;
		case DeclarationType:: DEPTHSTATE:
			break;
		case DeclarationType:: BUFFER:
			break;
		case DeclarationType:: STRUCT:
		{
			Struct *s=(Struct*)d;
			string str = sfxConfig.structDeclaration;

			//Check if struct is used in templatized ConstantBuffer
			//HLSL SM5.1 has the struct declared before ConstantBuffer<>
			//GLSL 420 has the struct declared with layout(std140, binding = X) uniform
			//PSSL Doesn't support templatized ConstantBuffer, we need to convert it back to a global constant buffer.
			if (sfxConfig.constantBufferDeclaration.length() || sfxConfig.sourceExtension.compare("pssl") == 0)
			{ 
				bool structIsUsedInTemplatizedConstantBuffer = false;
				for (auto& decl : declarations)
				{
					if (decl.second->declarationType == DeclarationType::CONSTANT_BUFFER && decl.second->structureType.length())
					{
						if (decl.second->structureType.compare(d->name) == 0)
						{
							structIsUsedInTemplatizedConstantBuffer = true;
						}
					}
				}
				if (structIsUsedInTemplatizedConstantBuffer)
					break; //Struct should now be declared with the templatized ConstantBuffer.
			}

			if (sfxConfig.pixelOutputDeclaration.empty() || sfxConfig.pixelOutputDeclarationDSB.empty())
			{
				for (auto& member : s->m_structMembers)
				{
					if (member.semantic.size() == std::string("SV_TARGET688366XX").size()) //SV_TARGET6883660##n for Dual Source Blending
					{
						char checkDSB[3];
						checkDSB[0] = (char)(10 * atoi(member.semantic.substr(9, 1).c_str())) + atoi(member.semantic.substr(10, 1).c_str());
						checkDSB[1] = (char)(10 * atoi(member.semantic.substr(11, 1).c_str())) + atoi(member.semantic.substr(12, 1).c_str());
						checkDSB[2] = (char)(10 * atoi(member.semantic.substr(13, 1).c_str())) + atoi(member.semantic.substr(14, 1).c_str());

						if (checkDSB[0] != 'D' || checkDSB[1] != 'S' || checkDSB[2] != 'B')
							std::cerr << "SV_TARGET index is not a valid for dual source blending: " << member.semantic << std::endl;
						
						int slotidx = atoi(member.semantic.substr(16, 1).c_str());
						int dsbIndex = atoi(member.semantic.substr(15, 1).c_str());
						int finalSV_TargetIdx = slotidx + dsbIndex;
						if(finalSV_TargetIdx < 0 || finalSV_TargetIdx > 7)
							std::cerr << "Resolved SV_TARGET index (for dual source blending) is not a valid: " << member.semantic << std::endl;

						std::string replacementSemantic = "SV_TARGET" + std::to_string(finalSV_TargetIdx);
						size_t pos = s->original.find(member.semantic);
						if(pos != std::string::npos)
							s->original.replace(pos, member.semantic.size(), replacementSemantic);
					}
				}
			}

			if (str.length() == 0)
			{
				str = "{pass_through}";
				os << s->original.c_str() << endl;
				return;
			}

			find_and_replace(str, "{pass_through}", s->original);
			string members;
			// in square brackets [] is the definition for ONE member.
			string structMemberDeclaration;
			process_member_decl(str, structMemberDeclaration);
			for (int i = 0; i < s->m_structMembers.size(); i++)
			{
				string m = "";
				// This should only be done for structs used for vertex output declaration
				// GLSL requires the flat
				if (s->m_structMembers[i].type.length() && (s->m_structMembers[i].type == "uint" || s->m_structMembers[i].type == "int"))
				{
					m += "\tflat " + structMemberDeclaration.substr(1);
				}
				else
				{
					m += structMemberDeclaration;
				}
				find_and_replace(m, "{type}", s->m_structMembers[i].type);
				find_and_replace(m, "{name}", s->m_structMembers[i].name);
				find_and_replace(m, "{semantic}", s->m_structMembers[i].semantic);
				members += m + "\n";
			}
			find_and_replace(str, "{members}", members);
			find_and_replace(str, "{name}", s->name);
			os << str.c_str() << endl;
		}
		break;
		case DeclarationType::CONSTANT_BUFFER:
			{
				// Anonymous.
				ConstantBuffer* s = (ConstantBuffer*)d;
				string str = sfxConfig.constantBufferDeclaration;
				if (str.length() == 0)
				{
					str = "{pass_through}";
					os << s->original.c_str() << endl;
					return;
				}

				find_and_replace(str, "{pass_through}", s->original);
				string members;
				// in square brackets [] is the definition for ONE member.
				std::regex re_member("\\[(.*)\\]");
				std::smatch match;
				string structMemberDeclaration;
				if (std::regex_search(str, match, re_member))
				{
					structMemberDeclaration = match[1].str();
					str.replace(match[0].first, match[0].first + match[0].length(), "{members}");
				}
				for (int i = 0; i < s->m_structMembers.size(); i++)
				{
					string m = structMemberDeclaration;
					string type = s->m_structMembers[i].type;
					find_and_replace(m, "{type}", type);
					find_and_replace(m, "{name}", s->m_structMembers[i].name);
					find_and_replace(m, "{semantic}", s->m_structMembers[i].semantic);
					members += m + "\n";
				}
				find_and_replace(str, "{members}", members);
				find_and_replace(str, "{name}", s->name);
				if (s->slot == -1)
				{
					// Let the compiler decide
					find_and_replace(str, "layout(binding = {slot})", "");
				}
				else
				{
					find_and_replace(str, "{slot}", ToString(GenerateConstantBufferSlot(s->slot)));
				}
				os << str.c_str() << endl;
			}
			break;
		case DeclarationType::NAMED_CONSTANT_BUFFER:
		{
			if (!d->structureType.length())
			{
				std::cerr << "Could not find structureType:" << d->name << std::endl;
			}
			else if (!sfxConfig.namedConstantBufferDeclaration.length())
			{
				std::cerr << "No namedConstantBufferDeclaration in json file:" << d->name << std::endl;
			}
			else
			{
				// named.
				const NamedConstantBuffer *c = static_cast<const NamedConstantBuffer*>(d);
				string str = sfxConfig.namedConstantBufferDeclaration;
				Struct* s = (Struct*)declarations[d->structureType];
				string members;
				// in square brackets [] is the definition for ONE member.
				std::regex re_member("\\[(.*)\\]");
				std::smatch match;
				string structMemberDeclaration;
				if (std::regex_search(str, match, re_member))
				{
					structMemberDeclaration = match[1].str();
					str.replace(match[0].first, match[0].first + match[0].length(), "{members}");
				}
				for (int i = 0; i < s->m_structMembers.size(); i++)
				{
					string m = structMemberDeclaration;
					find_and_replace(m, "{type}", s->m_structMembers[i].type);
					find_and_replace(m, "{name}", s->m_structMembers[i].name);
					find_and_replace(m, "{semantic}", s->m_structMembers[i].semantic);
					members += m + "\n";
				}
				find_and_replace(str, "{members}", members);
				find_and_replace(str, "{name}", s->structureType);
				find_and_replace(str, "{slot}", ToString(c->slot));
				find_and_replace(str, "{instance_name}", c->instance_name);
				os << str.c_str() << endl;
			}
		}
		break;
		default:
		break;
	};
}

void Effect::ConstructSource(ShaderInstance *shaderInstance)
{
	string shaderName = shaderInstance->m_functionName;
	if (strcasecmp(shaderName.c_str(), "NULL") == 0)
		return;

	ostringstream theShader;
	Function *function = gEffect->GetFunction(shaderName, 0);
	if (!function)
	{
		ostringstream errMsg;
		errMsg << "Unable to find referenced shader \"" << shaderName << '\"';
		errSem(errMsg.str(), shaderInstance->global_line_number);
		return;
	}

	std::set<const Declaration *> decs;
	std::set<const SamplerState*> samplerStates;
	std::set<std::string> rwTexturesUsedForLoad;
	AccumulateDeclarationsUsed(function,decs,rwTexturesUsedForLoad);
	// Keep a set of the sampler states used:
	for (const auto d : decs)
	{
		if(d->declarationType == DeclarationType::SAMPLER)
		{
			SamplerState* ss = (SamplerState*)d;
			if (ss)
			{
				samplerStates.insert(ss);
			}
		}
	}
	// for now, add in ALL the constant buffers...
	for(auto i:shaderInstance->constantBuffers)
	{
		decs.insert(i);
	}
	std::map<int,const Declaration *> ordered_decs;
	for (auto u = decs.begin(); u != decs.end(); u++)
	{
		int main_linenumber=(*u)->global_line_number;
		while (ordered_decs.find(main_linenumber) != ordered_decs.end())
		{
			main_linenumber++;
		}
		ordered_decs[main_linenumber]=*u;
	}
	// Used for platforms that dont support separate sampler and textures:
	ConstantBuffer textureCB;
	ConstantBuffer samplerCB;
	// declare the samplers if we didn't pass them through already:
	if(!sfxConfig.passThroughSamplers)
	{
		for (const auto s : samplerStates)
		{
		//Declare(shaderInstance->shaderType, theShader, s, textureCB, samplerCB, rwTexturesUsedForLoad, samplerStates, function);
			if(sfxConfig.samplerDeclaration.size() > 0)
			{
				string thisDeclaration=sfxConfig.samplerDeclaration;
				find_and_replace(thisDeclaration,"{name}",s->name);
				find_and_replace(thisDeclaration,"{slot}",ToString(gEffect->GenerateSamplerSlot(s->register_number)));
				find_and_replace(thisDeclaration,"{type}","sampler");
				theShader<<thisDeclaration<<"\n";
			}
			else
			{
				theShader<<s->original<<"\n";
			}
		}
	}
	char kTexHandleUbo []  = "_TextureHandles_X";
	char ext[]={'v','h','d','g','p','c'};
	bool combo=sfxConfig.combineTexturesSamplers&&!sfxConfig.combineInShader;
	if (combo)
	{
		textureCB.declarationType	= DeclarationType::CONSTANT_BUFFER;
		kTexHandleUbo[16]=ext[shaderInstance->shaderType];
		textureCB.name			  = kTexHandleUbo;
		textureCB.ref_count		 = 1;
		textureCB.slot			  = kTexHandleUbo[16] == 'p' ? 1 : 0;

		samplerCB.declarationType	= DeclarationType::CONSTANT_BUFFER;
		samplerCB.name			  = "_SamplerHandles_";
		samplerCB.ref_count		 = 1;
		samplerCB.slot			  = -1;
	}

	// Declare them!
	for(auto u=ordered_decs.begin();u!=ordered_decs.end();u++)
	{
		const Declaration *d=(u->second);
		if (d->line_number > 0 && d->file_number > 0)
		{
			// Dont add the line number for textures and samplers as we will acum it
			// inside a constant buffer
			if (combo && d->declarationType != DeclarationType::TEXTURE && d->declarationType != DeclarationType::SAMPLER)
			{
				if (!GetOptions()->disableLineWrites)
					theShader << "#line " << d->line_number << " " << GetFilenameOrNumber(d->file_number) << endl;
			}
		}
		Declare(shaderInstance->shaderType, theShader, d, textureCB, samplerCB, rwTexturesUsedForLoad, samplerStates, function);
	}

	// Declare the texture and sampler CB:
	if (combo)
	{
		if (!textureCB.m_structMembers.empty())
		{
			Declare(shaderInstance->shaderType,theShader, (Declaration*)&textureCB, textureCB, samplerCB, rwTexturesUsedForLoad, samplerStates,function);
		}
		if (sfxConfig.combineInShader && !samplerCB.m_structMembers.empty())
		{
			Declare(shaderInstance->shaderType,theShader, (Declaration*)&samplerCB, textureCB, samplerCB, rwTexturesUsedForLoad, samplerStates, function);
		}
	}

	std::set<const Function *> fns;
	AccumulateFunctionsUsed(function,fns);
	std::map<int,const Function *> ordered_fns;
	for(auto u=fns.begin();u!=fns.end();u++)
	{
		// Add only the called functions, not the main one. That goes at the end.
		if (*u != function)
		{
			ordered_fns[(*u)->main_linenumber]=*u;
		}
		else
		{
			// Fix samplers for main function:
			if (!sfxConfig.combineInShader && combo)
			{
				for (const auto s : samplerStates)
				{
					find_and_replace(function->content, s->name, "1 + " + std::to_string(s->register_number));
				}
			}

			//Convert constant buffer accesses from templatized to global style in main function:
			if (sfxConfig.sourceExtension.compare("pssl") == 0)
			{
				for (auto& decl : declarations)
				{
					if (decl.second->declarationType == DeclarationType::CONSTANT_BUFFER && decl.second->structureType.length())
					{
						find_and_replace(function->content, decl.second->name + ".", decl.second->name + "_");
					}
				}
			}
		}
	}
	std::set<const Variable*> vars;
	AccumulateGlobals(function, vars);
	for (auto v = vars.begin(); v != vars.end(); v++)
	{
		theShader<<v.operator*()->original<<std::endl;
	}
	for(auto u=ordered_fns.begin();u!=ordered_fns.end();u++)
	{
		const Function* f = (u->second);
		if (GetOptions()->disableLineWrites)
			theShader <<"//";
		theShader << "#line " << f->local_linenumber << " " << gEffect->GetFilenameOrNumber(f->filename) << endl;
		std::string newDec  = f->declaration;
		std::string newCont = f->content;
		
		// Remove semantics on non-main functions:
		if (combo)
		{
			find_and_replace(newDec, ":","");
			for (auto & p : f->parameters)
			{
				find_and_replace(newDec, p.semantic, "");
			}
		}
		// Convert sampler names to 1 + slot:
		if (combo)
		{
			for (const auto s : samplerStates)
			{
				find_and_replace(newCont, s->name, "1 + " + std::to_string(s->register_number));
			}
		}
		//Convert constant buffer accesses from templatized to global style in non-main functions:
		if (sfxConfig.sourceExtension.compare("pssl") == 0)
		{
			for (auto& decl : declarations)
			{
				if (decl.second->declarationType == DeclarationType::CONSTANT_BUFFER && decl.second->structureType.length())
				{
					find_and_replace(newCont, decl.second->name + ".", decl.second->name + "_");
				}
			}
		}

		theShader << newDec;
		theShader << "\n{\n" << newCont << "\n}\n";
	}

	string entryPoint					= "main";
	const SfxConfig *config				= gEffect->GetConfig();
	string dec							= function->declaration;
	if (config->entryPointOption.length() > 0)
	{
		entryPoint						= shaderName;
	}
	else
	{
		find_and_replace(dec,shaderName,"main");
	}
	
	// in square brackets [] is the definition for ONE member.
	string memberDeclaration;
	//find_and_replace(memberDeclaration,"{blockname}",blockname);
	// extract and replace the member declaration.
	// If this shader language can't accept input parameters in main shader functions
	// we will have an inputDeclaration specifying how to rewrite the input.
	bool split_structs		= false;
	int num=0;
	string inputDeclaration = sfxConfig.pixelInputDeclaration;
	if(shaderInstance->shaderType==sfx::ShaderType::VERTEX_SHADER)
	{
		if (sfxConfig.vertexInputDeclaration.length() > 0)
		{
			inputDeclaration=sfxConfig.vertexInputDeclaration;
			split_structs = true;
		}
	}
	if(shaderInstance->shaderType==sfx::ShaderType::FRAGMENT_SHADER)
	{
		if(sfxConfig.pixelInputDeclaration.find("{member_type}")<sfxConfig.pixelInputDeclaration.length())
		{
			split_structs=true;
			num=1;
			find_and_replace(inputDeclaration,"{type}","{struct_type}");
			find_and_replace(inputDeclaration,"{name}","{struct_name}");
		}
	}
	string content			= function->content;
	string str=inputDeclaration;
	process_member_decl(str,memberDeclaration);
	string blockname = "ioblock";
	if(inputDeclaration.length()>0)
	{
		string members;
		string setup_code;
		for(auto i:function->parameters)
		{
			auto D = declarations.find(i.type);
			Declaration *d=nullptr;
			if (D != declarations.end())
				d=D->second;
			// It is a struct 
			if (d && split_structs && d->declarationType == DeclarationType::STRUCT)
			{
				Struct *s=(Struct *)d;
				if(s)
				{
					setup_code+=(i.type+" ")+i.identifier+";\n";
					string structPrefix;
					if(shaderInstance->shaderType==sfx::ShaderType::FRAGMENT_SHADER)
					{
						structPrefix=i.type+"BlocKData";
					}
					for(auto j:s->m_structMembers)
					{
						string m=memberDeclaration;
						find_and_replace(m,"{struct_type}",i.type);
						find_and_replace(m,"{struct_name}","BlocKData");
						find_and_replace(m,"{member_type}",j.type);
						find_and_replace(m,"{member_name}",j.name);
						find_and_replace(m,"{type}",j.type);
						find_and_replace(m,"{name}",j.name);
						find_and_replace(m,"{semantic}",j.semantic);
						find_and_replace(m,"{slot}",ToString(num++));
						auto s=sfxConfig.vertexSemantics.find(j.semantic);
						if(shaderInstance->shaderType==sfx::ShaderType::VERTEX_SHADER&&s!=sfxConfig.vertexSemantics.end())
						{
							setup_code+=((i.identifier+".")+j.name+"=")+s->second+";\n";
						}
						else
						{
							members+=m+"\n";
							setup_code+=((i.identifier+".")+j.name+"=")+structPrefix+j.name+";\n";
						}
					}
				}
			}
			// We are in vertex but its not a struct
			else if (split_structs)
			{
				// Could be a simple type (uint,int etc) with a semantic:
				auto s = sfxConfig.vertexSemantics.find(i.semantic);
				if (s != sfxConfig.vertexSemantics.end())
				{
					setup_code += i.type + " " + i.identifier + "=" + s->second + ";\n";
				}
			}
			// Fragment shader (could be or not a struct)
			else if(shaderInstance->shaderType == sfx::ShaderType::FRAGMENT_SHADER)
			{
				string customId = i.identifier;
				auto s=sfxConfig.pixelSemantics.find(i.semantic);
				if(i.semantic.length()>0&&s!=sfxConfig.pixelSemantics.end())
				{
					setup_code+=((i.type+" ")+i.identifier+"=")+s->second+";\n";
				}
				else
				{
					if (sfxConfig.identicalIOBlocks)
					{
						// In GLSL io blocks should match, i.e. the names of
						// the members should be the same.		
						customId = "BlockData";
						find_and_replace(content, i.identifier, customId);
					}
					else
					{
						customId = string("BlockData") + QuickFormat("%d", num);
						find_and_replace_identifier(content, i.identifier, customId);
					}
					string m = memberDeclaration;
					string acceptable_type=i.type;
					// ioblocks can't have bool's.
					if(i.type=="bool")
					{
						acceptable_type="char";
					}
					find_and_replace(m, "{type}", acceptable_type);
					find_and_replace(m, "{name}", customId);
					find_and_replace(m, "{semantic}", i.semantic);
					find_and_replace(m, "{slot}", ToString(num++));
					members += m + "\n";
					setup_code += ((i.type + " ") + customId + "=") + (blockname + ".") + customId + ";\n";
				}
			}
			// Compute shader (could be or not a struct)
			else if (shaderInstance->shaderType == sfx::ShaderType::COMPUTE_SHADER)
			{
				for (auto s: sfxConfig.computeSemantics)
				{
					if (strcasecmp(s.first.c_str(), i.semantic.c_str()) == 0)
					{
						setup_code += i.type + " " +  i.identifier + " = " + s.second + ";\n";
						break;
					}
				}
			}
			else
			{
				// error
			}
		}
		// Write members
		if (!members.empty())
		{
			find_and_replace(str,"{members}",members);
			find_and_replace(str, "{slot}", ToString(0));
			theShader<<str.c_str()<<endl;
		}
		content = setup_code + content;
		dec="void "+entryPoint+"()";
	}
	if(function->returnType!="void")
	{
		if(shaderInstance->shaderType==sfx::ShaderType::FRAGMENT_SHADER)
		{
			if(sfxConfig.pixelOutputDeclaration.length()>0)
			{
				// replace each return statement with a block that writes
				string str=sfxConfig.pixelOutputDeclaration;
				find_and_replace(str,"{slot}","0"); // TODO: What if this is not 0! 
				string m;
				// extract and replace the member declaration.
				process_member_decl(str,m);
				// Simple case: one return.
				if(function->returnType=="vec4"|| function->returnType == "float4")
				{
					// Declare the output
					string returnName="returnObject_"+function->returnType;
					find_and_replace(m,"{type}",function->returnType);
					find_and_replace(m,"{name}",returnName);
					find_and_replace(m,"{slot}","0"); // What if this is not 0! Could be SV_TARGET4!
					find_and_replace(str,"{members}",m+"\n");
					theShader<<str.c_str()<<endl;
					// now replace every instance of return ...; with returnName=...;return;
					regex re("return\\s+(.*);");
					string ret="{";
					// CHECK
					ret += returnName + "=$1;}";
					//ret+=returnName+"=$1;return;}";
					content = std::regex_replace(content, re, ret);
				}
				// We are returning a struct?
				else
				{
					auto outStructDec = declarations.find(function->returnType);
					if (outStructDec != declarations.end())
					{
						auto s = (Struct*)outStructDec->second;
						content += "\n";
						for (auto member : s->m_structMembers)
						{
							string str;
							if (member.semantic.size() == std::string("SV_TARGET688366XX").size() && sfxConfig.pixelOutputDeclarationDSB.length() > 0 )
								str = sfxConfig.pixelOutputDeclarationDSB;
							else
								str = sfxConfig.pixelOutputDeclaration;

							string m;
							process_member_decl(str, m);
							// Declare the output
							string returnName = "returnObject_" + member.type;
							find_and_replace(m, "{type}", member.type);
							find_and_replace(m, "{name}", member.name);
							std::string slotidx;
							int cnt = 0;
							if (member.semantic == "SV_TARGET")
							{
								slotidx = "0";
							}
							else if (member.semantic == "SV_DEPTH")
							{
								// TO-DO: replace with gl_Depth = value;
                                slotidx = "1";
                                content += "gl_FragDepth = tmp." + member.name + ";\n";
                                
							}
							else if (member.semantic.size() == std::string("SV_TARGET688366XX").size()) //SV_TARGET6883660##n for Dual Source Blending
							{
								char checkDSB[3];
								checkDSB[0] = (char)(10 * atoi(member.semantic.substr(9 , 1).c_str())) + atoi(member.semantic.substr(10, 1).c_str());
								checkDSB[1] = (char)(10 * atoi(member.semantic.substr(11, 1).c_str())) + atoi(member.semantic.substr(12, 1).c_str());
								checkDSB[2] = (char)(10 * atoi(member.semantic.substr(13, 1).c_str())) + atoi(member.semantic.substr(14, 1).c_str());

								if (checkDSB[0] != 'D' || checkDSB[1] != 'S' || checkDSB[2] != 'B')
									std::cerr << "SV_TARGET index is not a valid for dual source blending: " << member.semantic << std::endl;

								slotidx.append(member.semantic.substr(16, 1));

								std::string dsbIndex;
								dsbIndex.append(member.semantic.substr(15, 1));
								find_and_replace(m, "{id}", dsbIndex);
							}
							else
							{
								// Here we do some magic to find out the output slot (SV_TARGETX)
								slotidx.append(&member.semantic.at(member.semantic.size() - 1));
							}
							// TO-DO: find a way of implementing this
							find_and_replace(m, "{slot}", slotidx);
							//find_and_replace(m, "{slot}", std::to_string(cnt));
							find_and_replace(str, "{members}", m + "\n");
							theShader << str.c_str() << endl;
							content += member.name + " = tmp." + member.name + ";\n";

							cnt++;
						}
						find_and_replace(content, "return", function->returnType + " tmp = ");
					}
					else
					{
						// error
						std::cerr << "Could not find the output declaration:" << function->returnType << std::endl;
					}
				}
			}
		}
		else if(sfxConfig.vertexOutputDeclaration.length()>0)
		{
			Declaration* retDec = nullptr;
			auto retEntry = declarations.find(function->returnType);
			// replace each return statement with a block that writes
			string vertexOutputDeclaration=sfxConfig.vertexOutputDeclaration;
			find_and_replace(vertexOutputDeclaration,"{blockname}",blockname);
			string m;
			// extract and replace the member declaration.
			process_member_decl(vertexOutputDeclaration,m);
			string members;
			// if the member declaration contains member_name, we will declar the struct members individually, instead of passing a whole structure.
			// This alternative approach is to get around Qualcomm's broken SPIR-V drivers for Adreno 600-series devices, e.g. Oculus Quest etc.
			// NOTE(NACHO): we want to match io block member names...
			bool as_blocks=true;
			if(m.find("{member_name}")<m.length()&&retEntry != declarations.end())
			{
				as_blocks=false;
				retDec = retEntry->second;
				int slot=1;
				if (retDec && retDec->declarationType == DeclarationType::STRUCT)
				{
					Struct* retStruct = (Struct *)retDec;
					for (auto j : retStruct->m_structMembers)
					{
						string this_member_decl=m;
						find_and_replace(this_member_decl,"{member_type}",j.type);
						find_and_replace(this_member_decl,"{member_name}",j.name);
						find_and_replace(this_member_decl,"{slot}",std::to_string(slot++)); // TODO: What if this is not 0! 
						members+=this_member_decl+"\n";
					}
				}
				m=members;
			}
			else
			{
				members=m;
			}
			string returnName = "BlockData";
			find_and_replace(members,"{type}",function->returnType);
			find_and_replace(members,"{name}",returnName);
			find_and_replace(vertexOutputDeclaration,"{members}",members+"\n");
			
			find_and_replace(vertexOutputDeclaration,"{slot}","0"); // TODO: What if this is not 0! 

			// Now construct the replacement for the return command.
			string ret = "{\n";
			regex re("return\\s+(.*);");
			// if we're returning by block
			// 
			// Now replace every instance of return ...; with returnName=...;return;
			if(as_blocks)
			{
				ret += (blockname + ".") + returnName + "=$1;";
			}
			else
			{
			// If we're filling individual elements:
			}

			// If required, write to global outputs
			if (retEntry != declarations.end())
			{
				retDec = retEntry->second;
				if (retDec && retDec->declarationType == DeclarationType::STRUCT)
				{
					Struct* retStruct = (Struct *)retDec;
					for (auto j : retStruct->m_structMembers)
					{
						if(!as_blocks)
						{
							string this_member_fill= function->returnType+returnName+j.name + "=" + "$1" + "." + j.name + ";\n";
							ret += this_member_fill;
						}

						// And fill in the "magic GLSL outputs":
						auto sem = sfxConfig.vertexOutputAssignment.find(j.semantic);
						if (sem != sfxConfig.vertexOutputAssignment.end())
						{
							std::string code;
							if(sfxConfig.reverseVertexOutputY&&is_equal(j.semantic,"SV_POSITION"))
							{
								code += "\n" + sem->second + "=" + "vec4($1." + j.name + ".x,-$1." + j.name + ".y,$1."+j.name+".z,$1."+j.name+".w);\n";
							}
							else
							{
								code += "\n" + sem->second + "=" + "$1" + "." + j.name + ";\n";
							}
							ret += code;
						}
					}
				}
			}

			ret += "\n}";

			content = std::regex_replace(content, re, ret);
			theShader<<vertexOutputDeclaration.c_str()<<endl;
		}
	}
	// Add the CS layout:
	if (shaderInstance->shaderType == COMPUTE_SHADER)
	{
		std::string csLayout = gEffect->m_cslayout[shaderName];
		// Convert the compute layout
		if (sfxConfig.computeLayout.size() > 0)
		{
			std::string cur;
			std::vector < std::string> items;
			bool isReading = false;
			// First we get all the values
			for (auto c : csLayout)
			{
				if (c == '(')
				{
					isReading = true;
					continue;
				}
				if (c == ',' || c == ')')
				{
					items.push_back(cur);
					cur.clear();
					continue;
				}
				if (isReading)
				{
					cur += c;
				}
			}
			csLayout = sfxConfig.computeLayout;
			find_and_replace(csLayout, "$1", items[0]);
			find_and_replace(csLayout, "$2", items[1]);
			find_and_replace(csLayout, "$3", items[2]);
		}

		theShader<< csLayout <<"\n";
	}
	if (shaderInstance->shaderType == RAY_GENERATION_SHADER)
	{
		theShader<<"[shader(\"raygeneration\")]\n";
	}
	if (shaderInstance->shaderType == CLOSEST_HIT_SHADER)
	{
		theShader<<"[shader(\"closesthit\")]\n";
	}
	if (shaderInstance->shaderType == ANY_HIT_SHADER)
	{
		theShader<<"[shader(\"anyhit\")]\n";//sfxConfig
	}
	if (shaderInstance->shaderType == MISS_SHADER)
	{
		theShader<<"[shader(\"miss\")]\n";//sfxConfig
	}
	if (shaderInstance->shaderType == INTERSECTION_SHADER)
	{
		theShader << "[shader(\"intersection\")]\n";//sfxConfig
	}
	if (shaderInstance->shaderType == CALLABLE_SHADER)
	{
		theShader<<"[shader(\"callable\")]\n";//sfxConfig
	}
	// Only if COMPILED as a GS, not VS streamout.
	if (shaderInstance->shaderType == GEOMETRY_SHADER)
	{
		theShader << gEffect->m_gslayout[shaderName] << "\n";
	}
	// Add the root signature:
	if (!sfxConfig.graphicsRootSignatureSource.empty())
	{
		theShader << "#define GFXRS \"RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT), DescriptorTable(CBV(b0, numDescriptors = 14),SRV(t0, numDescriptors = 24),UAV(u0, numDescriptors = 16)),DescriptorTable(Sampler(s0, numDescriptors = 16))\"\n";
		theShader << "[RootSignature(GFXRS)]\n";
	}
	// Add entry declaration:
	theShader << dec << endl;
	// Add main function content:
	theShader << "{\n" << content << "\n}";

	shaderInstance->m_augmentedSource = theShader.str();
}
