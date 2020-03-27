//  Copyright (c) 2015 Simul Software Ltd. All rights reserved.
#ifndef IMAGE_CONSTANTS_SL
#define IMAGE_CONSTANTS_SL

SIMUL_CONSTANT_BUFFER(ImageConstants,13)
	uniform uint2	imageSize;
	uniform uint	g_NumApproxPasses;
	uniform uint	texelsPerThread;
	uniform float	g_HalfBoxFilterWidth;			// w/2
	uniform float	g_FracHalfBoxFilterWidth;		// frac(w/2+0.5)
	uniform float	g_InvFracHalfBoxFilterWidth;	// 1-frac(w/2+0.5)
	uniform float	g_RcpBoxFilterWidth;			// 1/w
SIMUL_CONSTANT_BUFFER_END


#endif
