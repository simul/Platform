#include "Platform/Core/StringFunctions.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <algorithm>
#ifdef UNIX
#include <string.h> // for strlen
#endif
#ifndef _MSC_VER
#define	sprintf_s(buffer, buffer_size, stringbuffer, ...) (snprintf(buffer, buffer_size, stringbuffer, ##__VA_ARGS__))
#endif

using namespace simul::base;
namespace simul
{
	namespace base
	{
			#define LB_TO_NEWTONS (4.448f)
			#define FEET_TO_METRES (12.f*2.54f/100.f)
			#define BHP_TO_WATTS (745.7f)
			#define KMH_TO_MS (0.277777778f)
			#define MPH_TO_MS (0.44704f)
			using std::string;
			using std::vector;
            static size_t ttab;
			//------------------------------------------------------------------------------
			std::string ExtractDelimited(std::string &str,char delim)
			{
				string first;
				GetDelimited(str,first,str,delim);
				return first;
			}
			std::string ExtractDelimited(std::string &str,const std::string &delim)
			{
				string first;
				GetDelimited(str,first,str,delim);
				return first;
			}
			std::string ExtractDelimited(std::string &str,char delim,bool match_brackets)
			{
				return ExtractDelimited(str,string(&delim),match_brackets);
			}
			std::string ExtractDelimited(std::string &str,const std::string &delim,bool match_brackets)
			{
				string First;
				if(!str.length())
				{
					First="";
					str="";
					return First;
				}
			//	assert(delim!="<"&&delim!=">");
				size_t pos=0;
				int num_brackets=0;
				while(pos<str.length())
				{
					size_t pos_delim=str.find(delim,pos);
					size_t pos_open=str.find('<',pos);
					size_t pos_close=str.find('>',pos);
					if(num_brackets==0||!match_brackets)
						if(pos_delim<pos_open)
						{
							First=str.substr(0,pos_delim);
							str=str.substr(pos_delim+delim.length(),str.length()-pos_delim-delim.length());
							return First;
						}
					if(pos_delim>=str.length()||(pos_open>=str.length()&&pos_close>=str.length()))
					{
						First=str;
						str="";
						return First;
					}
					if(pos_open<str.length()&&pos_open<pos_close)
					{
						num_brackets++;
						pos=pos_open;
						pos++;
					}
					if(pos_close<str.length()&&pos_close<pos_open)
					{
						num_brackets--;
						pos=pos_close;
						pos++;
					}
				}
				First=str;
				str="";
				return First;
			}
			bool GetDelimited(const string &str,string &First,string &Remainder,const std::string &delim)
			{
				size_t pos=str.find(delim);
				if(!str.length())
				{
					First="";
					Remainder="";
					return false;
				}
				if(pos>=str.length())
				{
					First=Remainder;
					Remainder="";
					return true;
				}
				First=str.substr(0,pos);
				Remainder=str.substr(pos+delim.length(),str.length()-pos-delim.length());
				return true;
			}
			bool GetDelimited(const string &str,string &First,string &Remainder,char delim)
			{
				size_t pos=str.find(delim);
				if(!str.length())
				{
					First="";
					Remainder="";
					return false;
				}
				if(pos>=str.length())
				{
					First=Remainder;
					Remainder="";
					return true;
				}
				First=str.substr(0,pos);
				Remainder=str.substr(pos+1,str.length()-pos-1);
				return true;
			}
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
                    if(idx>=size_t(Line.length()))
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
                int val=int(strtol(num.c_str(),&stopstring,10));
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
			string StringOf(int i)
			{
				char txt[100];
				sprintf_s(txt,99,"%d",i);
				return string(txt);
			}
			string StringOf(float i)
			{
				char txt[100];
				sprintf_s(txt,99,"%g",i);
				return string(txt);
			}
		void StripDirectoryFromFilename(string &str)
		{
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
		std::string stringFormat(std::string fmt, ...)
		{
			int size=(int)fmt.size()+100;
			std::string str;
			va_list ap;
			int n=-1;
			const char *format_str=fmt.c_str();
			while(n<0||n>=size)
			{
				str.resize(size);
				va_start(ap, fmt);
				//n = vsnprintf_s((char *)str.c_str(), size, size,format_str, ap);
				n = vsnprintf((char *)str.c_str(), size,format_str, ap);
				va_end(ap);
				if(n> -1 && n < size)
				{
					str.resize(n);
					return str;
				}
				if (n > -1)
					size=n+1;
				else
					size*=2;
			}
			return str;
		}
		const char *QuickFormat(const char *format_str,...)
		{
			int size=(int)strlen(format_str)+100;
			static std::string str;
			va_list ap;
			int n=-1;
			while(n<0||n>=size)
			{
                str.resize(size_t(size));
				va_start(ap, format_str);
				//n = vsnprintf_s((char *)str.c_str(), size, size,format_str, ap);
				n = vsnprintf((char *)str.c_str(), size,format_str, ap);
				va_end(ap);
				if(n> -1 && n < size)
				{
					str.resize(n);
					return str.c_str();
				}
				if (n > -1)
					size=n+1;
				else
					size*=2;
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
		vector<string> split(const string& source, char separator)
		{
			vector<string> vec;
			int pos=0;
			while(pos>=0&&pos<(int)source.length())
			{
				int nextpos=(int)source.find(separator,pos);
				if(nextpos<0)
					nextpos=(int)source.length();
				if(nextpos>=0)
					vec.push_back(source.substr(pos,nextpos-pos));
				pos=nextpos+1;
			}
			return vec;
		}
		string toNext(const string& source,char separator,size_t &pos)
		{
			string res;
			size_t nextpos=(int)source.find(separator,pos);
			res=source.substr(pos,nextpos-pos);
			pos=std::min(source.size(),nextpos+1);
			return res;
		}
	}
}
