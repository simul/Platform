#include "Platform/Core/Settings.h"
#include "Platform/Core/FileLoader.h"

using namespace platform;
using namespace core;

Settings::Settings(const char *filename)
	: m_Filename(filename)
{
	LoadFile();
}

Settings::~Settings()
{
	SaveFile();
}

bool Settings::LoadFile()
{
	bool success = LoadFileInternal();
	if (!success)
	{
		PLATFORM_ERROR("Failed to load from {}.", m_Filename);
	}
	return success;
}

bool Settings::SaveFile()
{
	bool success = SaveFileInternal();
	if (!success)
	{
		PLATFORM_ERROR("Failed to save to {}.", m_Filename);
	}
	return success;
}

bool Settings::LoadFileInternal()
{
	FileLoader *fileLoader = FileLoader::GetFileLoader(); //Should not fail as there's a DefaultFileLoader - AJR.
	if (!fileLoader)
		return false;

	std::string str = fileLoader->LoadAsString(m_Filename.c_str());
	if (str.empty())
	{
		PLATFORM_ERROR("FileLoader failed to load from {}.", m_Filename);
		return false;
	}

	SI_Error siError = m_Ini.LoadData(str);
	if (siError < 0)
	{
		PLATFORM_ERROR("CSimpleIniA failed to load data from {} with error code {}.", m_Filename, (int)siError);
		return false;
	}

	return true;
}

bool Settings::SaveFileInternal()
{
	FileLoader *fileLoader = FileLoader::GetFileLoader(); //Should not fail as there's a DefaultFileLoader - AJR.
	if (!fileLoader)
		return false;

	std::string str;
	SI_Error siError = m_Ini.Save(str);
	if (siError < 0)
	{
		PLATFORM_ERROR("CSimpleIniA failed to save data to {} with error code {}.", m_Filename, (int)siError);
		return false;
	}

	bool saved = fileLoader->Save(str.data(), (unsigned int)str.length(), m_Filename.c_str(), true);
	if (!saved)
	{
		PLATFORM_ERROR("FileLoader failed to save to {}.", m_Filename);
		return false;
	}

	return true;
}

void Settings::BeginGroup(const char *name)
{
	m_CurrentGroup = name;
}

void Settings::EndGroup()
{
	m_CurrentGroup = "";
}