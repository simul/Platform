#pragma once
#include "Simul/Platform/CrossPlatform/Export.h"
#include "Simul/Platform/CrossPlatform/SL/CppSl.hs"
#include "Simul/Base/FileLoader.h"
#include "Simul/Base/MemoryInterface.h"
#include <map>
#include <string>
#include <vector>
#ifdef _MSC_VER
    #pragma warning(push)
    #pragma warning(disable:4251)
    #pragma warning(disable:4275)
#endif
namespace simul
{
	namespace crossplatform
	{
		class TextInput
		{
		public:

			DEFINE_NEW_DELETE_OVERRIDES;

			virtual ~TextInput(){}
			virtual bool Good()=0;
			//! Text value of the specified element.
			virtual const char *Get(const char *name,const char *default_)=0;
			//! Boolean value of the specified element.
			virtual bool Get(const char *name,bool default_)=0;
			//! Integer value of the specified element.
			virtual int Get(const char *name,int default_)=0;
			//! Floating-point value of the specified element.
			virtual double Get(const char *name,double default_)=0;
			//! Floating-point value of the specified element.
			virtual float Get(const char *name,float default_)=0;
			//! Floating-point value of the specified element.
			virtual vec4 Get(const char *name,vec4 default_)=0;
			//! The property at the given index. type should be Variant::UNKNOWN at the end of the list.
			virtual const char *Get(int propertyIndex)=0;
			//! Sub-element with the given name. If null, the Value() should be non-null.
			virtual TextInput *GetSubElement(const char *name)=0;
			typedef std::vector<TextInput*> Array;
			virtual Array &GetArray(const char *name)=0;
			virtual bool HasArray(const char *name) const=0;
		};
		class TextOutput
		{
		public:

			DEFINE_NEW_DELETE_OVERRIDES;

			virtual ~TextOutput(){}
			virtual bool Good()=0;
			// Name of the specified element
			virtual void Set(const char *name,const char *value)=0;
			virtual void Set(const char *name,bool value)=0;
			virtual void Set(const char *name,int value)=0;
			virtual void Set(const char *name,double value)=0;
			virtual void Set(const char *name,float value)=0;
			// Floating-point value of the specified element.
			virtual void Set(const char *name,vec4 value)=0;
			// Make a sub-element with the given name.
			virtual TextOutput *CreateSubElement(const char *name)=0;
			typedef std::vector<TextOutput*> Array;
			virtual Array &GetArray(const char *name)=0;
			virtual Array &CreateArray(const char *name,int size)=0;
		};

		class SIMUL_CROSSPLATFORM_EXPORT TextFileInput:public TextInput
		{
		public:
			TextFileInput(simul::base::MemoryInterface *m=NULL);
			virtual ~TextFileInput();
			void SetFileLoader(simul::base::FileLoader *f);
			void Load(const char *filename);
			void Load(const std::string &text);
			bool Good();
			// Text value of the specified element.
			const char *Get(const char *name,const char *default_);
			// Boolean value of the specified element.
			bool Get(const char *name,bool default_);
			// Integer value of the specified element.
			int Get(const char *name,int default_);
			// Floating-point value of the specified element.
			double Get(const char *name,double default_);
			// Floating-point value of the specified element.
			float Get(const char *name,float default_);
			// Floating-point value of the specified element.
			vec4 Get(const char *name,vec4 default_);
			virtual const char *Get(int propertyIndex);
			// Sub-element with the given name. If null, the Value() should be non-null.
			TextInput *GetSubElement(const char *name);
			Array &GetArray(const char *name);
			bool HasArray(const char *name) const;
			//std::string text;
			std::map<std::string,std::string> properties;
			std::map<std::string,TextFileInput> subElements;
			std::map<std::string,Array> arrays;
		private:
			bool good;
			simul::base::FileLoader *fileLoader;
			simul::base::MemoryInterface *memoryInterface;
		};
		class SIMUL_CROSSPLATFORM_EXPORT TextFileOutput:public TextOutput
		{
		public:
			TextFileOutput(simul::base::MemoryInterface *m=NULL);
			virtual ~TextFileOutput();
			void Save(const char *filename_utf8);
			void Save(std::ostream &ofs,int tab=0);
			bool Good();
			// Name of the specified element
			void Set(const char *name,const char *value);
			void Set(const char *name,bool value);
			void Set(const char *name,int value);
			void Set(const char *name,double value);
			void Set(const char *name,float value);
			// Floating-point value of the specified element.
			void Set(const char *name,vec4 value);
			// Make a sub-element with the given name.
			TextOutput *CreateSubElement(const char *name);
			Array &GetArray(const char *name);
			Array &CreateArray(const char *name,int size);
			std::string getText();
			std::map<std::string,std::string> properties;
			std::map<std::string,TextFileOutput> subElements;
			std::map<std::string,Array> arrays;
		private:
			simul::base::MemoryInterface *memoryInterface;
		};
	}
}

#ifdef _MSC_VER
    #pragma warning(pop)
#endif
