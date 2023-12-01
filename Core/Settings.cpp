#include "Platform/Core/Settings.h"
#include "Platform/Core/FileLoader.h"

using namespace platform;
using namespace core;

Settings::Settings(const char *f)
{
	filename = f;
	LoadFile();
}

Settings::~Settings()
{
	SaveFile();
}

void Settings::LoadFile()
{
	reading = true;
	auto *fileLoader = platform::core::FileLoader::GetFileLoader();
	if (!fileLoader)
		return;
	std::string str = fileLoader->LoadAsString(filename.c_str());
	SI_Error rc = ini.LoadData(str);
}

void Settings::SaveFile()
{
	if (reading)
		return;
	auto *fileLoader = platform::core::FileLoader::GetFileLoader();

	std::string str;
	SI_Error err = ini.Save(str);
	fileLoader->Save(str.data(), (unsigned int)str.length(), filename.c_str(), true);
}

void Settings::BeginGroup(const char *name)
{
	currentGroup = name;
}

void Settings::EndGroup()
{
	currentGroup = "";
}