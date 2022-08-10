#include "Light.h"

using namespace simul;

gles::Light::Light()
{
    mLightIndex = 0;
}

gles::Light::~Light()
{
}

void gles::Light::UpdateLight(const double * /*lLightGlobalPosition*/,float lConeAngle,const float lLightColor[4]) const
{
}