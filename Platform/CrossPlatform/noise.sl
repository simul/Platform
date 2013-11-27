#ifndef PLATFORM_CROSSPLATFORM_NOISE_SL
#define PLATFORM_CROSSPLATFORM_NOISE_SL

SIMUL_CONSTANT_BUFFER(RendernoiseConstants,10)
	uniform int octaves;
	uniform float persistence;
	uniform float noisepad1;
	uniform float noisepad2;
SIMUL_CONSTANT_BUFFER_END

#ifndef __cplusplus
float rand(vec2 co)
{
    return fract(sin(dot(co.xy ,vec2(12.9898,78.233))) * 43758.5453);
}

float rand3(vec3 co)
{
    return fract(sin(dot(co.xyz,vec3(12.9898,78.233,42.1897))) * 43758.5453);
}

vec4 Noise(Texture2D noise_texture,vec2 texCoords,float persistence,int octaves)
{
	vec4 result=vec4(0,0,0,0);
	float mult=.5;
	float total=0.0;
    for(int i=0;i<octaves;i++)
    {
		vec4 c=texture_wrap_lod(noise_texture,texCoords,0);
		texCoords*=2.0;
		total+=mult;
		result+=mult*c;
		mult*=persistence;
    }
	// divide by total to get the range -1,1.
	result*=1.0/total;
    return result;
}
#endif
#endif