#pragma once
#include <set>
#include "Sfx.h"
#include "SfxParser.h"

namespace sfx
{
	struct VariantState
	{	
		std::vector<std::string> states;
	};
	struct Function
	{
		void clear()
		{
			functionsCalled.clear();
			returnType.clear();
			name.clear();
			content.clear();
			parameters.clear();
			variant_parameters.clear();
			globals.clear();
			rwTexturesLoaded.clear();
			localTextures.clear();
			resources.clear();
			declaration.clear();
			declarations.clear();
			constantBuffers.clear();
			types_used.clear();
			locals.clear();
			main_linenumber=0;
			min_parameters = 0;
			initialized=false;
			numThreads[0] = 0;
			numThreads[1] = 0;
			numThreads[2] = 0;
		}
		std::string name;
		std::string						declaration;
		std::set<std::string>			declarations;
		struct FunctionCall
		{
			std::set<std::string> variant_conditions;
		};
		void AddFunctionCall(Function*f)
		{
			auto &fnc=functionsCalled[f];
			for(auto c:variant_conditions)
			{
				fnc.variant_conditions.insert(c);
			}
		}
		void declareResource(std::string r)
		{
			// current variant state. An empty list means it always matches.
			VariantState variantState;
			variantState.states=variant_conditions;
			resources[r].push_back(variantState);
		}
		void declareConstantBuffer(ConstantBuffer *cb)
		{
			VariantState variantState;
			variantState.states = variant_conditions;
			constantBuffers[cb].push_back(variantState);
		}
		std::vector<std::string> variant_conditions;
		std::map<Function *, FunctionCall> functionsCalled;
		std::map<ConstantBuffer *, std::vector<VariantState>> constantBuffers;
		const std::set<std::string>&	GetTypesUsed() const;
		std::string						returnType;
		std::string						content;
		//! textures, samplers, buffers. To be divided into parameters and globals when we read the signature
		//! of the function.
		//! The variantstates are all the possible combinations that this resource appears with. It's empty if there are no variant conditions
		//! attached to the resource.
		std::map<std::string,std::vector<VariantState>>			resources;			
		int								min_parameters;
		//! Passed in from the caller.
		std::vector<sfxstype::variable> parameters;		
		//! Passed in from the variant generator.
		std::vector<sfxstype::variable> variant_parameters;		
		//! Passed in from the caller.
		std::vector<sfxstype::variable> locals;		
		//! Referenced but not passed - must be global.
		std::map<std::string, std::vector<VariantState>> globals;					
		//! RW Textures used in load operations
		std::set<std::string>			rwTexturesLoaded;		 
		//! Holds a set of local textures.
		std::set<std::string>			localTextures;			
		
		int								main_linenumber;
		int								local_linenumber;
		std::string						filename;
		int								numThreads[3];

		//! Where a function has lambda [] parameters or variant inputs, we create specialized versions with values in this variable.
		std::map<std::string, std::string> variantValues;
	protected:
		mutable std::set<std::string>	types_used;
		mutable bool					initialized;
	};
}