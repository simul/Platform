

#ifndef SPGLSL_SL
#define SPGLSL_SL
#include "CppGlsl.hs"

profile vs_4_0(410);
profile ps_4_0(410);
profile gs_4_0(410);
profile vs_5_0(430);
profile ps_5_0(430);
profile cs_5_0(430);
profile gs_5_0(430);
#define SV_DispatchThreadID gl_GlobalInvocationID 
#define SV_VertexID gl_VertexID
#endif

