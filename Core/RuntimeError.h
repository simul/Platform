#ifndef PLATFORM_RUNTIMEERROR_H
#define PLATFORM_RUNTIMEERROR_H

#include "Export.h"
#include <string>
#include <string.h>
#include <iostream>
#include <cerrno>
#include <assert.h>

#ifndef SIMUL_INTERNAL_CHECKS
#define SIMUL_INTERNAL_CHECKS 0
#endif

#ifdef _MSC_VER
    #pragma warning(push)
	#pragma warning(disable:4996)
	#include <Windows.h>// for DebugBreak etc
#endif
#ifdef _WIN32
#define strerror_r(err_code, sys_msg, sizeofsys_msg) strerror_s(sys_msg, sizeofsys_msg, err_code)
#endif

#ifdef __ORBIS__
#define strerror_r(err_code, sys_msg, sizeofsys_msg) strerror_s(sys_msg, sizeofsys_msg, err_code)
#include <libdbg.h>
#endif
#ifdef UNIX
#define strerror_s(sys_msg, sizeofsys_msg, err_code) strerror_r(err_code, sys_msg, sizeofsys_msg)
#include <signal.h>
#include <cstring>
#endif
#include <stdexcept> // for runtime_error

#define SIMUL_COUT\
	std::cout<<__FILE__<<"("<<__LINE__<<"): info: "

#define SIMUL_CERR\
	std::cerr<<__FILE__<<"("<<__LINE__<<"): warning: "

namespace simul
{
	namespace base
	{
		#ifndef _MSC_VER	
		extern PLATFORM_CORE_EXPORT void DebugBreak();
		#endif
		extern PLATFORM_CORE_EXPORT bool DebugBreaksEnabled();
		extern PLATFORM_CORE_EXPORT void EnableDebugBreaks(bool b);
		extern PLATFORM_CORE_EXPORT bool SimulInternalChecks;
		//! This is a throwable error class derived from std::runtime_error.
		//! It is used in builds that have C++ exceptions enabled. As it always outputs to std::cerr,
		//! it is easier to see the nature of the error than with runtime_error alone.
		class RuntimeError : public std::runtime_error
		{
		public:
			RuntimeError(const char *what_arg)
				:std::runtime_error(what_arg)
		  {
			  std::cerr<<what_arg<<std::endl;
		  }
			RuntimeError(const std::string &what_arg)
				:std::runtime_error((const char*)what_arg.c_str())
		  {
			  std::cerr<<what_arg.c_str()<<std::endl;
		  }
			RuntimeError(int errn):std::runtime_error("")
			{
				char buffer[250];
				strerror_s((char*)buffer, 250, errn);
				SIMUL_CERR << buffer << std::endl;
			}
		};
	}
}

#define SIMUL_INTERNAL_CERR\
	if(simul::base::SimulInternalChecks)\
		std::cerr << __FILE__ << "(" << __LINE__ << "): warning: "

#define SIMUL_CERR_ONCE\
	static bool SIMUL_CERR_ONCE_done=false;\
	if(!SIMUL_CERR_ONCE_done)\
		std::cerr<<__FILE__<<"("<<__LINE__<<"): warning: "<<(SIMUL_CERR_ONCE_done=true)<<": "

#define SIMUL_INTERNAL_CERR_ONCE\
	static bool SIMUL_CERR_ONCE_done=false;\
	if(simul::base::SimulInternalChecks&&!SIMUL_CERR_ONCE_done)\
		std::cerr<<__FILE__<<"("<<__LINE__<<"): warning: "<<(SIMUL_CERR_ONCE_done=true)<<": "

#define SIMUL_CERR_ONCE_PER(inst)\
	static std::map<void*,bool> SIMUL_CERR_ONCE_PER_done;\
	if(SIMUL_CERR_ONCE_PER_done.find(inst)==SIMUL_CERR_ONCE_PER_done.end())\
			std::cerr<<__FILE__<<"("<<__LINE__<<"): warning: "<<(SIMUL_CERR_ONCE_PER_done[inst]?"":"")

#ifdef __EXCEPTIONS
	#define SIMUL_THROW(err)\
	{\
		static bool done=false;\
		if(!done)\
			SIMUL_CERR<<err<<std::endl;\
		done=true;\
		throw simul::base::RuntimeError(err);\
	}		
#else
	#define SIMUL_THROW(err)\
		SIMUL_CERR<<err<<std::endl;
#endif

#define SIMUL_FILE_CERR(filename)\
	std::cerr<<filename<<"("<<__LINE__<<"): warning B0001: "

#define SIMUL_FILE_LINE_CERR(filename,line)\
	std::cerr<<filename<<"("<<line<<"): warning B0001: "

#ifdef _DEBUG
#define SIMUL_ASSERT(value)\
	if(value!=true)\
	{\
		std::cerr<<__FILE__<<"("<<__LINE__<<"): warning B0001: "<<"SIMUL_ASSERT failed on: "<<#value<<std::endl;\
		BREAK_IF_DEBUGGING\
	}
#else
#define SIMUL_ASSERT(value)
#endif
#define SIMUL_BREAK(msg)\
	{\
		std::cerr<<__FILE__<<"("<<__LINE__<<"): warning B0001: "<<msg<<std::endl;\
		BREAK_IF_DEBUGGING\
	}

#define SIMUL_BREAK_ONCE(msg)\
	{\
		static bool done=false;\
		if(!done) \
		{ std::cerr<<__FILE__<<"("<<__LINE__<<"): warning B0001: "<<msg<<std::endl; BREAK_IF_DEBUGGING ; done=true; } \
	}

#define SIMUL_ASSERT_WARN(val,message)\
	if((val)!=true)\
	{\
		std::cerr<<__FILE__<<"("<<__LINE__<<"): warning B0001: "<<"SIMUL_ASSERT_WARN failed : "<<#val<<" "<<message<<std::endl;\
	}

#define SIMUL_ASSERT_WARN_ONCE(val,message)\
	{\
		static bool done=false;\
		if((val)!=true&&!done)\
		{\
			std::cerr<<__FILE__<<"("<<__LINE__<<"): warning B0001: "<<"SIMUL_ASSERT_WARN failed : "<<#val<<" "<<message<<std::endl;\
			BREAK_IF_DEBUGGING\
			done=true;\
		}\
	}

#ifdef _DEBUG
#define SIMUL_NULL_CHECK_RETURN(val,message)\
	{\
		static bool done=false;\
		if((val)==nullptr)\
		{\
			if(!done)\
			{\
				std::cerr<<__FILE__<<"("<<__LINE__<<"): warning B0001: "<<"SIMUL_NULL_CHECK_RETURN: "<<#val<<" "<<message<<std::endl;\
				BREAK_IF_DEBUGGING\
				done=true;\
			}\
			return;\
		}\
	}
#else
#define SIMUL_NULL_CHECK_RETURN(val,message)
#endif

#ifdef _MSC_VER
	#define BREAK_IF_DEBUGGING\
		{\
			if(simul::base::DebugBreaksEnabled()&&IsDebuggerPresent())\
				DebugBreak();\
		}
#else
	#if defined(__ORBIS__) && defined(_DEBUG)
		#define BREAK_IF_DEBUGGING if(simul::base::DebugBreaksEnabled()&&sceDbgIsDebuggerAttached()) SCE_BREAK();
	#else
		// None of the __builtin_debugtrap, __debugbreak, raise(SIGTRAP) etc work properly in Linux with LLDB. They stop the program permanently, with no call stack.
		// Therefore we use this workaround.
		#define BREAK_IF_DEBUGGING simul::base::DebugBreak();
	#endif
#endif

#define BREAK_ONCE_IF_DEBUGGING\
		{static bool done=false;\
		if(!done)\
			BREAK_IF_DEBUGGING\
		done=true;}

/// This errno check can be disabled for production. ALWAYS_ERRNO_CHECK must always be enabled as it is used for functionality.
#ifdef NDEBUG
	#define ERRNO_CHECK\
		errno=0;
#else
	#define ERRNO_CLEAR \
		errno=0;

#ifndef UNIX
	#define ERRNO_CHECK \
		if(errno!=0)\
		{\
			char errno_e[101];\
			strerror_r(errno,errno_e,100);\
			std::cerr<<__FILE__<<"("<<__LINE__<<"): warning B0001: "<<"WARNING: errno!=0: "<<errno_e<<std::endl;\
			errno=0;\
		}
#else
	#define ERRNO_CHECK \
		if(errno!=0)\
		{\
			char buffer[256];\
			int err=errno;\
			char * errorMsg = strerror_r( errno, buffer, 256 ); \
			std::cerr<<__FILE__<<"("<<__LINE__<<"): warning B0001: "<<"WARNING: errno!=0: "<<(errorMsg?errorMsg:"")<<std::endl; \
			errno=0;\
		}
#endif
#endif
/// This errno check is always enabled, wherease ERRNO_CHECK can be disabled for production.
	#define ALWAYS_ERRNO_CHECK \
		if(errno!=0)\
		{\
			char errno_e[101];\
			int err=errno;\
			strerror_r(err,errno_e,100);\
			std::cerr<<__FILE__<<"("<<__LINE__<<"): warning B0001: "<<"WARNING: errno=="<<err<<": "<<errno_e<<std::endl;\
			errno=0;\
			SIMUL_THROW(errno_e);\
		}
/// This errno check is only used to find specific bugs, then removed from the code.
#define ERRNO_BREAK \
		if(errno!=0)\
		{\
			char errno_e[401];\
			int err=errno;\
			strerror_r(err,errno_e,400);\
			std::cerr<<__FILE__<<"("<<__LINE__<<"): warning B0001: "<<"WARNING: errno=="<<err<<": "<<errno_e<<std::endl;\
			BREAK_IF_DEBUGGING	\
			errno=0;\
		}

#ifdef _MSC_VER
    #pragma warning(pop)
#endif

#endif
