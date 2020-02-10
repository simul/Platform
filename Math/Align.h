#ifndef SIMUL_ALIGN_H
#define SIMUL_ALIGN_H
#define CANNOT_ALIGN

#if !defined(DOXYGEN) && !defined(__GNUC__)
	#define ALIGN16 _declspec(align(16))
#else
	#define ALIGN16
#endif
#endif
