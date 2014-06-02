#pragma once

namespace simul
{
	namespace crossplatform
	{
		class Texture
		{
		public:
			Texture(){}
			virtual ~Texture(){}
			virtual void LoadFromFile(const char *pFilePathUtf8)=0;
			virtual bool IsValid() const=0;
		};
	}
}
