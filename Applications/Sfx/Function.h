#pragma once
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
		std::string						declaration;
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
		//! Passed in from the variant generator.
		std::vector<sfxstype::variable> variant_parameters;		
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
		int								numThreads[3];
	protected:
		mutable std::set<std::string>	types_used;
		mutable bool					initialized;
	};
}