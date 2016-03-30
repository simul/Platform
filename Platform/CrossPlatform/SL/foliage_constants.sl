//  Copyright (c) 2015 Simul Software Ltd. All rights reserved.
#ifndef FOLIAGE_CONSTANTS_SL
#define FOLIAGE_CONSTANTS_SL

SIMUL_CONSTANT_BUFFER(FoliageConstants,11)
	uniform mat4	transformMatrix;
	uniform mat4	invViewProj;
	uniform uint3	texture_size;
	uniform uint	grid_size;
SIMUL_CONSTANT_BUFFER_END

#endif
