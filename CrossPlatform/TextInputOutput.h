#pragma once
#include "Platform/CrossPlatform/Export.h"
#include "Platform/CrossPlatform/Quaterniond.h"
#include "Platform/CrossPlatform/Shaders/CppSl.sl"
#include "Platform/Core/FileLoader.h"
#include "Platform/Core/MemoryInterface.h"
#include <map>
#include <string>
#include <vector>
#ifdef _MSC_VER
    #pragma warning(push)
    #pragma warning(disable:4251)
    #pragma warning(disable:4275)
#endif
namespace platform
{
	namespace crossplatform
	{
		class TextInput
		{
		public:

			DEFINE_NEW_DELETE_OVERRIDES;

			virtual ~TextInput(){}
			//! Returns true if the file is successfully loaded
			virtual bool Good()=0;
			//! Is the specified element in the list?
			virtual bool Has(const char *name) const=0;
			//! Text value of the specified element.
			virtual const char *Get(const char *name,const char *default_)=0;
			//! Boolean value of the specified element.
			virtual bool Get(const char *name,bool default_)=0;
			//! Integer value of the specified element.
			virtual int Get(const char *name,int default_)=0;
			//! Integer value of the specified element.
			virtual uint Get(const char *name,uint default_)=0;
			//! 64-bit integer value of the specified element.
			virtual long long Get(const char* name, long long default_) = 0;
			//! 64-bit integer value of the specified element.
			virtual unsigned long long Get(const char* name, unsigned long long default_) = 0;
			//! Floating-point value of the specified element.
			virtual double Get(const char *name,double default_)=0;
			//! Floating-point value of the specified element.
			virtual float Get(const char *name,float default_)=0;
			//! Integer value of the specified element.
			virtual uint2 Get(const char* name,uint2 default_)=0;
			//! Integer value of the specified element.
			virtual uint3 Get(const char* name,uint3 default_)=0;
			//! Integer value of the specified element.
			virtual uint4 Get(const char* name,uint4 default_)=0;
			//! Integer value of the specified element.
			virtual int2 Get(const char *name,int2 default_)=0;
			//! Integer value of the specified element.
			virtual int3 Get(const char *name,int3 default_)=0;
			//! Integer value of the specified element.
			virtual int4 Get(const char *name,int4 default_)=0;
			//! Floating-point value of the specified element.
			virtual vec2 Get(const char *name,vec2 default_)=0;
			//! Floating-point value of the specified element.
			virtual vec3 Get(const char *name,vec3 default_)=0;
			//! Floating-point value of the specified element.
			virtual vec4 Get(const char *name,vec4 default_)=0;
			//! Floating-point value of the specified element.
			virtual Quaterniond Get(const char *name,Quaterniond default_)=0;
			//! The property at the given index. type should be Variant::UNKNOWN at the end of the list.
			virtual const char *Get(int propertyIndex)=0;
			//! Sub-element with the given name. If null, the Value() should be non-null.
			virtual TextInput *GetSubElement(const char *name)=0;
			//! Sub-element with the given index, starting at zero. If null, it's past the end of the list
			virtual const char *GetSubElement(int)=0;
			typedef std::vector<TextInput*> Array;
			virtual Array &GetArray(const char *name)=0;
			virtual const Array &GetArray(const char *name) const=0;
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
			virtual void Set(const char *name,uint value)=0;
			virtual void Set(const char* name,long long value) = 0;
			virtual void Set(const char* name,unsigned long long value) = 0;
			virtual void Set(const char *name,double value)=0;
			virtual void Set(const char *name,float value)=0;
			// Unsigned Integer value of the specified element.
			virtual void Set(const char *name,uint2 value)=0;
			// Unsigned Integer value of the specified element.
			virtual void Set(const char *name,uint3 value)=0;
			// Unsigned Integer value of the specified element.
			virtual void Set(const char *name,uint4 value)=0;
			// Integer value of the specified element.
			virtual void Set(const char *name,int2 value)=0;
			// Integer value of the specified element.
			virtual void Set(const char *name,int3 value)=0;
			// Integer value of the specified element.
			virtual void Set(const char *name,int4 value)=0;
			// Floating-point value of the specified element.
			virtual void Set(const char *name,vec2 value)=0;
			// Floating-point value of the specified element.
			virtual void Set(const char *name,vec3 value)=0;
			// Floating-point value of the specified element.
			virtual void Set(const char *name,vec4 value)=0;
			// Floating-point value of the specified element.
			virtual void Set(const char *name,Quaterniond value)=0;
			// Make a sub-element with the given name.
			virtual TextOutput *CreateSubElement(const char *name)=0;
			typedef std::vector<TextOutput*> Array;
			virtual Array &GetArray(const char *name)=0;
			virtual Array &CreateArray(const char *name,int size)=0;
		};

		class SIMUL_CROSSPLATFORM_EXPORT TextFileInput:public TextInput
		{
		public:
			TextFileInput(platform::core::MemoryInterface *m=NULL);
			virtual ~TextFileInput();
			void SetFileLoader(platform::core::FileLoader *f);
			void LoadFile(const char *filename);
			void LoadText(const std::string &text);
			bool Good();
			//! Is the specified element in the list?
			virtual bool Has(const char *name) const;
			// Text value of the specified element.
			const char *Get(const char *name,const char *default_);
			// Boolean value of the specified element.
			bool Get(const char *name,bool default_);
			// Integer value of the specified element.
			int Get(const char *name,int default_);
			// Integer value of the specified element.
			uint Get(const char *name,uint default_);
			// Integer value of the specified element.
			long long Get(const char* name, long long default_);
			// Integer value of the specified element.
			unsigned long long Get(const char* name, unsigned long long default_);
			// Floating-point value of the specified element.
			double Get(const char *name,double default_);
			// Floating-point value of the specified element.
			float Get(const char *name,float default_);
			// Integer value of the specified element.
			uint2 Get(const char* name, uint2 default_);
			// Integer value of the specified element.
			uint3 Get(const char* name, uint3 default_);
			// Integer value of the specified element.
			uint4 Get(const char* name, uint4 default_);
			// Integer value of the specified element.
			int2 Get(const char* name, int2 default_);
			// Integer value of the specified element.
			int3 Get(const char* name, int3 default_);
			// Integer value of the specified element.
			int4 Get(const char* name, int4 default_);
			// Floating-point value of the specified element.
			vec2 Get(const char *name,vec2 default_);
			// Floating-point value of the specified element.
			vec3 Get(const char *name,vec3 default_);
			// Floating-point value of the specified element.
			vec4 Get(const char *name,vec4 default_);
			// Floating-point value of the specified element.
			Quaterniond Get(const char *name,Quaterniond default_);
			virtual const char *Get(int propertyIndex);
			// Sub-element with the given name. If null, the Value() should be non-null.
			TextInput *GetSubElement(const char *name);
			const char *GetSubElement(int);
			Array &GetArray(const char *name);
			const Array &GetArray(const char *name) const;
			bool HasArray(const char *name) const;
			//std::string text;
			std::map<std::string,std::string> properties;
			std::map<std::string,TextFileInput> subElements;
			std::map<std::string,Array> arrays;
		private:
			bool good;
			platform::core::FileLoader *fileLoader;
			platform::core::MemoryInterface *memoryInterface;
		};
		
		//NOTE: Copy constructor is unimplemented, and the default is shallow.
		class SIMUL_CROSSPLATFORM_EXPORT TextFileOutput:public TextOutput
		{
		public:
			TextFileOutput(platform::core::MemoryInterface *m=NULL);
			virtual ~TextFileOutput();

			void Save(const char *filename_utf8);
			void Save(std::ostream &ofs,int tab=0,bool bookEnd=false);
			bool Good();
			// Name of the specified element
			void Set(const char *name,const char *value);
			void Set(const char *name,bool value);
			void Set(const char *name,int value);
			void Set(const char *name,uint value);
			void Set(const char* name, long long value);
			void Set(const char* name, unsigned long long value);
			void Set(const char *name,double value);
			void Set(const char *name,float value);
			void Set(const char* name, uint2 value);
			void Set(const char* name, uint3 value);
			void Set(const char* name, uint4 value);
			void Set(const char* name, int2 value);
			void Set(const char* name, int3 value);
			void Set(const char* name, int4 value);
			// Floating-point value of the specified element.
			void Set(const char *name,vec2 value);
			// Floating-point value of the specified element.
			void Set(const char *name,vec3 value);
			// Floating-point value of the specified element.
			void Set(const char *name,vec4 value);
			// Floating-point value of the specified element.
			void Set(const char *name,Quaterniond value);
			// Make a sub-element with the given name.
			TextOutput *CreateSubElement(const char *name);
			Array &GetArray(const char *name);
			Array &CreateArray(const char *name,int size);
			std::string getText();
			std::map<std::string,std::string> properties;
			std::map<std::string,TextFileOutput> subElements;
			std::map<std::string,Array> arrays;
		private:
			platform::core::MemoryInterface *memoryInterface;
		};
	}
}

#ifdef _MSC_VER
    #pragma warning(pop)
#endif
