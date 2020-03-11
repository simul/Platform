#pragma once
#include <vulkan/vulkan.hpp>
#include "Platform/Vulkan/Export.h"
#include "Platform/CrossPlatform/Effect.h"

#ifdef _MSC_VER
	#pragma warning(push)
	#pragma warning(disable:4251)
#endif

namespace simul
{
	namespace vulkan
	{
        //! A Vulkan shader
		class SIMUL_VULKAN_EXPORT Shader:public simul::crossplatform::Shader
		{
		public:
                    Shader();
                    ~Shader();
			void load(crossplatform::RenderPlatform *r, const char *filename_utf8, const void *data, size_t len, crossplatform::ShaderType t) override;
            vk::ShaderModule mShader;
        private: 
            void    Release() override;
			//temp:
			std::string src;
		};
	}
}

#ifdef _MSC_VER
	#pragma warning(pop)
#endif