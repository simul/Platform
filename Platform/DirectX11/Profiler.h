#pragma once

// Platform SDK defines, specifies that our min version is Windows Vista
#ifndef WINVER
#define WINVER 0x0600
#endif

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif

#ifndef _WIN32_WINDOWS
#define _WIN32_WINDOWS 0x0410
#endif

#ifndef _WIN32_IE
#define _WIN32_IE 0x0700
#endif

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
// C RunTime Header Files
// C++ Standard Library Header Files
#include <string>
#include <vector>
#include <map>
#include <sstream>

#pragma warning(push)
#pragma warning(disable:4251)

#ifdef _DEBUG
#ifndef D3D_DEBUG_INFO
#define D3D_DEBUG_INFO
#endif
#endif

#include <dxgi.h>
#include <d3d11.h>
#include <d3dx11.h>
#include <dxerr.h>

#include <string>
#include "Simul/Base/Timer.h"
#include "Simul/Base/ProfilingInterface.h"
#include "MacrosDX1x.h"

#include "Simul/Platform/DirectX11/Export.h"
namespace simul
{
	namespace dx11
	{
		SIMUL_DIRECTX11_EXPORT_CLASS Profiler:public simul::base::GpuProfilingInterface
		{
		public:
			static Profiler &GetGlobalProfiler();
			~Profiler();
			void Initialize(ID3D11Device* device);
			void Uninitialize();
					void Begin(void *context,const char *name);
					void End();

			void EndFrame(ID3D11DeviceContext* context);
	
			float GetTime(const std::string &name) const;
			//! Get all the active profilers as a text report.
			const char *GetDebugText() const;
		protected:
			std::vector<std::string> last_name;
			std::vector<ID3D11DeviceContext *> last_context;
			static Profiler GlobalProfiler;

			// Constants
			static const UINT64 QueryLatency = 5;

			struct ProfileData
			{
				ID3D11Query *DisjointQuery[QueryLatency];
				ID3D11Query *TimestampStartQuery[QueryLatency];
				ID3D11Query *TimestampEndQuery[QueryLatency];
				BOOL QueryStarted;
				BOOL QueryFinished;
				float time;
				ProfileData()
					:QueryStarted(false)
					,QueryFinished(false)
					, time(0.f)
				{
					for(int i=0;i<QueryLatency;i++)
					{
						DisjointQuery[i]		=0;
						TimestampStartQuery[i]	=0;
						TimestampEndQuery[i]	=0;
					}
				}
				~ProfileData()
				{
					for(int i=0;i<QueryLatency;i++)
					{
						SAFE_RELEASE(DisjointQuery[i]);
						SAFE_RELEASE(TimestampStartQuery[i]);
						SAFE_RELEASE(TimestampEndQuery[i]);
					}
				}
			};

			typedef std::map<std::string, ProfileData*> ProfileMap;

			ProfileMap profiles;
			UINT64 currFrame;

			ID3D11Device* device;

			simul::base::Timer timer;
			std::string output;
		};

		class ProfileBlock
		{
		public:

			ProfileBlock(ID3D11DeviceContext* c,const std::string& name);
			~ProfileBlock();

			/// Get the previous frame's timing value.
			float GetTime() const;
		protected:
			ID3D11DeviceContext* context;
			std::string name;
		};
	}
}
#pragma warning(pop)