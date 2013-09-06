#ifndef PLATFORM_CROSSPLATFORM_NOISE_SL
#define PLATFORM_CROSSPLATFORM_NOISE_SL
float rand(vec2 co)
{
    return fract(sin(dot(co.xy ,vec2(12.9898,78.233))) * 43758.5453);
}

float rand3(vec3 co)
{
    return fract(sin(dot(co.xyz,vec3(12.9898,78.233,12.9898))) * 43758.5453);
}
#endif