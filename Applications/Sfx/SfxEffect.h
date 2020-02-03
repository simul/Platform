#ifndef PSFXEFFECT_H
#define PSFXEFFECT_H
#include <set>
#include "Sfx.h"
#include "SfxParser.h"

namespace sfx
{
	struct Function
	{
		void clear()
		{
			functionsCalled.clear();
			returnType.clear();
			name.clear();
			content.clear();
			parameters.clear();		
			globals.clear();
			rwTexturesLoaded.clear();
			localTextures.clear();
			resources.clear();
			declaration.clear();
			constantBuffers.clear();
			types_used.clear();
			locals.clear();
			main_linenumber=0;
			min_parameters = 0;
			initialized=false;
		}
		std::string						 declaration;
		std::set<std::string>			declarations;
		std::set<Function*>				functionsCalled;
		std::set<ConstantBuffer*>		constantBuffers;
		const std::set<std::string>&	GetTypesUsed() const;
		std::string						returnType;
		std::string						name;
		std::string						content;
		//! textures, samplers, buffers. To be divided into parameters and globals when we read the signature
		//! of the function.
		std::set<std::string>			resources;			
		int								min_parameters;
		//! Passed in from the caller.
		std::vector<sfxstype::variable> parameters;		
		//! Passed in from the caller.
		std::vector<sfxstype::variable> locals;		
		//! Referenced but not passed - must be global.
		std::set<std::string>			globals;					
		//! RW Textures used in load operations
		std::set<std::string>			rwTexturesLoaded;		 
		//! Holds a set of local textures.
		std::set<std::string>			localTextures;			
		
		int								main_linenumber;
		int								local_linenumber;
		std::string						filename;
	protected:
		mutable std::set<std::string>	types_used;
		mutable bool					initialized;
	};

	//! A shader to be compiled. 
	struct CompiledShader
	{
		CompiledShader(const std::set<Declaration*> &d);
		CompiledShader(const CompiledShader &cs);
		ShaderType shaderType;
		std::string m_profile;
		std::string m_functionName;
		std::string m_preamble;
		std::string m_augmentedSource;
		std::string entryPoint;
		std::map<int,std::string> sbFilenames;// maps from PixelOutputFormat for pixel shaders, or int for vertex(0) and export(1) shaders.
		std::set<Declaration*> declarations;
		std::set<ConstantBuffer*> constantBuffers;
		int global_line_number;
		std::string rtFormatStateName;
	};
	typedef std::map<std::string,std::vector<Function*> > FunctionMap;
	typedef std::map<std::string,CompiledShader*> CompiledShaderMap;
	typedef std::map<std::string,std::string> StringMap;
	struct TechniqueGroup
	{
		std::map<std::string, Technique*> m_techniques;
	};
	class Effect
	{
		std::map<std::string,Declaration *>		declarations;
		std::map<std::string, Pass*>			m_programs;
		std::vector<std::string>						m_programNames;
		std::map<std::string, TechniqueGroup>		m_techniqueGroups;
		FunctionMap								m_declaredFunctions;
		std::map<std::string,Struct*>			m_structs;
		std::map<std::string, ConstantBuffer*>	m_constantBuffers;

		//! Holds SRVs and UAVs (UAVs will have a slot >= 1000)
		std::map<int, DeclaredTexture*>			m_declaredTexturesByNumber;
		std::map<std::string,int>				textureNumberMap;
		std::map<std::string,int>				rwTextureNumberMap;

		std::map<std::string,std::string>			m_cslayout;
		std::map<std::string,std::string>			m_gslayout;
		CompiledShaderMap						m_compiledShaders;
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
		void CalculateResourceSlots(CompiledShader *compiledShader,std::set<int> &textureSlots,std::set<int> &uavSlots
			,std::set<int> &textureSlotsForSB, std::set<int> &uavTextureSlotsForSB
			,std::set<int> &bufferSlots,std::set<int> &samplerSlots);
		std::map<std::string,int> filenumbers;
		std::map<int,std::string> fileList;
		mutable int last_filenumber;
		bool CheckDeclaredGlobal(const Function* func,const std::string toCheck);
		//! Declare in output format.
		void Declare(std::ostream &os,const Declaration *d, ConstantBuffer& texCB, ConstantBuffer& sampCB, const std::set<std::string>& rwLoad, std::set<const SamplerState*>& declaredSS,const Function* mainFunction);
	public:
		std::ostringstream m_sharedCode;
		std::ostringstream& Log();
		TechniqueGroup &GetTechniqueGroup(const std::string &name);
		const std::vector<std::string>& GetProgramList() const;
		const std::vector<std::string>& GetFilenameList() const;
		void SetFilenameList(const char **);
		void PopulateProgramList();
		bool Save(std::string sfxFilename, std::string sfxoFilename);
		void AccumulateDeclarationsUsed(const Function *f,std::set<const Declaration *> &ss, std::set<std::string>& rwLoad) const;
		void AccumulateFunctionsUsed(const Function *f,std::set<const Function *> &s) const;
		void AccumulateGlobals(const Function *f,std::set<std::string> &s) const;
		void AccumulateConstantBuffersUsed(const Function *f, std::set<ConstantBuffer*> &s) const;
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
		void ConstructSource(CompiledShader *cs);
		CompiledShader *AddCompiledShader(const std::string &name,std::set<Declaration *> declarations);
		void DeclareStruct(const std::string &name, const Struct &ts,const std::string &original);
		void DeclareConstantBuffer(const std::string &name, int slot,const Struct &ts,const std::string &original);
		bool IsDeclared(const std::string &name);
		std::string GetTypeOfParameter(std::vector<sfxstype::variable>& parameters, std::string keyName);
		std::string GetDeclaredType(const std::string &str) const;
		int GetRWTextureNumber(std::string n, int specified_slot=-1);
		int GetTextureNumber(std::string n, int specified_slot=-1);
		DeclaredTexture *DeclareTexture(const std::string &name, ShaderResourceType shaderResourceType, int slot,const std::string &templatedType,const std::string &original);
		SamplerState *DeclareSamplerState(const std::string &name,int reg,const SamplerState &templateSS);
		RasterizerState *DeclareRasterizerState(const std::string &name);
		RenderTargetFormatState *DeclareRenderTargetFormatState(const std::string &name);
		BlendState *DeclareBlendState(const std::string &name);
		DepthStencilState *DeclareDepthStencilState(const std::string &name);
		Function* DeclareFunction(const std::string &functionName,Function &buildFunction);
		Function* GetFunction(const std::string &functionName,int i);
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
#endif