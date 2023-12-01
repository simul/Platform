#include "StringFunctions.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <vector>
#include <iostream>
#include <regex>

using namespace std;
#ifndef _MSC_VER
#define	sprintf_s(buffer, buffer_size, stringbuffer, ...) (snprintf(buffer, buffer_size, stringbuffer, ##__VA_ARGS__))
#endif

#define LB_TO_NEWTONS (4.448f)
#define FEET_TO_METRES (12.f*2.54f/100.f)
#define BHP_TO_WATTS (745.7f)
#define KMH_TO_MS (0.277777778f)
#define MPH_TO_MS (0.44704f)
using std::string;


size_t count_lines_in_string(const std::string &str)
{
	size_t n = 0;
	for( auto c:str)
	{
		if ( c == '\n' )
			n++;
	}
	return n+1;
}

size_t ttab;
string NextNum(const string &Line,size_t &idx)
{
	string str="";
	if(Line.length()<1)
		return str;
	if(idx>=Line.length())
		return str;
	char x=Line[idx];
	while((x<'0'||x>'9')&&(x!='.')&&(x!='-'))
	{
		if(idx>=Line.length())
			return str;
		idx++;
		x=Line[idx];
	}
	while((x>='0'&&x<='9')||(x=='.')||(x=='-')||(x=='+')||(x=='e'))
	{
		idx++;
		str+=x;
		if(idx>=(size_t)Line.length())
			x=0;
		else
			x=Line[idx];
	}
	return str;
}
bool IsWhitespace(char c)
{
	return (c==' '||c==13||c==10||c=='\t');
}
void ClipWhitespace(std::string &Line)
{
	while(Line.length()?IsWhitespace(Line[0]):false)
		Line=Line.substr(1,Line.length()-1);
	while(Line.length()?IsWhitespace(Line[Line.length()-1]):false)
		Line=Line.substr(0,Line.length()-1);
}
string ExtractNextName(std::string &Line)
{
	size_t pos=0;
	string name=NextName(Line,pos);
	Line=Line.substr(pos,Line.length()-pos);
	ClipWhitespace(Line);
	return name;
}
string NextName(const string &Line)
{
	size_t idx=0;
	return NextName(Line,idx);
}
//------------------------------------------------------------------------------
string NextName(const string &Line,size_t &idx)
{
	string str;
	str="";
	if(Line.length()<1)
		return str;
	if(idx>=Line.length())
		return str;
	char x=Line[idx];
	while((x<'a'||x>'z')&&(x<'A'||x>'Z')&&(x<'0'||x>'9')&&x!='_'&&x!='-'&&x!='.')
	{
		idx++;
		if(idx>=(size_t)Line.length())
			return "";
		else
			x=Line[idx];
	}
	while((x>='a'&&x<='z')||(x>='A'&&x<='Z')||(x>='0'&&x<='9')||x=='_'||x=='-'||x=='.')
	{
		str+=x;
		idx++;
		if(idx>=(size_t)Line.length())
			x=0;
		else
			x=Line[idx];
	}
	return str;
}
//------------------------------------------------------------------------------
string NextNum(const string &Line)
{
	size_t idx=0;
	string str="";
	if(Line.length()<1)
		return str;
	char x=Line[idx];
	while(x&&((x<'0'||x>'9')&&(x!='.')&&(x!='-')&&(x!='e')))
	{
		idx++;
		if(idx>=(size_t)Line.length())
			x=0;
		else
			x=Line[idx];
	}
	while((x>='0'&&x<='9')||(x=='.')||(x=='-')||(x=='e'))
	{
		idx++;
		str+=x;
		if(idx>=(size_t)Line.length())
			x=0;
		else
			x=Line[idx];
	}
	return str;
}
//------------------------------------------------------------------------------
int NextInt(const string &Line)
{
	string num=NextNum(Line);
	char *stopstring;
	int val=strtol(num.c_str(),&stopstring,10);
	return val;
}
//------------------------------------------------------------------------------
bool NextBool(const string &Line)
{
	if(Line.find("true")<Line.length())
		return true;
	if(Line.find("false")<Line.length())
		return false;
	string num=NextNum(Line);
	char *stopstring;
	size_t val=strtol(num.c_str(),&stopstring,10);
	return val!=0;
}
//------------------------------------------------------------------------------
int NextInt(const string &Line,size_t &idx)
{
	string num=NextNum(Line,idx);
	char *stopstring;
	int val=strtol(num.c_str(),&stopstring,10);
	return val;
}
//------------------------------------------------------------------------------
const float *NextVector2(const string &Line)
{
	static float v[3];
	size_t pos=0;
	string num1=NextNum(Line,pos);
	string num2=NextNum(Line,pos);
	char *stopstring;
	v[0]=(float)strtod(num1.c_str(),&stopstring);
	v[1]=(float)strtod(num2.c_str(),&stopstring);
	v[2]=0;
	return v;
}
//------------------------------------------------------------------------------
const float *NextVector3(const string &Line)
{
	static float v[4];
	size_t pos=0;
	string num1=NextNum(Line,pos);
	string num2=NextNum(Line,pos);
	string num3=NextNum(Line,pos);
	char *stopstring;
	v[0]=(float)strtod(num1.c_str(),&stopstring);
	v[1]=(float)strtod(num2.c_str(),&stopstring);
	v[2]=(float)strtod(num3.c_str(),&stopstring);
	v[3]=0;
	return v;
}
//------------------------------------------------------------------------------
float NextFloat(const string &Line)
{
	string num=NextNum(Line);
	char *stopstring;
	float val=(float)strtod(num.c_str(),&stopstring);
	return val;
}
//------------------------------------------------------------------------------
float NextFloat(const string &Line,size_t &idx)
{
	string num=NextNum(Line,idx);
	char *stopstring;
	float val=(float)strtod(num.c_str(),&stopstring);
	return val;
}
//------------------------------------------------------------------------------
bool NextSettingFromTag(string &tag,string &Name,string &Value)
{
	size_t equalpos=tag.find("=");
	if(equalpos>=tag.length())
		return false;
	size_t quotepos=tag.find("\"");
	size_t quotepos2=tag.find("\"",quotepos+1);
	if(quotepos>=tag.length())
		return false;
	if(quotepos2>=tag.length())
		return false;
	Name=tag.substr(0,equalpos);
	Value=tag.substr(quotepos+1,quotepos2-quotepos-1);
	tag=tag.substr(quotepos2+1,tag.length()-quotepos2-1);
	return true;
}
//------------------------------------------------------------------------------
void GetXMLSetting(string &setting,string &Value,const string &Name)
{
	size_t pos=setting.find(Name);
	if(pos>=setting.length())
	{
		Value="";
		return;
	}
	size_t equalpos=setting.find("=",pos);
	if(equalpos!=pos+Name.length())
	{
		Value="";
		return;
	}
	size_t openquote=setting.find("\"",equalpos);
	size_t closequote=setting.find("\"",openquote+1);
	Value=setting.substr(openquote+1,closequote-openquote-1);
}
//------------------------------------------------------------------------------
void StartXMLWrite(string &)
{
	ttab=0;
}
//------------------------------------------------------------------------------
bool FindXMLToken(const string &in,size_t &pos0)
{
	size_t openpos=in.find("<",pos0);
	if(openpos>=in.length())
		return false;
	size_t pos2=in.find(">",openpos+1);
	pos0=pos2+1;
	return true;
}
//------------------------------------------------------------------------------
bool FindXMLToken(const string &in,size_t &pos0,string &name,string &setting,string &contents)
{
	size_t openpos=in.find("<",pos0);
	if(openpos>=in.length())
		return false;
	size_t pos2=in.find(">",openpos+1);
	string tag=in.substr(openpos+1,pos2-openpos-1);
	size_t pos=0;
	name=NextName(tag,pos);
	//size_t equalpos=tag.find("=");
	if(pos<(size_t)tag.length()-1)
		setting=tag.substr(pos+1,tag.length()-pos-1);
	else
		setting="";
	if(name.length()==0)
		return false;
	char tt=tag[0];
	if(tt=='\\'||tt=='?'||tt=='!'||(tag[tag.length()-1]=='/'))
	{
		contents="";
		pos0=pos2+1;
		return true;
	}
	string closing=string("</")+name+string(">");
	size_t closepos=in.find(closing,pos0);
	if(closepos>=in.length())
		return false;
	contents=in.substr(pos2+1,closepos-pos2-1);
	pos0=closepos+closing.length();
	return true;
}
//------------------------------------------------------------------------------
void WriteXMLOpen(string &out,const string &name,const string &setting)
{
	for(size_t i=0;i<ttab;i++)
		out+="\t";
	out+="<"+name;
	if(setting.length())
		out+=" "+setting;
	out+=">\r\n";
	ttab++;
}
//------------------------------------------------------------------------------
void WriteXMLOpen(string &out,const string &name)
{
	WriteXMLOpen(out,name,"");
}
//------------------------------------------------------------------------------
void WriteXMLClose(string &out,const string &name)
{
	ttab--;
	for(size_t i=0;i<ttab;i++)
		out+="\t";
	out+="</"+name;
	out+=">\r\n";
}
void WriteXMLTokenWithSetting(string &out,const string &name,const string &setting)
{
	for(size_t i=0;i<ttab;i++)
		out+="\t";
	out+="<"+name+">";
	out+=setting;
	out+="</"+name+">";
	out+="\r\n";
}
void WriteXMLToken(string &out,const string &name)
{
	for(size_t i=0;i<ttab;i++)
		out+="\t";
	out+="<"+name;
	out+="/>\r\n";
}
void WriteFloat(string &out,float setting)
{
	char txt[100];
	sprintf_s(txt,99,"%0.6g",setting);
	out=txt;
}
void WriteInt(string &out,size_t setting)
{
	char txt[100];
	sprintf_s(txt,99,"%d",(int)setting);
	out=txt;
}

void Tab(string &out)
{
	for(size_t i=0;i<ttab;i++)
		out+="\t";
}
size_t GoToLine(const string &Str,size_t line)
{
	size_t pos=0;
	char txt[100];
	sprintf_s(txt,99,"\r\n");
	string CR;
	for(size_t i=0;i<line;i++)
	{
		pos=Str.find(13,pos);
		pos++;
	}
	//pos++;
	return pos;
}
// for streaming
using std::string;
string MakeXMLOpen(const string &name)
{
	string str="";
	for(size_t i=0;i<ttab;i++)
		str+="\t";
	str+="<"+name;
	str+=">\r";
	ttab++;
	return str;
}
string MakeXMLClose(const string &name)
{
	string str="";
	ttab--;
	for(size_t i=0;i<ttab;i++)
		str+="\t";
	str+="</"+name;
	str+=">\r";
	return str;
}
string MakeXMLToken(const string &name,const string &value)
{
	string str="";
	for(size_t i=0;i<ttab;i++)
		str+="\t";
	str+="<"+name+">";
	str+=value;
	str+="</"+name+">";
	str+="\r";
	return str;
}
string MakeXMLToken(const string &name,int value)
{
	string str="";
	WriteInt(str,value);
	return MakeXMLToken(name,str);
}
string MakeXMLToken(const string &name,float value)
{
	string str="";
	WriteFloat(str,value);
	return MakeXMLToken(name,str);
}
string stringof(int i)
{
	char txt[100];
	sprintf_s(txt,99,"%d",i);
	return string(txt);
}
string stringof(float f)
{
	char txt[100];
	sprintf_s(txt,99,"%g",f);
	return string(txt);
}
string ToString(int i)
{
	char txt[100];
	sprintf_s(txt,99,"%d",i);
	return string(txt);
}

string ToString(float i)
{
	char txt[100];
	sprintf_s(txt,99,"%g",i);
	return string(txt);
}

string ToString(bool i)
{
	char txt[100];
	sprintf_s(txt,99,i?"true":"false");
	return string(txt);
}

string GetFilenameOnly(const string &src)
{
	string str=src;
	int pos=(int)str.find("\\");
	int fs_pos=(int)str.find("/");
	if(pos<0||(fs_pos>=0&&fs_pos<pos))
		pos=fs_pos;
	do
	{
		str=str.substr(pos+1,str.length()-pos);
		pos=(int)str.find("\\");
		fs_pos=(int)str.find("/");
		if(pos<0||(fs_pos>=0&&fs_pos<pos))
			pos=fs_pos;
	} while(pos>=0);
	return str;
}

string GetDirectoryFromFilename(const string &str)
{
	int pos=(int)str.find_last_of("\\");
	int fs_pos=(int)str.find_last_of("/");
	if(pos<0||(fs_pos>=0&&fs_pos>pos))
		pos=fs_pos;
	if(pos<0)
		return "";
	return str.substr(0,pos);
}

vector<string> SplitPath(const string &fullPath)
{
	size_t slash_pos = fullPath.find_last_of("/");
	if (slash_pos >= fullPath.length())
		slash_pos = 0;
	size_t back_pos = fullPath.find_last_of("\\");
	if (back_pos<fullPath.length() && back_pos>slash_pos)
		slash_pos = back_pos;
	if (slash_pos >= fullPath.length())
		slash_pos = 0;
	string dirOnly = fullPath.substr(0,slash_pos);
	string filenameOnly = fullPath.substr(slash_pos, fullPath.length() - slash_pos);
	vector<string> v;
	v.push_back(dirOnly);
	v.push_back(filenameOnly);
	return v;
}

#include <sstream>
#include <iterator>

template<typename Out>
void split(const std::string &s, char delim, Out result) {
	std::stringstream ss;
	ss.str(s);
	std::string item;
	while (std::getline(ss, item, delim)) {
		*(result++) = item;
	}
}


std::vector<std::string> split(const std::string &s, char delim) {
	std::vector<std::string> elems;
	split(s, delim, std::back_inserter(elems));
	return elems;
}

std::string stringFormat(std::string fmt, ...)
{
	int size=(int)fmt.size()+100;
	std::string str;
	va_list ap;
	int n=-1;
	const char *format_str=fmt.c_str();
	while(n<0)
	{
		str.resize(size);
		va_start(ap, fmt);
		//n = vsnprintf_s((char *)str.c_str(), size, size,format_str, ap);
		n = vsnprintf((char *)str.c_str(), size,format_str, ap);
		va_end(ap);
		if(n> -1 && n <size)
		{
			str.resize(n);
			return str;
		}
		if (n > -1)
			size=n+1;
		else
			size*=2;
		n=-1;
	}
	return str;
}
const char *QuickFormat(const char *format_str,...)
{
	int size=(int)strlen(format_str)+100;
	static std::string str;
	va_list ap;
	int n=-1;
	while(n<0)
	{
		str.resize(size);
		va_start(ap, format_str);
		//n = vsnprintf_s((char *)str.c_str(), size, size,format_str, ap);
		n = vsnprintf((char *)str.c_str(), size,format_str, ap);
		va_end(ap);
		if(n> -1 && n <=size)
		{
			str.resize(n);
			return str.c_str();
		}
		if (n > -1)
			size=n+1;
		else
			size*=2;
		n=-1;
	}
	return str.c_str();
}
void find_and_replace(std::string& source, std::string const& find, std::string const& replace)
{
	for(std::string::size_type i = 0; (i = source.find(find, i)) != std::string::npos;)
	{
		source.replace(i, find.length(), replace);
		i += replace.length() - find.length() + 1;
	}
}
void find_and_replace_identifier(std::string& source, std::string const& find, std::string const& replace)
{
	std::regex param_re(QuickFormat("\\b(%s)\\b", find.c_str()));
	source = std::regex_replace(source, param_re, replace);
}

string ToString(sfx::FillMode x)
{
	switch(x)
	{
	case sfx::FILL_WIREFRAME:
		return "FILL_WIREFRAME";
	case sfx::FILL_SOLID:
		return "FILL_SOLID";
	case sfx::FILL_POINT:
		return "FILL_POINT";
	default:
		std::cerr << "Unknown Fill Mode: " << x << std::endl;
		return "UNKNOWN";
	}
}
string ToString(sfx::CullMode x)
{
	switch(x)
	{
	case sfx::CULL_NONE:
		return "CULL_NONE";
	case sfx::CULL_FRONT:
		return "CULL_FRONT";
	case sfx::CULL_BACK:
		return "CULL_BACK";
	default:
		std::cerr << "Unknown Cull Mode: " << x << std::endl;
		return "UNKNOWN";
	}
}
string ToString(sfx::FilterMode x)
{
	switch(x)
	{
	case sfx::MIN_MAG_MIP_LINEAR:
		return "LINEAR";
	case sfx::MIN_MAG_MIP_POINT:
		return "POINT";
	case sfx::ANISOTROPIC:
		return "ANISOTROPIC";
	default:
		std::cerr << "Unknown Filter Mode: " << x << std::endl;
		return "UNKNOWN";
	}
}

string ToString(sfx::AddressMode x)
{
	switch(x)
	{
	case sfx::CLAMP:
		return "CLAMP";
	case sfx::WRAP:
		return "WRAP";
	case sfx::MIRROR:
		return "MIRROR";
	default:
		std::cerr << "Unknown Address Mode: " << x << std::endl;
		return "UNKNOWN";
	}
}
#include "StringToWString.h"
std::string GetEnv(const std::string &name_utf8)
{
#if defined(WIN32)|| defined(WIN64)
	std::wstring wname=Utf8ToWString(name_utf8);
	size_t sizeNeeded;
	wchar_t buffer[1];
	errno_t err=_wgetenv_s(&sizeNeeded,
				buffer,
				1,
				wname.c_str());
	std::wstring wret=L"";
	if(err!=0&&sizeNeeded>0)
	{
		wchar_t *txt=new wchar_t[sizeNeeded];
		err=_wgetenv_s(&sizeNeeded,
				txt,
				sizeNeeded,
				wname.c_str()); 
		if(err!=EINVAL)
			wret=(txt);
		delete [] txt;
	}
	std::string ret=WStringToUtf8(wret);
	return ret;
#else
	return std::string("");
#endif
}

string join(const std::set<string> &replacements,string sep)
{
	string str;
	bool first=true;
	for(auto i:replacements)
	{
		if(first)
		{
			first=false;
		}
		else
		{
			str+=sep;
		}
		str+=i;
	}
	return str;
}

string join_vector(const std::vector<string> &replacements,string sep)
{
	string str;
	bool first=true;
	for(auto i:replacements)
	{
		if(first)
		{
			first=false;
		}
		else
		{
			str+=sep;
		}
		str+=i;
	}
	return str;
}