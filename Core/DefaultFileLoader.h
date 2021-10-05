#pragma once
#include "Platform/Core/FileLoader.h"
#include "Platform/Core/Export.h"

namespace simul
{
	namespace base
	{
		//! The default derived file loader
		class PLATFORM_CORE_EXPORT DefaultFileLoader:public simul::base::FileLoader
		{
		public:
			DefaultFileLoader();
			bool FileExists(const char *filename_utf8) const override;
			void AcquireFileContents(void*& pointer, unsigned int& bytes, const char* filename_utf8,bool open_as_text) override;
			double GetFileDate(const char* filename_utf8) const override;
			void ReleaseFileContents(void* pointer) override;
			bool Save(void* pointer, unsigned int bytes, const char* filename_utf8,bool save_as_text) override;
		};
	}
}