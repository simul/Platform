#pragma once
// C RunTime Header Files
// C++ Standard Library Header Files
#include "SimulDirectXHeader.h"
#include <sdkddkver.h>
#ifndef SIMUL_WIN8_SDK
#include <d3dx11.h>
#include <dxerr.h>
#include <dxgi.h>
#endif
#include <string>
#include <vector>
#include <set>
#include <map>
#include <sstream>

#pragma warning(push)
#pragma warning(disable:4251)

#ifdef _DEBUG
#ifndef D3D_DEBUG_INFO
#define D3D_DEBUG_INFO
#endif
#endif

#include "SimulDirectXHeader.h"
#ifndef SIMUL_WIN8_SDK
#include <d3dx11.h>
#include <dxerr.h>
#include <dxgi.h>
#else
	#ifndef _XBOX_ONE
		#include <D3D11_1.h>
	#endif
#endif

#include <string>
#include "Simul/Base/Timer.h"
#include "Simul/Base/ProfilingInterface.h"
#include "MacrosDX1x.h"

#include "Simul/Platform/DirectX11/Export.h"
#ifndef _XBOX_ONE
	struct ID3DUserDefinedAnnotation;
#endif
namespace simul
{
	namespace dx11
	{
		SIMUL_DIRECTX11_EXPORT_CLASS Profiler:public simul::base::GpuProfilingInterface
		{
		public:
			static Profiler &GetGlobalProfiler();
			Profiler();
			~Profiler();
			void Initialize(ID3D11Device* device);
			void Uninitialize();
			void Begin(void *context,const char *name);
			void End();
			
			void StartFrame(void* context);
			void EndFrame(void* context);
			ID3DUserDefinedAnnotation *pUserDefinedAnnotation;
			float GetTime(const std::string &name) const;
			//! Get all the active profilers as a text report.
			const char *GetDebugText(bool as_html=false) const;
			std::string GetChildText(const char *name,std::string tab) const;
			std::vector<std::string> last_name;
			std::vector<ID3D11DeviceContext *> last_context;
			static Profiler GlobalProfiler;

			// Constants
			static const UINT64 QueryLatency = 6;

			struct ProfileData;
			typedef std::map<std::string,ProfileData*> ProfileMap;
			typedef std::map<int,ProfileData*> ChildMap;
			struct ProfileData
			{
				ProfileData *parent;
				ID3D11Query *DisjointQuery[QueryLatency];
				ID3D11Query *TimestampStartQuery[QueryLatency];
				ID3D11Query *TimestampEndQuery[QueryLatency];
				bool gotResults[QueryLatency];
				BOOL QueryStarted;
				BOOL QueryFinished;
				std::string full_name;
				std::string unqualifiedName;
				std::wstring wUnqualifiedName;
				float time;
				ProfileData()
					:QueryStarted(false)
					,QueryFinished(false)
					,time(0.f)
					,parent(NULL)
					,last_child_updated(0)
					,child_index(0)
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
				ChildMap children;
				int last_child_updated;
				int child_index;
			};
		protected:
			ProfileMap profileMap;
			ProfileMap rootMap;
			UINT64 currFrame;

			ID3D11Device* device;

			simul::base::Timer timer;
			float queryTime;
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