#include "Platform/Core/Settings.h"
#include "Platform/Core/FileLoader.h"

using namespace platform;
using namespace core;
using std::string;
CSimpleIniA ini;

Settings::Settings(const char *f)
{
	filename=f;
}

Settings::~Settings()
{
	if(reading)
		return;
	auto *fileLoader=platform::core::FileLoader::GetFileLoader();
	
	std::string str;
	SI_Error err=ini.Save(str);
	fileLoader->Save(str.data(),(unsigned int)str.length(),filename.c_str(),true);
}

void Settings::LoadFile() const
{
	reading=true;
	auto *fileLoader=platform::core::FileLoader::GetFileLoader();
	if(!fileLoader)
		return;
	string str=fileLoader->LoadAsString(filename.c_str());
	SI_Error rc = ini.LoadData(str);
}


void Settings::beginGroup(const char *name) const
{
	currentGroup=name;
}

void Settings::setValue(const char *name,int i)
{
	reading=false;
	ini.SetLongValue(currentGroup.c_str(), name,i);
}

int Settings::getValue(const char *name,int default_i) const
{
	if(!reading)
	{
		LoadFile();
	}
	return ini.GetLongValue(currentGroup.c_str(), name,default_i);
}

void Settings::setValue(const char *name,double d)
{
	reading=false;
	ini.SetDoubleValue(currentGroup.c_str(), name,d);
}

double Settings::getValue(const char *name,double default_d) const
{
	if(!reading)
	{
		LoadFile();
	}
	return ini.GetDoubleValue(currentGroup.c_str(), name,default_d);
}

void Settings::setValue(const char *name,float d)
{
	reading=false;
	ini.SetDoubleValue(currentGroup.c_str(), name,(double)d);
}

float Settings::getValue(const char *name,float default_d) const
{
	if(!reading)
	{
		LoadFile();
	}
	return (float)ini.GetDoubleValue(currentGroup.c_str(), name,(double)default_d);
}

void Settings::setValue(const char *name,const char *txt)
{
	reading=false;
	ini.SetValue(currentGroup.c_str(), name,txt);
}

const char *Settings::getValue(const char *name,const char *default_txt) const
{
	if(!reading)
	{
		LoadFile();
	}
	return ini.GetValue(currentGroup.c_str(), name, default_txt);
}


void Settings::endGroup() const
{
	currentGroup="";
}
