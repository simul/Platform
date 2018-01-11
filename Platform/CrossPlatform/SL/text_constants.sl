//  Copyright (c) 2015 Simul Software Ltd. All rights reserved.
#ifndef TEXT_CONSTANTS_SL
#define TEXT_CONSTANTS_SL

SIMUL_CONSTANT_BUFFER(TextConstants,8)
	vec4	colour;
	vec4	background;
	vec4	background_rect;
SIMUL_CONSTANT_BUFFER_END

struct FontChar
{
	vec4	text_rect;
	vec4	texc;
};
#endif