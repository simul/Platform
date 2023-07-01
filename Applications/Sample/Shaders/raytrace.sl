#ifndef RAYTRACINGSL
#define RAYTRACINGSL

struct Viewport
{
    float left;
    float top;
    float right;
    float bottom;
};

PLATFORM_NAMED_CONSTANT_BUFFER(RayGenConstantBuffer, g_rayGenCB, 0)
    Viewport viewport;
    Viewport stencil;
PLATFORM_NAMED_CONSTANT_BUFFER_END

#endif