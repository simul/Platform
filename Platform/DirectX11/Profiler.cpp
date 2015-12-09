#include "Profiler.h"
#include "DX11Exception.h"
#include "Utilities.h"
#include "Simul/Base/StringFunctions.h"
#include "Simul/Base/RuntimeError.h"
#include "Simul/Base/StringToWString.h"
#include "Simul/Platform/CrossPlatform/RenderPlatform.h"
#include "Simul/Platform/CrossPlatform/DeviceContext.h"
#ifdef SIMUL_ENABLE_PIX
#include "pix.h"
#endif
using namespace simul;
using namespace dx11;
using std::string;
using std::map;
#pragma optimize("",off)
// Throws a DXException on failing HRESULT
inline void DXCall(HRESULT hr)
{
	if (FAILED(hr))
    {
        _ASSERT(false);
		throw DX11Exception(hr);
    }
}
// == Profiler ====================================================================================

Profiler Profiler::GlobalProfiler;

Profiler &Profiler::GetGlobalProfiler()
{
	return GlobalProfiler;
}

Profiler::Profiler():pUserDefinedAnnotation(NULL)
{

}

Profiler::~Profiler()
{
}

void Profiler::StartFrame(crossplatform::DeviceContext &deviceContext)
{
#ifdef SIMUL_WIN8_SDK
	if(enabled)
	{
		SAFE_RELEASE(pUserDefinedAnnotation);
		IUnknown *unknown=(IUnknown *)ctx;
#ifdef _XBOX_ONE
		V_CHECK(unknown->QueryInterface( __uuidof(pUserDefinedAnnotation), reinterpret_cast<void**>(&pUserDefinedAnnotation) ));
#else
		V_CHECK(unknown->QueryInterface(IID_PPV_ARGS(&pUserDefinedAnnotation)));
#endif
}
#endif
	GpuProfiler::StartFrame(deviceContext);
}

void Profiler::Begin(crossplatform::DeviceContext &deviceContext,const char *name)
{
	GpuProfiler::Begin(deviceContext,name);
#ifdef SIMUL_WIN8_SDK
    crossplatform::ProfileData *profileData = NULL;
	auto u=profileMap.find(qualified_name);
	if(u!=profileMap.end())
		profileData=u->second;
	if(u!=profileMap.end())
	{
		profileData=u->second;
		if(!profileData->wUnqualifiedName.length())
			profileData->wUnqualifiedName=base::StringToWString(name);
		if(pUserDefinedAnnotation)
			pUserDefinedAnnotation->BeginEvent(profileData->wUnqualifiedName.c_str());
	}
#endif
#ifdef SIMUL_ENABLE_PIX
	if(last_name==string(name))
		PIXBeginEvent( 0, profileData->wUnqualifiedName.c_str(), name );
#endif
}

void Profiler::End()
{
	GpuProfiler::End();
#ifdef SIMUL_WIN8_SDK
	if(pUserDefinedAnnotation)
		pUserDefinedAnnotation->EndEvent();
#endif
#ifdef SIMUL_ENABLE_PIX
	PIXEndEvent(  );
#endif
}

void Profiler::EndFrame(crossplatform::DeviceContext &deviceContext)
{
	GpuProfiler::EndFrame(deviceContext);
#ifdef SIMUL_WIN8_SDK
	SAFE_RELEASE(pUserDefinedAnnotation);
#endif
}

float Profiler::GetTime(const std::string &name) const
{
    if(!enabled||!renderPlatform)
		return 0.f;
	return profileMap.find(name)->second->time;
}


// == ProfileBlock ================================================================================

ProfileBlock::ProfileBlock(crossplatform::DeviceContext &deviceContext,const std::string& name)
	:name(name)
	,context(&deviceContext)
{
	Profiler::GetGlobalProfiler().Begin(deviceContext,name.c_str());
}

ProfileBlock::~ProfileBlock()
{
    Profiler::GetGlobalProfiler().End();
}

float ProfileBlock::GetTime() const
{
	return Profiler::GetGlobalProfiler().GetTime(name);
}