#ifndef NOISE_CONSTANTS_SL
#define NOISE_CONSTANTS_SL

SIMUL_CONSTANT_BUFFER(RendernoiseConstants,10)
	uniform int octaves;
	uniform float persistence;
	uniform int noise_texture_size;
	uniform float noisepad2;
SIMUL_CONSTANT_BUFFER_END

#endif
