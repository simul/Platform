
#include "Platform/Core/RuntimeError.h"
#include "Platform/CrossPlatform/Camera.h"
#include "Platform/Math/Quaternion.h"
#include "Platform/Math/Matrix4x4.h"
#include "Platform/Math/MatrixVector3.h"
#include "Platform/CrossPlatform/BaseRenderer.h"
#include "Platform/CrossPlatform/Camera.h"
#include "Platform/Math/Pi.h"
#include <memory.h>
#include <algorithm>
using namespace platform;
using namespace math;
using namespace crossplatform;

void ViewStruct::PushModelMatrix(math::Matrix4x4 m)
{
	math::Matrix4x4 newModel;
	Multiply4x4(newModel, model, m);
	model=newModel;
	modelStack.push(newModel);
}

void ViewStruct::PopModelMatrix()
{
	modelStack.pop();
	model = modelStack.top();
}

void ViewStruct::Init()
{
	ERRNO_CHECK
	while(modelStack.size()>1)
	{
		modelStack.pop();
	}
	if(modelStack.size() ==0)
	{
		model= math::Matrix4x4::IdentityMatrix();
		modelStack.push(model);
	}
	else
	{
		modelStack.top()=math::Matrix4x4::IdentityMatrix();
	}
	frustum = GetFrustumFromProjectionMatrix(proj);
	MakeViewProjMatrix((float*)&viewProj, (const float*)&view, (const float*)&proj);
	MakeInvViewProjMatrix((float*)&invViewProj, (const float*)&view, (const float*)&proj);
	view.Inverse(*((platform::math::Matrix4x4*) & invView));
	GetCameraPosVector((const float*)&view, (float*)&cam_pos, (float*)&view_dir, (float*)&up);
	depthToLinearDistanceParameters=GetDepthToLinearDistanceParameters(1.0f);
	initialized = true;
}


vec4 ViewStruct::GetDepthToLinearDistanceParameters(float unit_distance_metres) const
{
	return GetDepthToDistanceParameters(depthTextureStyle,*this, unit_distance_metres);
}
