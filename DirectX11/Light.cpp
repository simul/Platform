#include "Light.h"

using namespace platform;
using namespace dx11;

namespace
{

    const float DEFAULT_LIGHT_POSITION[]			={0.0f, 0.0f, 0.0f, 1.0f};
    const float DEFAULT_DIRECTION_LIGHT_POSITION[]	={0.0f, 0.0f, 1.0f, 0.0f};
    const float DEFAULT_SPOT_LIGHT_DIRECTION[]		={0.0f, 0.0f, -1.0f};
    const float DEFAULT_LIGHT_COLOR[]				={1.0f, 1.0f, 1.0f, 1.0f};
    const float DEFAULT_LIGHT_SPOT_CUTOFF			=180.0f;
}


dx11::Light::Light()
{
}

dx11::Light::~Light()
{
}

void dx11::Light::UpdateLight(const double *,float ,const float[4]) const
{
}