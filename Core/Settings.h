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

			bool LoadFile();
			bool SaveFile();

		protected:
			bool LoadFileInternal();
			bool SaveFileInternal();

		public:
			void BeginGroup(const char *name);
			void EndGroup();

			const std::string &GetFilename() const { return m_Filename; }
			const std::string &GetFilename() { return m_Filename; }

			template <typename T>
			void SetValue(const char *name, T value)
			{
				if (typeid(T) == typeid(double) || typeid(T) == typeid(float))
				{
					m_Ini.SetDoubleValue(m_CurrentGroup.c_str(), name, (double)value, nullptr, true);
				}
				else if (typeid(T) == typeid(bool))
				{
					m_Ini.SetBoolValue(m_CurrentGroup.c_str(), name, value, nullptr, true);
				}
				else if (typeid(T) == typeid(int8_t) || typeid(T) == typeid(int16_t) || typeid(T) == typeid(int32_t) || typeid(T) == typeid(int64_t) 
					|| typeid(T) == typeid(uint8_t) || typeid(T) == typeid(uint16_t) || typeid(T) == typeid(uint32_t) || typeid(T) == typeid(uint64_t))
				{
					m_Ini.SetLongValue(m_CurrentGroup.c_str(), name, static_cast<long>(value), nullptr, false, true);
				}
				else
				{
					SIMUL_BREAK("Unknown Type passed as template argument");
				}
			}
			void SetValue(const char *name, const char *value)
			{
				m_Ini.SetValue(m_CurrentGroup.c_str(), name, value, nullptr, true);
			}

			template <typename T>
			T GetValue(const char *name, T defaultValue)
			{
				if (typeid(T) == typeid(double) || typeid(T) == typeid(float))
				{
					return (T)m_Ini.GetDoubleValue(m_CurrentGroup.c_str(), name, (double)defaultValue);
				}
				else if (typeid(T) == typeid(bool))
				{
					return (T)m_Ini.GetBoolValue(m_CurrentGroup.c_str(), name, defaultValue);
				}
				else if (typeid(T) == typeid(int8_t) || typeid(T) == typeid(int16_t) || typeid(T) == typeid(int32_t) || typeid(T) == typeid(int64_t) 
					|| typeid(T) == typeid(uint8_t) || typeid(T) == typeid(uint16_t) || typeid(T) == typeid(uint32_t) || typeid(T) == typeid(uint64_t))
				{
					return (T)m_Ini.GetLongValue(m_CurrentGroup.c_str(), name, static_cast<long>(defaultValue));
				}
				else
				{
					SIMUL_BREAK("Unknown Type passed as template argument");
					return T();
				}
			}
			const char *GetValue(const char *name, const char *defaultValue)
			{
				return m_Ini.GetValue(m_CurrentGroup.c_str(), name, defaultValue);
			}

		protected:
			std::string m_Filename;
			CSimpleIniA m_Ini;
			std::string m_CurrentGroup;
		};
	}
}
