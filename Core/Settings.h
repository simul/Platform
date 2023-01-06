#pragma once

#include "Platform/Core/SimpleIni.h"
#include "Platform/Core/Export.h"
#ifdef _MSC_VER
#pragma warning(disable:4251)
#endif
namespace platform
{
	namespace core
	{
		// A simple class to save and restore settings.
		class PLATFORM_CORE_EXPORT Settings
		{
		public:
			Settings(const char *filename);
			~Settings();
			void beginGroup(const char *name) const;
			void endGroup() const;
			void setValue(const char *name,int i);
			int getValue(const char *name,int default_i) const;
			void setValue(const char *name,double d);
			double getValue(const char *name,double default_d) const;
			void setValue(const char *name,float f);
			float getValue(const char *name,float default_f) const;
			void setValue(const char *name,const char *txt);
			const char *getValue(const char *name,const char *txt) const;
		protected:
			void LoadFile() const;
			std::string filename;
			mutable bool reading=false;
			mutable std::string currentGroup;
		};
	}
}
