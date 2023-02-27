#pragma once

#include <time.h>

//struct tm *localtime(const time_t *timer);
// reimplement this standard function as microsoft does not support it:
inline struct tm *localtime_r(const time_t * tt,struct tm * result)
{
   // signature of localtime_s is:
   //	  errno_t __CRTDECL localtime_s(
   //              _Out_ struct tm*    const _Tm,
   //              _In_  time_t const* const _Time
   //          )
	auto res=localtime_s(result,tt);
	res;
	return result;
}