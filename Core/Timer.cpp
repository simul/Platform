#include "Platform/Core/Timer.h"
#include <math.h>
using namespace platform::core;
#ifdef _MSC_VER
	#include <time.h>
	#if defined (WIN32) || defined (WIN64) || defined(WINAPI_FAMILY_DESKTOP_APP) //WinGDK
		#include <windows.h>
		#include <mmsystem.h>
	#elif defined (_GAMING_XBOX)
		#include <gxdk.h>
		#include <wrl.h>
	#else
		#error("Unknown platform")
	#endif
	#pragma optimize("",off)
#else
	#define LARGE_INTEGER long
#endif
#if defined(__ORBIS__) | defined(__COMMODORE__)
#include <perf.h>
static uint64_t performanceCounter()
	{
		uint64_t result = 0;
		timespec ts;
		clock_gettime(CLOCK_MONOTONIC, &ts);
		result = (uint64_t)ts.tv_sec * 1000000000LL + (uint64_t)ts.tv_nsec;
		return result;
	}
	uint64_t performanceFrequency()
	{
		uint64_t result = 1;
		result = 1000000000LL;
		return result;
	}
#endif

void Timer::EmptyFunction()
{
	return;
}

__int64 Timer::OverheadTicks=0;

Timer::Timer()
	:Time(0)
	,TimeSum(0)
{          
#ifdef _MSC_VER
	LARGE_INTEGER PerfFreq;
	if(!QueryPerformanceFrequency(&PerfFreq))
		return;
    dPerfFreq = float(PerfFreq.QuadPart)/1000.f;
	// We can use hires timer, determine overhead
	if(!OverheadTicks)
	{
		OverheadTicks=200;//;600000
		for(int i=0;i<200;i++)
		{
			LARGE_INTEGER b,e;
			LONGLONG Ticks;
			QueryPerformanceCounter(&b);
			EmptyFunction();
			QueryPerformanceCounter(&e);
			Ticks = e.QuadPart - b.QuadPart;
			if ( Ticks > 0 && Ticks < OverheadTicks )
				OverheadTicks = Ticks;
		}
	}
	Oht = OverheadTicks;

#endif
	StartTime();
}

Timer::~Timer()
{
}

void Timer::StartTime()
{
#ifdef _MSC_VER
//	Freq=PerfFreq;
	Oht=OverheadTicks;
	LARGE_INTEGER &tStart=*(reinterpret_cast<LARGE_INTEGER*>(&iStart));
	QueryPerformanceCounter(&tStart);
#endif
#if defined(__ORBIS__) | defined(__COMMODORE__)
	iStart=(double)performanceCounter();
#endif
}

float Timer::FinishTime()
{
#ifdef _MSC_VER
	LARGE_INTEGER &tStop=*(reinterpret_cast<LARGE_INTEGER*>(&iStop));
	LARGE_INTEGER &tStart=*(reinterpret_cast<LARGE_INTEGER*>(&iStart));
	QueryPerformanceCounter(&tStop);
	if(dPerfFreq==0)
		 Time=0.0;
	else
		Time=((float)(tStop.QuadPart-tStart.QuadPart-Oht))/(float)dPerfFreq;
	TimeSum+=Time;
#endif
#if defined(__ORBIS__) | defined(__COMMODORE__)
	iStop=(double)performanceCounter();
	Time=(float)(1000.0*(double)(iStop-iStart)/(double)performanceFrequency());
	iStart=iStop;
	TimeSum+=Time;
#endif
	return Time;
}

float Timer::AbsoluteTimeMS()
{
	float t=0.f;
#ifdef _MSC_VER
	LARGE_INTEGER &tStop=*(reinterpret_cast<LARGE_INTEGER*>(&iStop));
	LARGE_INTEGER &tStart=*(reinterpret_cast<LARGE_INTEGER*>(&iStart));
	QueryPerformanceCounter(&tStop);
	if(dPerfFreq==0)
		 t=0.0;
	else
		t=((float)(tStop.QuadPart-tStart.QuadPart))/(float)dPerfFreq;
#endif
#ifdef __ORBIS__
	return (float)(1000.0*(double)performanceCounter()/(double)performanceFrequency());
#endif
	return t;
}
