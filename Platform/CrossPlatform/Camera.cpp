#include "Simul/Base/RuntimeError.h"
#include "Simul/Platform/CrossPlatform/Camera.h"
#include "Simul/Math/Quaternion.h"
#include "Simul/Math/Matrix4x4.h"
#include "Simul/Math/MatrixVector3.h"
#include "Simul/Platform/CrossPlatform/BaseRenderer.h"
#include "Simul/Math/Pi.h"
#include <memory.h>
using namespace simul;
using namespace math;
using namespace crossplatform;
static const float DEG_TO_RAD=pi/180.f;
static const float RAD_TO_DEG=180.f/pi;
/*
The projection matrix converts cartesian coordinates in view-space (e.g. in metres relative to camera-fixed xyz axes) into projection space.
Here is a symmetric projection matrix:

[Xp Yp Zp Wp]	=	[Xv Yv Zv 1]	[ m11	0	0	0	]	
									|   0	m22	0	0	|	
									|   0	0	m33	-1	|	
									[   0	0	m43	0	]	

So, Xp		=m11 Xv,
	Yp		=m22 Yv
	Zp		=m33 Zv + m43
and Wp		=-Zv.

Because of the -1 at the (3,4) position, this is a RIGHT-HANDED projection: x is right, y is up, and z must be negative in front of the camera.
LEFT-HANDED matrices can also be used.

After this, to get to screen co-ordinates, there's a division step that occurs
in-hardware. X, Y and Z, are all divided by W.

	Xscreen	= M11 Xv / -Zv

So what goes in the hardware depth buffer, which in DX11 we can read as a texture provided it is in a readable format,
is:
	Zp		= (m33 Zv + m43) / -Zv

When we want linear distance, we must extract Zv from Zp using the matrix:

	-Zp Zv			= m33 Zv + m43
	 Zv (Zp+m33)	= -m43
	 Zv				= -m43 / (Zp + m33)

Note that this is Zv in the original units, e.g. metres, and that the camera in this case is in the negative-Z direction. This is 
For example, a standard forward projection matrix (0=near, 1=far) has:

	m33	= F/(N-F);		m34	=-1;
	m43	= NF/(N-F);

i.e: Zp =F(1+ N/Zv)/(F-N)
		=F(1+ N/Zv)/(F-N)
		
and inversely:
	Zv	= NF/(Zp(F-N)-F)

So, remembering that in this case Z in front of the camera is negative, we have:

for Zv = -N, Zp = 0
for Zv = -F, Zp = 1
*/

static float U(float x)
{
	return atan(x/2.f);
}

vec4 simul::crossplatform::GetDepthToDistanceParameters(const crossplatform::ViewStruct &viewStruct,float max_dist_metres)
{
//	vec4 c(proj[3*4+2],max_dist_metres,proj[2*4+2]*max_dist_metres,0);
	if(viewStruct.depthTextureStyle==crossplatform::PROJECTION)
		return vec4(viewStruct.proj.m[3][2], max_dist_metres,viewStruct.proj.m[2][2]*max_dist_metres,0.0f);
	if(viewStruct.depthTextureStyle==crossplatform::DISTANCE_FROM_NEAR_PLANE)
	{
		Frustum f=simul::crossplatform::GetFrustumFromProjectionMatrix(viewStruct.proj);
		return vec4(f.nearZ/max_dist_metres, 0.0f,1.f,(f.farZ-f.nearZ)/max_dist_metres);
	}
	return vec4(0,0,0,0);
}

Frustum simul::crossplatform::GetFrustumFromProjectionMatrix(const float *mat)
{
	Frustum frustum;
	Matrix4x4 M(mat);
	float z0=-M._43/M._33;
	float z1=(M._43-M._44)/(M._34-M._33);
	if(z0>z1)
	{
		frustum.reverseDepth=false;
		frustum.nearZ=-z0;
		frustum.farZ=-z1;
	}
	else
	{
		frustum.reverseDepth=true;
		frustum.nearZ=-z1;
		frustum.farZ=-z0;
	}
	frustum.tanHalfHorizontalFov=1.f/M._11;
	frustum.tanHalfVerticalFov=1.f/M._22;
	return frustum;
}


void simul::crossplatform::ModifyProjectionMatrix(float *mat,float new_near_plane,float new_far_plane)
{
	Matrix4x4 *m=(Matrix4x4*)mat;
	Matrix4x4 &M=(*m);
	float z0=-M._43/M._33;
	float z1=(M._43-M._44)/(M._34-M._33);
	float nearZ=0.0f,farZ=1.0f;
	if(z0>z1)
	{
		nearZ=-z0;
		farZ=-z1;
		M._33	=	new_far_plane/(new_near_plane-new_far_plane);			M._34	=-1.f;
		M._43	=	new_near_plane*new_far_plane/(new_near_plane-new_far_plane);
	}
	else// reversedepth
	{
		nearZ=-z1;
		farZ=-z0;
		if(new_far_plane>0)
		{
			M._33	=new_near_plane/(new_far_plane-new_near_plane);	M._34	=-1.f;
			M._43	=new_far_plane*new_near_plane/(new_far_plane-new_near_plane);
		}
		else // infinite far plane.
		{
			M._33	=0.f;					M._34	=-1.f;
			M._43	=new_near_plane;
		}
	}
}

void simul::crossplatform::ConvertReversedToRegularProjectionMatrix(float *p)
{
	simul::math::Matrix4x4 &proj=*((simul::math::Matrix4x4*)p);
	if(proj._43>0)
	{
		float zF	=proj._43/proj._33;
		float zN	=proj._43*zF/(zF+proj._43);
		proj._33	=-zF/(zF-zN);
		proj._43	=-zN*zF/(zF-zN);
	}
}

void simul::crossplatform::MakeInvViewProjMatrix(float *ivp,const float *v,const float *p)
{
	simul::math::Matrix4x4 view(v),proj(p);
	simul::math::Matrix4x4 vpt;
	simul::math::Matrix4x4 viewproj;
	view(3,0)=view(3,1)=view(3,2)=0;
	simul::math::Multiply4x4(viewproj,view,proj);
	viewproj.Transpose(vpt);
	simul::math::Matrix4x4 invp;
	viewproj.Inverse(*((simul::math::Matrix4x4*)ivp));
}

void simul::crossplatform::MakeInvWorldViewProjMatrix(float *ivp,const float *world,const float *v,const float *p)
{
	simul::math::Matrix4x4 view(v),proj(p);
	simul::math::Matrix4x4 vpt;
	simul::math::Matrix4x4 wvp,tmp1;
	view(3,0)=view(3,1)=view(3,2)=0;
	simul::math::Multiply4x4(tmp1,world,view);
	simul::math::Multiply4x4(wvp,tmp1,proj);
	wvp.Transpose(vpt);
	wvp.Inverse(*((simul::math::Matrix4x4*)ivp));
}

void simul::crossplatform::MakeViewProjMatrix(float *vp,const float *v,const float *p)
{
	simul::math::Matrix4x4 viewProj, tmp,view(v),proj(p);
	simul::math::Multiply4x4(tmp,view,proj);
	tmp.Transpose(*((simul::math::Matrix4x4*)vp));
}

void simul::crossplatform::MakeCentredViewProjMatrix(float *vp,const float *v,const float *p)
{
	simul::math::Matrix4x4 viewProj, tmp,view(v),proj(p);
	view._41=view._42=view._43=0;
	simul::math::Multiply4x4(tmp,view,proj);
	tmp.Transpose(*((simul::math::Matrix4x4*)vp));
}

void simul::crossplatform::MakeWorldViewProjMatrix(float *wvp,const float *w,const float *v,const float *p)
{
	simul::math::Matrix4x4 tmp1,tmp2,view(v),proj(p),world(w);
	simul::math::Multiply4x4(tmp1,view,proj);
	simul::math::Multiply4x4(tmp2,world,tmp1);
	tmp2.Transpose(*((simul::math::Matrix4x4*)wvp));
}

void simul::crossplatform::MakeCentredWorldViewProjMatrix(float *wvp,const float *w,const float *v,const float *p)
{
	simul::math::Matrix4x4 tmp1,tmp2,view(v),proj(p),world(w);
	view._41=view._42=view._43=0;
	simul::math::Multiply4x4(tmp1,world,view);
	simul::math::Multiply4x4(tmp2,tmp1,proj);
	tmp2.Transpose(*((simul::math::Matrix4x4*)wvp));
}

void simul::crossplatform::GetCameraPosVector(const float *v,float *dcam_pos,float *view_dir,float *up)
{
	simul::math::Matrix4x4 tmp1,view(v);
	ERRNO_BREAK
	view.Inverse(tmp1);
	ERRNO_BREAK
	dcam_pos[0]=tmp1._41;
	dcam_pos[1]=tmp1._42;
	dcam_pos[2]=tmp1._43;
	if(view_dir)
	{
		view_dir[0]=-view._13;
		view_dir[1]=-view._23;
		view_dir[2]=-view._33;
	}
	if(up)
	{
		up[0]=view._12;
		up[1]=view._22;
		up[2]=view._32;
	}
}

const float *simul::crossplatform::GetCameraPosVector(const float *v)
{
	ERRNO_BREAK
	simul::math::Matrix4x4 view(v);
	static float cam_pos[4],view_dir[4];
	GetCameraPosVector(view,(float*)cam_pos,(float*)view_dir);
	return cam_pos;
}

using namespace simul::math;
Matrix4x4 simul::crossplatform::MatrixLookInDirection(const float *dir,const float *view_up)
{ 
	Matrix4x4 M;
	M.ResetToUnitMatrix();
	Vector3 zaxis = dir;
	Vector3 up = view_up;
	zaxis*=-1.f;
	zaxis.Normalize();
	Vector3 xaxis = up^zaxis;
	xaxis.Normalize();
	Vector3 yaxis = zaxis^xaxis;
	yaxis.Normalize();
	M.InsertColumn(0,xaxis);
	M.InsertColumn(1,yaxis);
	M.InsertColumn(2,zaxis);
//zaxis = normal(Eye - At)
//xaxis = normal(cross(Up, zaxis))
//yaxis = cross(zaxis, xaxis)
// xaxis.x           yaxis.x           zaxis.x          0
 //xaxis.y           yaxis.y           zaxis.y          0
 //xaxis.z           yaxis.z           zaxis.z          0
 //dot(xaxis, eye)   dot(yaxis, eye)   dot(zaxis, eye)  1
	return M;
}
void simul::crossplatform::MakeCubeMatrices(simul::math::Matrix4x4 mat[],const float *cam_pos,bool ReverseDepth)
{
#ifdef SIMUL_WIN8_SDK
	vec3 vEyePt ={cam_pos[0],cam_pos[1],cam_pos[2]};
    vec3 vLookAt;
    vec3 vUpDir;
#else
	vec3 vEyePt (cam_pos[0],cam_pos[1],cam_pos[2]);
    vec3 vLookAt;
    vec3 vUpDir;
#endif
    memset(mat,0,6*sizeof(math::Matrix4x4));
    /*D3DCUBEMAP_FACE_POSITIVE_X     = 0,
    D3DCUBEMAP_FACE_NEGATIVE_X     = 1,
    D3DCUBEMAP_FACE_POSITIVE_Y     = 2,
    D3DCUBEMAP_FACE_NEGATIVE_Y     = 3,
    D3DCUBEMAP_FACE_POSITIVE_Z     = 4,
    D3DCUBEMAP_FACE_NEGATIVE_Z     = 5,*/
	static const float lookf[6][3]=
	{
		 {-1.f,0.f,0.f}		,{1.f,0.f,0.f}
		,{0.f,-1.f,0.f}		,{0.f,1.f,0.f}
		,{0.f,0.f,-1.f}		,{0.f,0.f,1.f}
	};
	static const float upf[6][3]=
	{
		 {0.f,-1.f,0.f}		,{0.f,-1.f,0.f}
		,{0.f,0.f,1.f}		,{0.f,0.f,-1.f}
		,{0.f,-1.f,0.f}		,{0.f,-1.f,0.f}
	};
	for(int i=0;i<6;i++)
	{
		vUpDir		=upf[i];
		if(ReverseDepth)
		{
			vUpDir		*=-1.0f;
		}
		vLookAt		=vEyePt+lookf[i];
		vUpDir		=upf[i];
		//D3DXMatrixLookAtRH((D3DXMATRIX*)&mat[i], &vEyePt, &vLookAt, &vUpDir );
		mat[i]		=MatrixLookInDirection((const float*)&lookf[i],vUpDir);
		Vector3 loc_cam_pos;
		simul::math::Multiply3(loc_cam_pos,mat[i],cam_pos);
		loc_cam_pos*=-1.f;
		mat[i].InsertRow(3,loc_cam_pos);
		mat[i]._44=1.f;
	}
}
Camera::Camera():Orientation()
{
	InitializePropertiesDefinition();
	VerticalFieldOfViewInRadians=60.f*pi/180.f;
	HorizontalFieldOfViewInRadians=0;
	Orientation.Rotate(3.14f/2.f,simul::math::Vector3(1,0,0));
}

Camera::~Camera()
{
}

const float *Camera::MakeViewMatrix() const
{
	static Matrix4x4 M;
	Orientation.T4.Inverse(M);
	return M.RowPointer(0);
}

// Really making projT here.
const float *Camera::MakeDepthReversedProjectionMatrix(float h,float v,float zNear,float zFar)
{
	float xScale= 1.f/tan(h/2.f);
	float yScale = 1.f/tan(v/2.f);
	math::Matrix4x4 m;
	memset(&m,0,16*sizeof(float));
	m._11	=xScale;
	m._22	=yScale;
	if(zFar>0)
	{
		m._33	=zNear/(zFar-zNear);	m._34	=-1.f;
		m._43	=zFar*zNear/(zFar-zNear);
	}
	else // infinite far plane.
	{
		m._33	=0.f;					m._34	=-1.f;
		m._43	=zNear;
	}
	// testing:
	GetFrustumFromProjectionMatrix(m);
	return m;
}

const float *Camera::MakeDepthReversedProjectionMatrix(const FovPort &fovPort, float zNear, float zFar)
{
	float xScale	= 2.0f / (fovPort.leftTan + fovPort.rightTan);
	float xOffset	 = (fovPort.leftTan - fovPort.rightTan) * xScale * 0.5f;
	float yScale	= 2.0f / (fovPort.upTan + fovPort.downTan);
	float yOffset	= (fovPort.upTan - fovPort.downTan) * yScale * 0.5f;

	static float handedness = 1.0f;// right-handed obviously.
	math::Matrix4x4 m;
	memset(&m, 0, 16 * sizeof(float));
	m._11 = xScale;
	m._22 = yScale;
	m._31 = -handedness*xOffset;
	m._32 = handedness *yOffset;
	if (zFar>0)
	{
		m._33 = zNear / (zFar - zNear);	m._34 = -1.f;
		m._43 = zFar*zNear / (zFar - zNear);
	}
	else // infinite far plane.
	{
		m._33 = 0.f;					m._34 = -1.f;
		m._43 = zNear;
	}
	// testing:
	GetFrustumFromProjectionMatrix(m);
	return m;
}

const float *Camera::MakeDepthReversedProjectionMatrix(float aspect) const
{
	float h=HorizontalFieldOfViewInRadians;
	float v=VerticalFieldOfViewInRadians;
	if(aspect&&v&&!h)
		h=2.f*tan(U(v)*aspect);
	if(aspect&&h&&!v)
		v=2.f*tan(U(h)/aspect);
	static float max_fov=160.0f*pi/180.0f;
	if(h>max_fov)
		h=max_fov;
	if(v>max_fov)
		v=max_fov;
	if(!h)
		h=v;
	if(!v)
		v=h;
	return MakeDepthReversedProjectionMatrix(h,v,cameraViewStruct.nearZ,cameraViewStruct.InfiniteFarPlane?0.f:cameraViewStruct.farZ);
}

const float *Camera::MakeProjectionMatrix(float h,float v,float zNear,float zFar)
{
	float xScale = 1.f / tan(h / 2.f);
	float yScale = 1.f / tan(v / 2.f);
	math::Matrix4x4 m;
	memset(&m, 0, 16 * sizeof(float));
	m._11 = xScale;
	m._22 = yScale;

	m._33 = zFar / (zNear - zFar);		m._34 = -1.f;
	m._43 = zNear*zFar / (zNear - zFar);

	return m;
}
const float *Camera::MakeProjectionMatrix(const FovPort &fovPort, float zNear, float zFar)
{
	float xScale = 2.0f / (fovPort.leftTan + fovPort.rightTan);
	float xOffset = (fovPort.leftTan - fovPort.rightTan) * xScale * 0.5f;
	float yScale = 2.0f / (fovPort.upTan + fovPort.downTan);
	float yOffset = (fovPort.upTan - fovPort.downTan) * yScale * 0.5f;

	static float handedness = 1.0f;// right-handed obviously.
	math::Matrix4x4 m;
	memset(&m, 0, 16 * sizeof(float));
	m._11 = xScale;
	m._22 = yScale;
	m._12 = -handedness*xOffset;
	m._23 = handedness *yOffset;

	m._33 = zFar / (zNear - zFar);		m._34 = -1.f;
	m._43 = zNear*zFar / (zNear - zFar);

	return m;
}


const float *Camera::MakeProjectionMatrix(float aspect) const
{
// According to the documentation for D3DXMatrixPerspectiveFovLH:
//	xScale     0          0               0
//	0        yScale       0               0
//	0          0       zf/(zf-zn)         1
//	0          0       -zn*zf/(zf-zn)     0
//	where:
//	yScale = cot(fovY/2)
//
//	xScale = yScale / aspect ratio
// But for RH:
//	xScale     0          0               0
//	0        yScale       0               0
//	0          0       zf/(zn-zf)         -1
//	0          0       zn*zf/(zn-zf)     0
	float h=HorizontalFieldOfViewInRadians;
	float v=VerticalFieldOfViewInRadians;
	if(aspect&&v&&!h)
		h=2.f*tan(U(v)*aspect);
	if(aspect&&h&&!v)
		v=2.f*tan(U(h)/aspect);
	static float max_fov=160.0f*pi/180.0f;
	if(h>max_fov)
		h=max_fov;
	if(v>max_fov)
		v=max_fov;
	if(!h)
		h=v;
	if(!v)
		v=h;
	return MakeProjectionMatrix(h,v,cameraViewStruct.nearZ,cameraViewStruct.farZ);
}
			
const float *Camera::MakeStereoProjectionMatrix(WhichEye ,float aspect,bool ReverseDepth) const
{
	if(ReverseDepth)
		return MakeDepthReversedProjectionMatrix(aspect);
	else
		return MakeProjectionMatrix(aspect);
}

const float *Camera::MakeStereoViewMatrix(WhichEye ) const
{
	return MakeViewMatrix();
}

		float length(const vec3 &u)
		{
			float size=u.x*u.x+u.y*u.y+u.z*u.z;
			return sqrt(size);
		}

vec3 Camera::ScreenPositionToDirection(float x,float y,float aspect)
{
	Matrix4x4 proj			=MakeDepthReversedProjectionMatrix(aspect);
	Matrix4x4 view			=MakeViewMatrix();
	Matrix4x4 ivp;
	simul::crossplatform::MakeInvViewProjMatrix((float*)&ivp,view,proj);
	Vector3 clip(x*2.0f-1.0f,1.0f-y*2.0f,1.0f);
	vec3 res;
	ivp.Transpose();
	Multiply4(*((Vector3*)&res),ivp,clip);
	res=res/length(res);
	return res;
}

void Camera::SetCameraViewStruct(const CameraViewStruct &c)
{
	cameraViewStruct=c;
}

const CameraViewStruct &Camera::GetCameraViewStruct() const
{
	return cameraViewStruct;
}

	// virtual from OrientationInterface
const float *Camera::GetOrientationAsPermanentMatrix() const
{
	return (const float *)(&Orientation.T4);
}

const float *Camera::GetRotationAsQuaternion() const
{
	static simul::math::Quaternion q;
	simul::math::MatrixToQuaternion(q,Orientation.T4);
	return (const float *)(&q);
}

const float *Camera::GetPosition() const
{ 
	return (const float *)(&Orientation.T4.GetRowVector(3));
}

void Camera::SetOrientationAsMatrix(const float *m)
{
	Orientation.T4=m;
}

void Camera::SetOrientationAsQuaternion(const float *q)
{
	Orientation.Define(simul::math::Quaternion(q));
}

void Camera::SetPosition(const float *x)
{
	Orientation.SetPosition(simul::math::Vector3(x));
}

void Camera::SetPositionAsXYZ(float x,float y,float z)
{
	Orientation.SetPosition(simul::math::Vector3(x,y,z));
}

void Camera::Move(const float *x)
{
	Orientation.Translate(x);
}

void Camera::LocalMove(const float *x)
{
	Orientation.LocalTranslate(x);
}
				
void Camera::Rotate(float x,const float *a)
{
	Orientation.Rotate(x,a);
}		
void Camera::LocalRotate(float x,const float *a)
{
	Orientation.LocalRotate(x,a);
}
void Camera::LocalRotate(const float *a)
{
	Orientation.LocalRotate(simul::math::Vector3(a));
}

bool Camera::TimeStep(float )
{
	return true;
}

float Camera::GetHorizontalFieldOfViewDegrees() const
{
	return HorizontalFieldOfViewInRadians*RAD_TO_DEG;
}

void Camera::SetHorizontalFieldOfViewDegrees(float f)
{
	HorizontalFieldOfViewInRadians=f*DEG_TO_RAD;
}

float Camera::GetVerticalFieldOfViewDegrees() const
{
	return VerticalFieldOfViewInRadians*RAD_TO_DEG;
}

void Camera::SetVerticalFieldOfViewDegrees(float f)
{
	VerticalFieldOfViewInRadians=f*DEG_TO_RAD;
}

void Camera::LookInDirection(const float *view_dir,const float *view_up)
{
	Vector3 d(view_dir);
	d.Normalize();
	Vector3 u(view_up);
	Vector3 x=d^u;
	x.Normalize();
	u=x^d;
	Orientation.DefineFromYZ(u,d);
}


void Camera::LookInDirection(const float *view_dir)
{
	Vector3 d(view_dir);
	d.Normalize();
	Vector3 x=d^Vector3(0,0,1.f);
	x.Normalize();
	Vector3 u=x^d;
	Orientation.DefineFromYZ(u,-d);
}

void simul::crossplatform::UpdateMouseCamera(	Camera *cam
					,float time_step
					,float cam_spd
					,MouseCameraState &state
					,MouseCameraInput &input
					,float max_height)
{
	simul::math::Vector3 pos=cam->GetPosition();

	static float CameraDamping=1e4f;
	float retain			=1.f/(1.f+CameraDamping*time_step);
	float introduce			=1.f-retain;

	state.forward_back_spd	*=retain;
	state.right_left_spd	*=retain;
	state.up_down_spd		*=retain;
	state.forward_back_spd	+=input.forward_back_input*cam_spd*introduce;
	state.right_left_spd	+=input.right_left_input*cam_spd*introduce;
	state.up_down_spd		+=input.up_down_input*cam_spd*introduce;

	pos						-=state.forward_back_spd*time_step*cam->GetOrientation().Tz();
	pos						+=state.right_left_spd*time_step*cam->GetOrientation().Tx();
	pos.z					+=state.up_down_spd*time_step;

	if(pos.z>max_height)
		pos.z=max_height;

	cam->SetPosition(pos);

	int dx=input.MouseX-input.LastMouseX;
	int dy=input.MouseY-input.LastMouseY;
	float mouseDeltaX=0.f,mouseDeltaY=0.f;
	if(input.MouseButtons&(MouseCameraInput::LEFT_BUTTON|MouseCameraInput::RIGHT_BUTTON))
	{
		mouseDeltaX =dx/750.f;
		mouseDeltaY =dy/750.f;
	}
	input.LastMouseX=input.MouseX;
	input.LastMouseY=input.MouseY;

	static float x_rotate=0.f;
	static float y_rotate=0.f;
	x_rotate		*=retain;
	x_rotate		+=mouseDeltaX*introduce;
	mouseDeltaX		=0.f;
	y_rotate		*=retain;
	y_rotate		+=mouseDeltaY*introduce;
	mouseDeltaY		=0.f;

	simul::math::Vector3 vertical(0,0,-1.f);
	simul::math::Vector3 del	=vertical*x_rotate;
	simul::math::Vector3 dir	=del;
	dir.Normalize();
	cam->Rotate(del.Magnitude(),dir);

	del=cam->GetOrientation().Tx()*y_rotate*(-1.f);
	dir	=del;
	dir.Normalize();
	cam->Rotate(del.Magnitude(),dir);

	float tilt	=0;
	tilt		=asin(cam->GetOrientation().Tx().z);
	dir			=cam->GetOrientation().Tz();
	dir.Normalize();
	cam->Rotate(-0.5f*tilt,dir);
}