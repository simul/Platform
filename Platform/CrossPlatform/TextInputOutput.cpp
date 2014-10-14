
#include "Simul/Platform/CrossPlatform/TextInputOutput.h"
#include "Simul/Base/RuntimeError.h"
#include "Simul/Base/StringFunctions.h"
#include "Simul/Base/StringToWString.h"
#include <stdio.h>
#include <stdarg.h>
#include <fstream>
#include <sstream>
using namespace simul;
using namespace crossplatform;
using std::string;

TextFileInput::TextFileInput(simul::base::MemoryInterface *m)
	:good(true)
	,fileLoader(NULL)
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
			::operator delete(array[j],memoryInterface);
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
		size_t brace_pos	=text.find("{",pos+1);
		size_t end_brace_pos=(size_t)findMatchingBrace(text,(int)brace_pos);
		if(brace_pos>=text.length())
			return;
		std::string sub=text.substr(brace_pos,end_brace_pos+1-brace_pos);
		TextFileInput *e=::new(m) TextFileInput;
		array.push_back(e);
		e->Load(sub);
		pos=end_brace_pos;
	}
}

void TextFileInput::Load(const std::string &text)
{
	// we expect to see { and } at the start and end.
	size_t open_pos=text.find("{");
	std::string line;
	size_t pos=open_pos;
	while(pos<text.length())
	{
		int colon_pos=(int)text.find(":",pos+1);
		if(colon_pos<0||colon_pos>=(int)text.length())
			break;
		std::string name=text.substr(pos+1,colon_pos-1-pos);
		name=StripOuterQuotes(name);
		int next_colon	=(int)text.find(":",colon_pos+1);
		int brace_pos	=(int)text.find("{",colon_pos+1);
		int sq_pos		=(int)text.find("[",colon_pos+1);
		int next_ret	=(int)text.find("\n",colon_pos+1);
		if(sq_pos>=0&&sq_pos<next_colon&&sq_pos<brace_pos)
		{
			size_t end_sq_pos=(size_t)findMatchingSq(text,brace_pos);
			std::string sub=text.substr(sq_pos,end_sq_pos+1-sq_pos);
			Array &array=arrays[name];
			LoadArray(array,sub,memoryInterface);
			pos=end_sq_pos;
		}
		else if(brace_pos>=0&&brace_pos<next_colon)
		{
			size_t end_brace_pos=(size_t)findMatchingBrace(text,brace_pos);
			std::string sub=text.substr(brace_pos,end_brace_pos+1-brace_pos);
			subElements[name].Load(sub);
			pos=end_brace_pos;
		}
		else if(next_ret>=0)
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
		void *pointer=NULL;
		unsigned int bytes=0;
		fileLoader->AcquireFileContents(pointer,bytes,filename_utf8,true);
		good=(pointer!=NULL);
		if(pointer)
			text=(const char *)pointer;
		fileLoader->ReleaseFileContents(pointer);
	}
	else
	{
	ERRNO_CHECK
	ERRNO_CHECK
		if(!fileLoader->FileExists(filename_utf8))
		{
			good=false;
	ERRNO_CHECK
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

double TextFileInput::Get(const char *name,double dflt)
{
	if(properties.find(name)==properties.end())
		return dflt;
	return atof(properties[name].c_str());
}

float TextFileInput::Get(const char *name,float dflt)
{
	if(properties.find(name)==properties.end())
		return dflt;
	return (float)atof(properties[name].c_str());
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
		val[i]=(float)atof(s.c_str());
		pos=comma_pos+1;
	}
	vec4 ret=val;
	return ret;
}

const char *TextFileInput::Get(int propertyIndex)
{
	std::map<std::string,std::string>::iterator i=properties.begin();
	if(propertyIndex>=properties.size())
		return NULL;
	for(int j=0;j<propertyIndex;j++,i++)
	{
	}
	if(i==properties.end())
		return NULL;
	return i->first.c_str();
}

TextInput *TextFileInput::GetSubElement(const char *name)
{
	return &subElements[name];
}

TextInput::Array &TextFileInput::GetArray(const char *name)
{
	return arrays[name];
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
			::operator delete(array[j],memoryInterface);
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
	Save(ofs);
}

std::string TextFileOutput::getText()
{
	std::stringstream ofs;
	Save(ofs);
	return ofs.str();
}

void TextFileOutput::Save(std::ostream &ofs,int tab)
{
	std::string tabstr0,tabstr1;
	for(int i=0;i<tab;i++)
		tabstr0+="\t";
	tabstr1=tabstr0+"\t";
	const char *t0=tabstr0.c_str();
	const char *t1=tabstr1.c_str();
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

void TextFileOutput::Set(const char *name,double value)
{
	properties[name]=base::stringFormat("%16.16g",value);
}

void TextFileOutput::Set(const char *name,float value)
{
	properties[name]=base::stringFormat("%16.16g",value);
}

void TextFileOutput::Set(const char *name,vec4 value)
{
	properties[name]=base::stringFormat("%16.16g,%16.16g,%16.16g,%16.16g",value.x,value.y,value.z,value.w);
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
			::operator delete(memoryInterface,a[i]);
	a.resize(size);
	for(int i=old_size;i<size;i++)
		a[i]=::new(memoryInterface) TextFileOutput;
	return a;
}
