#include "MixedResolutionView.h"
#include "Simul/Math/Vector3.h"
#include "Simul/Camera/Camera.h"
using namespace simul;
using namespace crossplatform;

MixedResolutionView::MixedResolutionView()
	:depthFormat(RGBA_32_FLOAT)
	,pixelOffset(0.f,0.f)
	,ScreenWidth(0)
	,ScreenHeight(0)
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

void MixedResolutionView::UpdatePixelOffset(const crossplatform::ViewStruct &viewStruct,int scale)
{
	using namespace math;
	// Update the orientation due to changing view_dir:
	Vector3 cam_pos,new_view_dir,new_view_dir_local,new_up_dir;
	simul::camera::GetCameraPosVector(viewStruct.view,cam_pos,new_view_dir,new_up_dir);
	new_view_dir.Normalize();
	view_o.GlobalToLocalDirection(new_view_dir_local,new_view_dir);
	float dx			= new_view_dir*view_o.Tx();
	float dy			= new_view_dir*view_o.Ty();
	dx*=ScreenWidth*viewStruct.proj._11;
	dy*=ScreenHeight*viewStruct.proj._22;
	view_o.DefineFromYZ(new_up_dir,new_view_dir);
	static float cc=0.5f;
	pixelOffset.x-=cc*dx;
	pixelOffset.y-=cc*dy;

	pixelOffset=WrapOffset(pixelOffset,scale);
}

vec2 MixedResolutionView::LoResToHiResOffset(vec2 pixelOffset,int hiResScale)
{
	if(hiResScale<1)
		hiResScale=1;
	int2 intOffset=int2((int)pixelOffset.x,(int)pixelOffset.y);
	vec2 fracOffset=pixelOffset-vec2((float)intOffset.x,(float)intOffset.y);
	intOffset=int2(intOffset.x%hiResScale,intOffset.y%hiResScale);
	vec2 hi=vec2(intOffset.x,intOffset.y)+fracOffset;
	return hi;
}

vec2 MixedResolutionView::WrapOffset(vec2 pixelOffset,int scale)
{
	if(scale<1)
		scale=1;
	pixelOffset.x/=(float)scale;
	pixelOffset.y/=(float)scale;
	vec2 intOffset;
	pixelOffset.x = modf (pixelOffset.x , &intOffset.x);
	if(pixelOffset.x<0.0f)
		pixelOffset.x +=1.0f;
	pixelOffset.y = modf (pixelOffset.y , &intOffset.y);
	if(pixelOffset.y<0.0f)
		pixelOffset.y +=1.0f;
	
	pixelOffset.x*=(float)scale;
	pixelOffset.y*=(float)scale;
	return pixelOffset;
}