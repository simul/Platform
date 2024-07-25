#pragma once
#include <set>
#include "Sfx.h"
#include "SfxParser.h"
#include "Function.h"
#include "ShaderInstance.h"

namespace sfx
{
	struct ResourceGroupDeclaration
	{
		void clear()
		{
			textures.clear();
			constantBuffers.clear();
			structuredBuffers.clear();
			samplers.clear();
		}
		std::set<int> textures;
		std::set<int> constantBuffers;
		std::set<int> structuredBuffers;
		std::set<int> samplers;
	};
	typedef std::map<std::string,std::vector<Function*> > FunctionMap;
	typedef std::map<std::string,std::shared_ptr<ShaderInstance>> ShaderInstanceMap;
	typedef std::map<std::string,std::string> StringMap;
	struct TechniqueGroup
	{
		std::map<std::string, Technique*> m_techniques;
	};
	//! e.g. {var1_option1,var1_option2}
	struct VariantVariableSpec
	{
		std::vector<std::string> variableOptions;
		void clear()
		{
			variableOptions.clear();
		}
	};
	//! e.g. base_shader({var1_option1,var1_option2},{var2_option1,var2_option2})
	struct VariantSpec
	{
		std::vector<VariantVariableSpec> variableSpecs;
		void clear()
		{
			variableSpecs.clear();
		}
	};
	class Effect
	{
		std::map<std::string,Declaration *>		declarations;
		std::map<std::string, Pass*>			m_programs;
		std::vector<std::string>				m_programNames;
		std::map<std::string, TechniqueGroup>	m_techniqueGroups;
		FunctionMap								m_declaredFunctions;
		std::map<std::string,Struct*>			m_structs;
		std::map<std::string, ConstantBuffer*>	m_constantBuffers;

		//! Holds SRVs and UAVs (UAVs will have a slot >= 1000)
		std::map<int, DeclaredTexture*>			m_declaredTexturesByNumber;
		std::map<std::string,int>				textureNumberMap;
		std::map<std::string,int>				rwTextureNumberMap;

		std::map<std::string,std::string>			m_cslayout;
		std::map<std::string,std::string>			m_gslayout;
		std::set<std::shared_ptr<ShaderInstance>>	m_uniqueShaderInstances;
		ShaderInstanceMap							m_shaderInstances;

		std::map<int, ResourceGroupDeclaration> resourceGroups;
		struct InterfaceDcl
		{
			std::string id;
			int atLine;

			InterfaceDcl(std::string s, int l) : id(s), atLine(l) {}
			InterfaceDcl() {}
		};
		std::map<std::string, InterfaceDcl>	m_interfaces;
		std::vector<std::string>	m_filenames;
		std::ostringstream			m_log;
		int							m_includes;
		bool						m_active;
		std::string					m_dir;
		std::string					m_filename;
		SfxConfig					sfxConfig;
		SfxOptions					sfxOptions;

		/// For a given shader compilation, work out which slots it uses.
		void CalculateResourceSlots(ShaderInstance *shaderInstance,std::set<int> &textureSlots,std::set<int> &uavSlots
			,std::set<int> &textureSlotsForSB, std::set<int> &uavTextureSlotsForSB
			,std::set<int> &bufferSlots,std::set<int> &samplerSlots);
		std::map<std::string,int> filenumbers;
		std::map<int,std::string> fileList;
		mutable int last_filenumber;
		bool CheckDeclaredGlobal(const Function* func,const std::string toCheck);
		//! Declare in output format.
		void Declare( ShaderInstance *shaderInstance, std::ostream &os, const Declaration *d, ConstantBuffer &texCB, ConstantBuffer &sampCB, const std::set<std::string> &rwLoad, std::set<const SamplerState *> &declaredSS, const Function *mainFunction);

		void CheckTextureSlotForResourceGroup(int slot, int group);
		void CheckConstantBufferSlotForResourceGroup(int slot, int group);
		void CheckStructuredBufferSlotForResourceGroup(int slot, int group);
	public:
		std::ostringstream m_sharedCode;
		std::ostringstream& Log();
		std::string CombinedTypeString(const std::string& type, const std::string& memberType);
		TechniqueGroup &GetTechniqueGroup(const std::string &name);
		const std::vector<std::string>& GetProgramList() const;
		const std::vector<std::string>& GetFilenameList() const;
		void SetFilenameList(const char **);
		void PopulateProgramList();
		bool Save(std::string sfxFilename, std::string sfxoFilename);
		void AccumulateStructDeclarations(std::map<std::string, std::string> &variantValues,std::set<const Declaration *> &s, std::string i) const;
		void AccumulateDeclarationsUsed(const Function *f, std::map<std::string, std::string> &variantValues, std::set<const Declaration *> &ss, std::set<std::string> &rwLoad) const;
		void AccumulateFunctionsUsed(const Function *f, std::map<std::string, std::string> &variantValues, std::set<const Function *> &s) const;
		void AccumulateFunctionsUsed2(const Function *f, std::map<std::string, std::string> &variantValues,std::set<const Function *> &s) const;
		void AccumulateGlobals(const Function *f,std::set<const Variable *> &s) const;
		void AccumulateGlobalsAsStrings(const Function* f, std::set<std::string>& s) const;
		void AccumulateConstantBuffersUsed(const Function *f, ShaderInstance &sh) const;
		unsigned CompileAllShaders(std::string sfxoFilename,const std::string &sharedCode, std::string& log, BinaryMap &binaryMap);

		/// Adapt slot number in case slots can't be shared.
		int GenerateSamplerSlot(int s,bool offset=true);
		/// Adapt slot number in case slots can't be shared.
		int GenerateTextureSlot(int s,bool offset=true);
		/// Adapt slot number in case slots can't be shared.
		int GenerateTextureWriteSlot(int s,bool offset=true);
		/// Adapt slot number in case slots can't be shared.
		int GenerateConstantBufferSlot(int s,bool offset=true);

		bool& Active();
		std::string& Dir();
		std::string& Filename();
		~Effect();
		Effect();
		const SfxConfig *GetConfig() const
		{
			return &sfxConfig;
		}
		const SfxOptions *GetOptions()
		{
			return &sfxOptions;
		}
		void SetConfig(const SfxConfig *config);
		void SetOptions(const SfxOptions *opts);

		const std::map<std::string, std::vector<Function*> > &GetFunctions() const
		{
			return m_declaredFunctions;
		}
		const std::map<std::string, Declaration*> &GetDeclarations() const
		{
			return declarations;
		}
		Declaration *GetDeclaration(const std::string &name)
		{
			auto i=declarations.find(name);
			if(i!=declarations.end())
				return i->second;
			return nullptr;
		}
		NamedConstantBuffer *GetNamedConstantBufferForMember(const std::string &name)
		{
			Declaration *d=GetDeclaration(name);
			if(d->declarationType==DeclarationType::NAMED_CONSTANT_BUFFER)
				return static_cast<NamedConstantBuffer*>(d);
			return nullptr;
		}
		ConstantBuffer *GetConstantBufferForMember(const std::string &member)
		{
			for (auto i : m_constantBuffers)
			{
				for (auto j : i.second->m_structMembers)
				{
					// Array members of the constant buffer are stored with the full name
					// Ex: matTable[6], but when we call this function we only call it with
					// the name (without the [x]) so we have to check that
					std::size_t foundPos = j.name.find('[');
					if (foundPos != std::string::npos)
					{
						std::string newName = j.name;
						newName.erase(foundPos);
						if(newName == member)
							return i.second;
					}
					if (j.name == member)
						return i.second;
				}
			}
			return nullptr;
		}
		/// Get the named instance, or create it if it's a function without a default instance.
		std::shared_ptr<ShaderInstance> GetShaderInstance(const std::string &functionName,ShaderType type);
		void ConstructSource(ShaderInstance *cs);
		std::shared_ptr<ShaderInstance> AddShaderInstance(const std::string &name,const std::string &functionName,ShaderType type,const std::string &profileName,int lineno);
		std::vector<std::string> GenerateShaderInstanceVariants(const std::string &basename,const VariantSpec &shaderInstances);
		void DeclareStruct(const std::string &name, const Struct &ts,const std::string &original);
		void DeclareConstantBuffer(const std::string &name, int slot,int group_num,const Struct &ts,const std::string &original);
		void DeclareVariable(const Variable *v);
		bool IsDeclared(const std::string &name);
		bool IsConstantBufferMemberAlignmentValid(const Declaration* d);
		std::string GetTypeOfParameter(std::vector<sfxstype::variable>& parameters, std::string keyName);
		std::string GetDeclaredType(const std::string &str) const;
		int GetRWTextureNumber(std::string n, int specified_slot=-1);
		int GetTextureNumber(std::string n, int specified_slot=-1);
		NamedConstantBuffer* DeclareNamedConstantBuffer(const std::string &name,int slot,int group,int space,const std::string &structureType, const Struct& ts,const std::string &original);
		DeclaredTexture *DeclareTexture(const std::string &name, ShaderResourceType shaderResourceType, int slot, int group, int space, const std::string &templatedType, const std::string &original);
		SamplerState *DeclareSamplerState(const std::string &name, int register_number, int group_number, const SamplerState &templateSS);
		RasterizerState *DeclareRasterizerState(const std::string &name);
		RenderTargetFormatState *DeclareRenderTargetFormatState(const std::string &name);
		BlendState *DeclareBlendState(const std::string &name);
		DepthStencilState *DeclareDepthStencilState(const std::string &name);
		Function *DeclareFunction(const std::string &functionName, Function &buildFunction);
		void DeclareResourceGroup(int num, const ResourceGroupDeclaration &g);
		//! Get the function, including variant specifications to create a specialized function if necessary.
		Function *GetFunction(const std::string &functionName,  std::map<std::string, std::string> &variantValues);
		Function *GetFunction(const std::string &functionName, int i);
		const Function *GetFunction(const std::string &functionName, int i) const;
		int GetSlot(const std::string &variableName) const;
		int GetFilenumber(const std::string &fn) ;
		std::string GetFilenameOrNumber(const std::string &fn) ;
		std::string GetFilenameOrNumber(int filenumber) ;
		std::string GetFilename(int filenumber) ;
		friend int ::sfxparse();
		friend int ::sfxlex();
		int							current_texture_number;
		int							current_rw_texture_number;
		int							m_max_sampler_register_number;
	};
	extern Effect *gEffect;
}
