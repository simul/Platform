#include "Light.h"

using namespace simul;
using namespace dx11on12;

namespace
{

    const float DEFAULT_LIGHT_POSITION[]			={0.0f, 0.0f, 0.0f, 1.0f};
    const float DEFAULT_DIRECTION_LIGHT_POSITION[]	={0.0f, 0.0f, 1.0f, 0.0f};
    const float DEFAULT_SPOT_LIGHT_DIRECTION[]		={0.0f, 0.0f, -1.0f};
    const float DEFAULT_LIGHT_COLOR[]				={1.0f, 1.0f, 1.0f, 1.0f};
    const float DEFAULT_LIGHT_SPOT_CUTOFF			=180.0f;
}


dx11on12::Light::Light()
{
}

dx11on12::Light::~Light()
{
}

void dx11on12::Light::UpdateLight(const double *lLightGlobalPosition,float lConeAngle,const float lLightColor[4]) const
{
}