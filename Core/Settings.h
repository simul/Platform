#pragma once
#include "Platform/Core/Export.h"
#include "Platform/Core/SimpleIni.h"
#include "Platform/Core/RuntimeError.h"

#ifdef _MSC_VER
#pragma warning(disable : 4251)
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
			void BeginGroup(const char *name);
			void EndGroup();

			template <typename T>
			void SetValue(const char *name, T value)
			{
				reading = false;

				if (typeid(T) == typeid(double) || typeid(T) == typeid(float))
				{
					ini.SetDoubleValue(currentGroup.c_str(), name, (double)value, nullptr, true);
				}
				else if (typeid(T) == typeid(bool))
				{
					ini.SetBoolValue(currentGroup.c_str(), name, value, nullptr, true);
				}
				else if (typeid(T) == typeid(int8_t) || typeid(T) == typeid(int16_t) || typeid(T) == typeid(int32_t) || typeid(T) == typeid(int64_t) 
					|| typeid(T) == typeid(uint8_t) || typeid(T) == typeid(uint16_t) || typeid(T) == typeid(uint32_t) || typeid(T) == typeid(uint64_t))
				{
					ini.SetLongValue(currentGroup.c_str(), name, static_cast<long>(value), nullptr, false, true);
				}
				else
				{
					SIMUL_BREAK("Unknown Type passed as template argument");
				}
			}
			void SetValue(const char *name, const char *value)
			{
				ini.SetValue(currentGroup.c_str(), name, value, nullptr, true);
			}

			template <typename T>
			T GetValue(const char *name, T defaultValue)
			{
				if (!reading)
				{
					LoadFile();
				}

				if (typeid(T) == typeid(double) || typeid(T) == typeid(float))
				{
					return (T)ini.GetDoubleValue(currentGroup.c_str(), name, (double)defaultValue);
				}
				else if (typeid(T) == typeid(bool))
				{
					return (T)ini.GetBoolValue(currentGroup.c_str(), name, defaultValue);
				}
				else if (typeid(T) == typeid(int8_t) || typeid(T) == typeid(int16_t) || typeid(T) == typeid(int32_t) || typeid(T) == typeid(int64_t) 
					|| typeid(T) == typeid(uint8_t) || typeid(T) == typeid(uint16_t) || typeid(T) == typeid(uint32_t) || typeid(T) == typeid(uint64_t))
				{
					return (T)ini.GetLongValue(currentGroup.c_str(), name, static_cast<long>(defaultValue));
				}
				
				else
				{
					SIMUL_BREAK("Unknown Type passed as template argument");
					return T();
				}
			}
			const char *GetValue(const char *name, const char *defaultValue)
			{
				return ini.GetValue(currentGroup.c_str(), name, defaultValue);
			}

		protected:
			void LoadFile();
			void SaveFile();
			std::string filename;
			CSimpleIniA ini;
			bool reading = false;
			std::string currentGroup;
		};
	}
}
