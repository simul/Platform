//  Copyright (c) 2015 Simul Software Ltd. All rights reserved.
#ifndef NOISE_CONSTANTS_SL
#define NOISE_CONSTANTS_SL

SIMUL_CONSTANT_BUFFER(RendernoiseConstants,10)
	uniform int octaves;
	uniform float persistence;
	uniform int noise_texture_size;
	uniform int mip;
	uniform int noise_texture_frequency;
	uniform float strength;
	uniform float rnc_pad1;
	uniform float rnc_pad2;
SIMUL_CONSTANT_BUFFER_END

#endif
