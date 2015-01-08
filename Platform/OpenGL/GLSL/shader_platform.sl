

#ifndef SPGLSL_SL
#define SPGLSL_SL
#include "CppGlsl.hs"
struct idOnly
{
	int gl_VertexID;
};

profile vs_4_0(410);
profile ps_4_0(410);
profile vs_5_0(430);
profile ps_5_0(430);
profile cs_5_0(430);
#define SV_DispatchThreadID gl_GlobalInvocationID 
#endif

