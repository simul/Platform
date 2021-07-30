
#include "Platform/CrossPlatform/TextInputOutput.h"
#include "Platform/Core/RuntimeError.h"
#include "Platform/Core/StringFunctions.h"
#include "Platform/Core/StringToWString.h"
#include <stdio.h>
#include <stdarg.h>
#include <fstream>
#include <sstream>
#include <algorithm>
#if PLATFORM_STD_CHARCONV
#include <charconv>
#endif
using namespace simul;
using namespace crossplatform;
using std::string;
#ifndef _MSC_VER
#include <stdio.h>
#include <strings.h>
#include <string.h> // for memset
#define _stricmp strcasecmp
#else
#include <windows.h>
#endif

TextFileInput::TextFileInput(simul::base::MemoryInterface *m)
	:good(true)
	,fileLoader(nullptr)
	,memoryInterface(m)
{
	fileLoader=base::FileLoader::GetFileLoader();
}

TextFileInput::~TextFileInput()
{
	for(std::map<std::string,Array>::iterator i=arrays.begin();i!=arrays.end();i++)
	{
		Array &array=i->second;
		for(int j=0;j<(int)array.size();j++)
		{
			del(array[j],memoryInterface);
		}
	}
}

static string StripOuterWhitespace(string str)
{
	string ret=str;
	char c=0;
	while(ret.size()>0&&c==0)
	{
		c=ret.c_str()[0];
		if(!c)		//no ascii for this character!
			break;
		if(c==' '||c=='\t'||c=='\r'||c=='\n')
		{
			ret=ret.substr(1,ret.length()-1);
			c=0;
		}
	}
	c=0;
	while(ret.size()>0&&c==0)
	{
		c=ret.c_str()[ret.length()-1];
		if(!c)		//no ascii for this character!
			break;
		if(c==' '||c=='\t'||c=='\r'||c=='\n')
		{
			ret=ret.substr(0,ret.length()-1);
			c=0;
		}
	}
	return ret;
}

static string StripOuterQuotes(const string &str)
{
	string ret=StripOuterWhitespace(str);
	if(ret.size()==0)
		return str;
	if(ret[0]=='\"')
	{
		if(ret.length()>1)
			ret=ret.substr(1,ret.length()-1);
		else
			ret.clear();
		if(ret.length())
			if(ret[ret.length()-1]=='\"')
				ret=ret.substr(0,ret.length()-1);
	}
	if(ret[0]=='\'')
	{
		if(ret.length()>1)
			ret=ret.substr(1,ret.length()-1);
		else
			ret.clear();
		if(ret.length())
			if(ret[ret.length()-1]=='\'')
				ret=ret.substr(0,ret.length()-1);
	}
	return ret;
}

static int findMatching(const std::string &text,int open_brace_pos,const char *open,const char *close)
{
	int brace=1;
	size_t pos=open_brace_pos;
	while(pos<text.length())
	{
		size_t next_open=text.find(open,pos+1);
		size_t next_close=text.find(close,pos+1);
		if(next_open<next_close)
		{
			brace++;
			pos=next_open;
		}
		else if(next_close<next_open)
		{
			brace--;
			pos=next_close;
			if(!brace)
				return (int)pos;
		}
		else
			pos=text.length();
	}
	return (int)pos;
}

static int findMatchingSq(const std::string &text,int open_brace_pos)
{
	return findMatching(text,open_brace_pos,"[","]");
}

static int findMatchingBrace(const std::string &text,int open_brace_pos)
{
	return findMatching(text,open_brace_pos,"{","}");
}

static void LoadArray(TextInput::Array &array,const string &text,simul::base::MemoryInterface *m)
{
	size_t pos=0;
	while(pos<text.length())
	{
		size_t start_pos	=pos;
		size_t brace_pos	=text.find("{",pos);
		size_t colon_pos	=text.find(":",pos);
		size_t end_pos		=text.find("\n",colon_pos);
		if(brace_pos<text.length())
		{	
			start_pos=brace_pos;
			 end_pos=(size_t)findMatchingBrace(text,(int)brace_pos);
		}
		if(brace_pos>=text.length()&&colon_pos>=text.length())
			break;
		std::string sub=text.substr(start_pos,end_pos+1-start_pos);
		TextFileInput *e=::new(m) TextFileInput;
		array.push_back(e);
		e->Load(sub);
		pos=end_pos;
	}
}

void TextFileInput::Load(const std::string &text)
{
	// if there are multiple elements we expect to see { and } at the start and end.
	size_t open_pos=text.find("{");
	std::string line;
	//if(open_pos>=text.length())
		open_pos=0;
	size_t pos=open_pos;
	while(pos<text.length())
	{
		size_t colon_pos=text.find(":",pos+1);
		if(colon_pos>=text.length())
			break;
		std::string name=text.substr(pos+1,colon_pos-1-pos);
		name=StripOuterQuotes(name);
		size_t next_colon	=text.find(":",colon_pos+1);
		size_t brace_pos	=text.find("{",colon_pos+1);
		size_t sq_pos		=text.find("[",colon_pos+1);
		size_t next_ret		=text.find("\n",colon_pos+1);
		if(sq_pos<next_colon&&sq_pos<brace_pos)
		{
			size_t end_sq_pos=(size_t)findMatchingSq(text,(int)std::min(sq_pos,brace_pos));
			std::string sub=text.substr(sq_pos,end_sq_pos+1-sq_pos);
			Array &array=arrays[name];
			LoadArray(array,sub,memoryInterface);
			pos=end_sq_pos;
		}
		else if(brace_pos>=colon_pos&&brace_pos<next_colon)
		{
			size_t end_brace_pos=(size_t)findMatchingBrace(text,(int)brace_pos);
			std::string sub=text.substr(brace_pos,end_brace_pos+1-brace_pos);
			subElements[name].Load(sub);
			pos=end_brace_pos;
		}
		else if(next_ret<text.length())
		{
			std::string value=text.substr(colon_pos+1,next_ret-1-colon_pos-1);;
			value=StripOuterQuotes(value);
			properties[name]=value;
			pos=next_ret;
		}
		else
			break;
	}
}

void TextFileInput::SetFileLoader(simul::base::FileLoader *f)
{
	fileLoader=f;
}

void TextFileInput::Load(const char *filename_utf8)
{
	if(!filename_utf8)
		return;
	std::string text="";
	if(fileLoader)
	{
		void *pointer=nullptr;
		unsigned int bytes=0;
		fileLoader->AcquireFileContents(pointer,bytes,filename_utf8,true);
		good=(pointer!=nullptr);
		if(pointer)
			text=(const char *)pointer;
		fileLoader->ReleaseFileContents(pointer);
	}
	else
	{
	
	
		if(!fileLoader->FileExists(filename_utf8))
		{
			good=false;
	
			return;
		}
#ifdef _MSC_VER
		std::ifstream ifs(simul::base::Utf8ToWString(filename_utf8).c_str());
#else
		std::ifstream ifs(filename_utf8);
#endif
	ERRNO_CHECK
		std::string line;
		while(getline(ifs,line))
		{
			text+=line;
			text+="\r\n";
		}
	}
	Load(text);
	good=true;
}

bool TextFileInput::Good()
{
	return good;
}

bool TextFileInput::Has(const char *name) const
{
	if(properties.find(name)==properties.end())
		return false;
	return true;
}

const char *TextFileInput::Get(const char *name,const char *dflt)
{
	if(properties.find(name)==properties.end())
		return dflt;
	return properties[name].c_str();
}

bool TextFileInput::Get(const char *name,bool dflt)
{
	if(properties.find(name)==properties.end())
		return dflt;
	return(strcmp(properties[name].c_str(),"true")==0);
}

int TextFileInput::Get(const char *name,int dflt)
{
	if(properties.find(name)==properties.end())
		return dflt;
	if(_stricmp(properties[name].c_str(),"true")==0)
		return 1;
	if(_stricmp(properties[name].c_str(),"false")==0)
		return 0;
	return atoi(properties[name].c_str());
}

long long TextFileInput::Get(const char* name, long long dflt)
{
	if (properties.find(name) == properties.end())
		return dflt;
	if (_stricmp(properties[name].c_str(), "true") == 0)
		return (long long)1;
	if (_stricmp(properties[name].c_str(), "false") == 0)
		return (long long)0;
	return atoll(properties[name].c_str());
}

unsigned long long TextFileInput::Get(const char* name, unsigned long long dflt)
{
	if (properties.find(name) == properties.end())
		return dflt;
	if (_stricmp(properties[name].c_str(), "true") == 0)
		return (unsigned long long)1;
	if (_stricmp(properties[name].c_str(), "false") == 0)
		return (unsigned long long)0;
	return strtoull(properties[name].c_str(),nullptr,0);
}

double TextFileInput::Get(const char *name,double dflt)
{
	if(properties.find(name)==properties.end())
		return dflt;

	string propNameStr = properties[name];
#if PLATFORM_STD_CHARCONV
	propNameStr.erase(std::remove_if(propNameStr.begin(), propNameStr.end(), [](unsigned char x) {return std::isspace(x); }), propNameStr.end());
	const char* propName = propNameStr.c_str();
	double value;
	std::from_chars_result res = std::from_chars(propName, propName + strlen(propName), value);
	if (res.ec != std::errc())
		return dflt;
	else
		return value;
#else
	return atof(propNameStr.c_str());
#endif 
}

float TextFileInput::Get(const char *name,float dflt)
{
	if(properties.find(name)==properties.end())
		return dflt;

	string propNameStr = properties[name];
#if PLATFORM_STD_CHARCONV
	propNameStr.erase(std::remove_if(propNameStr.begin(), propNameStr.end(), [](unsigned char x) {return std::isspace(x); }), propNameStr.end());
	const char* propName = propNameStr.c_str();
	float value;
	std::from_chars_result res = std::from_chars(propName, propName + strlen(propName), value);
	if (res.ec != std::errc())
		return dflt;
	else
		return value;
	#else
	return (float)atof(propNameStr.c_str());
#endif 
}

int3 TextFileInput::Get(const char *name,int3 dflt)
{
	if(properties.find(name)==properties.end())
		return dflt;
	int val[3];
	size_t pos=0;
	std::string str=properties[name];
	for(int i=0;i<3;i++)
	{
		size_t comma_pos=str.find(",",pos+1);
		string s=str.substr(pos,comma_pos-pos);
		val[i]=(int)atoi(s.c_str());
		pos=comma_pos+1;
	}
	int3 ret=val;
	return ret;
}

vec2 TextFileInput::Get(const char *name,vec2 dflt)
{
	if(properties.find(name)==properties.end())
		return dflt;
	float val[2];
	size_t pos=0;
	std::string str=properties[name];
	for(int i=0;i<2;i++)
	{
		size_t comma_pos=str.find(",",pos+1);
		string s=str.substr(pos,comma_pos-pos);
	#if PLATFORM_STD_CHARCONV
		s.erase(std::remove_if(s.begin(), s.end(), [](unsigned char x) {return std::isspace(x); }), s.end());
		const char* s_cstr = s.c_str();
		float value;
		std::from_chars_result res = std::from_chars(s_cstr, s_cstr + strlen(s_cstr), value);
		if (res.ec != std::errc())
			val[i] = 0.0f;
		else
			val[i] = value;
	#else
		val[i]=(float)atof(s.c_str());
	#endif
		pos=comma_pos+1;
	}
	vec2 ret=(const float *)val;
	return ret;
}

vec3 TextFileInput::Get(const char *name,vec3 dflt)
{
	if(properties.find(name)==properties.end())
		return dflt;
	float val[3];
	size_t pos=0;
	std::string str=properties[name];
	for(int i=0;i<3;i++)
	{
		size_t comma_pos=str.find(",",pos+1);
		string s=str.substr(pos,comma_pos-pos);
	#if PLATFORM_STD_CHARCONV
		s.erase(std::remove_if(s.begin(), s.end(), [](unsigned char x) {return std::isspace(x); }), s.end());
		const char* s_cstr = s.c_str();
		float value;
		std::from_chars_result res = std::from_chars(s_cstr, s_cstr + strlen(s_cstr), value);
		if (res.ec != std::errc())
			val[i] = 0.0f;
		else
			val[i] = value;
	#else
		val[i]=(float)atof(s.c_str());
	#endif
		pos=comma_pos+1;
	}
	vec3 ret=val;
	return ret;
}

vec4 TextFileInput::Get(const char *name,vec4 dflt)
{
	if(properties.find(name)==properties.end())
		return dflt;
	float val[4];
	size_t pos=0;
	std::string str=properties[name];
	for(int i=0;i<4;i++)
	{
		size_t comma_pos=str.find(",",pos+1);
		string s=str.substr(pos,comma_pos-pos);
	#if PLATFORM_STD_CHARCONV
		s.erase(std::remove_if(s.begin(), s.end(), [](unsigned char x) {return std::isspace(x); }), s.end());
		const char* s_cstr = s.c_str();
		float value;
		std::from_chars_result res = std::from_chars(s_cstr, s_cstr + strlen(s_cstr), value);
		if (res.ec != std::errc())
			val[i] = 0.0f;
		else
			val[i] = value;
	#else
		val[i]=(float)atof(s.c_str());
	#endif
		pos=comma_pos+1;
	}
	vec4 ret=val;
	return ret;
}

Quaterniond TextFileInput::Get(const char *name,Quaterniond dflt)
{
	if(properties.find(name)==properties.end())
		return dflt;
	double val[4];
	size_t pos=0;
	std::string str=properties[name];
	for(int i=0;i<4;i++)
	{
		size_t comma_pos=str.find(",",pos+1);
		string s=str.substr(pos,comma_pos-pos);
	#if PLATFORM_STD_CHARCONV
		s.erase(std::remove_if(s.begin(), s.end(), [](unsigned char x) {return std::isspace(x); }), s.end());
		const char* s_cstr = s.c_str();
		double value;
		std::from_chars_result res = std::from_chars(s_cstr, s_cstr + strlen(s_cstr), value);
		if (res.ec != std::errc())
			val[i] = 0.0;
		else
			val[i] = value;
	#else
		val[i]=atof(s.c_str());
	#endif
		pos=comma_pos+1;
	}
	Quaterniond ret=val;
	return ret;
}


const char *TextFileInput::Get(int propertyIndex)
{
	std::map<std::string,std::string>::iterator i=properties.begin();
	if(propertyIndex>=properties.size())
		return nullptr;
	for(int j=0;j<propertyIndex;j++,i++)
	{
	}
	if(i==properties.end())
		return nullptr;
	return i->first.c_str();
}

TextInput *TextFileInput::GetSubElement(const char *name)
{
	std::map<std::string,TextFileInput>::iterator u=subElements.find(name);
	if(u!=subElements.end())
		return &(u->second);
	return nullptr;
}


const char *TextFileInput::GetSubElement(int index)
{
	auto u=subElements.begin();
	for(int i=0;i<index;i++)
	{
		u++;
	}
	if(u==subElements.end())
		return nullptr;
	return u->first.c_str();
}

TextInput::Array &TextFileInput::GetArray(const char *name)
{
	return arrays[name];
}

const TextInput::Array &TextFileInput::GetArray(const char *name) const
{
	auto j=arrays.find(name);
	return (j)->second;
}

bool TextFileInput::HasArray(const char *name) const
{
	return (arrays.find(name)!=arrays.end());
}

TextFileOutput::TextFileOutput(simul::base::MemoryInterface *m)
	:memoryInterface(m)
{
}

TextFileOutput::~TextFileOutput()
{
	for(std::map<std::string,Array>::iterator i=arrays.begin();i!=arrays.end();i++)
	{
		Array &array=i->second;
		for(size_t j=0;j<array.size();j++)
		{
			del(array[j],memoryInterface);
		}
	}
}
static void write(std::ostream &ofs,const char *txt)
{
	ofs.write(txt,strlen(txt));
}
static void write(std::ostream &ofs,std::string str)
{
	write(ofs,str.c_str());
}

void TextFileOutput::Save(const char *filename_utf8)
{
	if(!filename_utf8)
		return;
#ifdef _MSC_VER
	std::ofstream ofs(simul::base::Utf8ToWString(filename_utf8).c_str());
#else
	std::ofstream ofs(filename_utf8);
#endif
	if(!ofs.good())
		SIMUL_THROW((std::string("Can't open file for saving: ")+filename_utf8).c_str());
	Save(ofs,0,true);
}

std::string TextFileOutput::getText()
{
	std::stringstream ofs;
	Save(ofs);
	return ofs.str();
}

void TextFileOutput::Save(std::ostream &ofs,int tab,bool bookEnd)
{
	std::string tabstr0,tabstr1;
	for(int i=0;i<tab;i++)
		tabstr0+="\t";
	tabstr1=tabstr0+"\t";
	const char *t0=tabstr0.c_str();
	const char *t1=tabstr1.c_str();
	if(properties.size()>1||arrays.size()>0||bookEnd)
		write(ofs,base::stringFormat("%s{\n",t0));
	for(std::map<std::string,std::string>::iterator i=properties.begin();i!=properties.end();i++)
	{
		std::string &str=i->second;
		write(ofs,base::stringFormat("%s\"%s\": \"%s\"\n",t1,i->first.c_str(),str.c_str()));
	}
	for(std::map<std::string,TextFileOutput>::iterator i=subElements.begin();i!=subElements.end();i++)
	{
		write(ofs,base::stringFormat("%s\"%s\":\n",t1,i->first.c_str()));
		TextFileOutput &s=i->second;
		s.Save(ofs,tab+1);
	}
	for(std::map<std::string,Array>::iterator i=arrays.begin();i!=arrays.end();i++)
	{
		write(ofs,base::stringFormat("%s\"%s\":\n%s[\n",t1,i->first.c_str(),t1));
		Array &array=i->second;
		for(size_t j=0;j<array.size();j++)
		{
			((TextFileOutput*)array[j])->Save(ofs,tab+1);
		}
		write(ofs,base::stringFormat("%s]\n",t1));
	}
	if(properties.size()>1||arrays.size()>0||bookEnd)
		write(ofs,base::stringFormat("%s}\n",t0));
}

bool TextFileOutput::Good()
{
	return true;
}

void TextFileOutput::Set(const char *name,const char *value)
{
	properties[name]=base::stringFormat("%s",value);
}

void TextFileOutput::Set(const char *name,bool value)
{
	properties[name]=base::stringFormat("%s",value?"true":"false");
}

void TextFileOutput::Set(const char *name,int value)
{
	properties[name]=base::stringFormat("%d",value);
}

void TextFileOutput::Set(const char* name, long long value)
{
	properties[name] = base::stringFormat("%lld", value);
}

void TextFileOutput::Set(const char* name, unsigned long long value)
{
	properties[name] = base::stringFormat("%llu", value);
}

void TextFileOutput::Set(const char *name,double value)
{
	properties[name]=base::stringFormat("%16.16g",value);
}

void TextFileOutput::Set(const char *name,float value)
{
	properties[name]=base::stringFormat("%16.16g",value);
}

void TextFileOutput::Set(const char *name,int3 value)
{
	properties[name]=base::stringFormat("%d,%d,%d",value.x,value.y,value.z);
}

void TextFileOutput::Set(const char *name,vec2 value)
{
	properties[name]=base::stringFormat("%16.16g,%16.16g",value.x,value.y);
}

void TextFileOutput::Set(const char *name,vec3 value)
{
	properties[name]=base::stringFormat("%16.16g,%16.16g,%16.16g",value.x,value.y,value.z);
}

void TextFileOutput::Set(const char *name,vec4 value)
{
	properties[name]=base::stringFormat("%16.16g,%16.16g,%16.16g,%16.16g",value.x,value.y,value.z,value.w);
}

void TextFileOutput::Set(const char *name,Quaterniond value)
{
	properties[name]=base::stringFormat("%16.16g,%16.16g,%16.16g,%16.16g",value.x,value.y,value.z,value.s);
}


TextOutput *TextFileOutput::CreateSubElement(const char *name)
{
	TextFileOutput &e=subElements[std::string(name)];
	return &e;
}

TextOutput::Array &TextFileOutput::GetArray(const char *name)
{
	TextOutput::Array &a=arrays[name];
	return a;
}

TextOutput::Array &TextFileOutput::CreateArray(const char *name,int size)
{
	TextOutput::Array &a=arrays[name];
	int old_size=(int)a.size();
	if(size<old_size)
		for(int i=size;i<old_size;i++)
			del(a[i], memoryInterface);
	a.resize(size);
	for(int i=old_size;i<size;i++)
		a[i]=::new(memoryInterface) TextFileOutput;
	return a;
}
