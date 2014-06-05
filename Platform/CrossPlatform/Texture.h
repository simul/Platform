#pragma once
#include "Simul/Platform/CrossPlatform/Export.h"
namespace simul
{
	namespace crossplatform
	{
		class SIMUL_CROSSPLATFORM_EXPORT Texture
		{
		public:
			virtual ~Texture();
			virtual void LoadFromFile(const char *pFilePathUtf8)=0;
			virtual bool IsValid() const=0;
			virtual void InvalidateDeviceObjects()=0;
			virtual void *AsVoidPointer()=0;
		};
	}
}
