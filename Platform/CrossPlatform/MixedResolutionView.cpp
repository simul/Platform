#include "MixedResolutionView.h"
using namespace simul;
using namespace crossplatform;

MixedResolutionView::MixedResolutionView()
	:depthFormat(RGBA_32_FLOAT)
{
}


MixedResolutionView::~MixedResolutionView()
{
}

crossplatform::PixelFormat MixedResolutionView::GetDepthFormat() const
{
	return depthFormat;
}

void MixedResolutionView::SetDepthFormat(crossplatform::PixelFormat p)
{
	depthFormat=p;
}