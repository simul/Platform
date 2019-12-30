#include "Light.h"

using namespace simul;

int crossplatform::Light::sLightCount = 0;

crossplatform::PropertyChannel::~PropertyChannel()
{
	delete mAnimCurve;
}

float crossplatform::PropertyChannel::Get(double time) const
{
	if (mAnimCurve)
	{
		return mAnimCurve->Evaluate(time);
	}
	else
	{
		return mValue;
	}
}

crossplatform::Light::Light() : mType(ePoint)
{
}

crossplatform::Light::~Light()
{
    --sLightCount;
}


void crossplatform::Light::SetLight(const double *posmat,double time) const
{
    const float lLightColor[4]	={mColorRed.Get(time),mColorGreen.Get(time),mColorBlue.Get(time), 1.0f};
    const float lConeAngle		=mConeAngle.Get(time);
	UpdateLight(posmat,lConeAngle,lLightColor);
}