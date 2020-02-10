#define NOMINMAX
#include "Simul/Base/RuntimeError.h"
#include "Simul/Platform/CrossPlatform/Camera.h"
#include "Simul/Platform/Math/Quaternion.h"
#include "Simul/Platform/Math/Matrix4x4.h"
#include "Simul/Platform/Math/MatrixVector3.h"
#include "Simul/Platform/CrossPlatform/BaseRenderer.h"
#include "Simul/Platform/Math/Pi.h"
#include <memory.h>
#include <algorithm>
using namespace simul;
using namespace math;
using namespace crossplatform;

void ViewStruct::Init()
{
	ERRNO_BREAK
	frustum=GetFrustumFromProjectionMatrix(proj);
	MakeViewProjMatrix((float*)&viewProj, (const float*)&view, (const float*)&proj);
	MakeInvViewProjMatrix((float*)&invViewProj,(const float*)&view,(const float*)&proj);
	view.Inverse(*((simul::math::Matrix4x4*)&invView));
	GetCameraPosVector((const float *)&view,(float*)&cam_pos,(float *)&view_dir,(float*)&up);
	initialized=true;
}

static const float DEG_TO_RAD=SIMUL_PI_F/180.f;
static const float RAD_TO_DEG=180.f/SIMUL_PI_F;
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
/*
static float U(float x)
{
	return atan(x/2.f);
}
*/
vec4 simul::crossplatform::GetDepthToDistanceParameters(DepthTextureStyle depthTextureStyle, const ViewStruct &viewStruct, float max_dist_metres)
{
	// 	Z = x/(depth*y + z)+w*depth;
	// e.g. for depth-rev, infinite far plane:
	//	m._33	=0.f;					m._34	=-1.f;
	//	m._43	=zNear;
	// so x=m43=nearplane, y=1.0, z= 0, w=0.
	// and Z = near/depth
	//	vec4 c(proj[3*4+2],max_dist_metres,proj[2*4+2]*max_dist_metres,0);
	if (depthTextureStyle == crossplatform::PROJECTION)
		return vec4(viewStruct.proj.m[3][2], max_dist_metres,viewStruct.proj.m[2][2] * max_dist_metres, 0.0f);
	if (depthTextureStyle == crossplatform::DISTANCE_FROM_NEAR_PLANE)
	{
		return vec4(viewStruct.frustum.nearZ / max_dist_metres, 0.0f, 1.f, (viewStruct.frustum.farZ - viewStruct.frustum.nearZ) / max_dist_metres);
	}
	return vec4(0, 0, 0, 0);
}


vec4 simul::crossplatform::GetDepthToDistanceParameters(DepthTextureStyle depthTextureStyle, const math::Matrix4x4 &proj, float max_dist_metres)
{
	// 	Z = x/(depth*y + z)+w*depth;
	// e.g. for depth-rev, infinite far plane:
	//	m._33	=0.f;					m._34	=-1.f;
	//	m._43	=zNear;
	// so x=m43=nearplane, y=1.0, z= 0, w=0.
	// and Z = near/depth
	//	vec4 c(proj[3*4+2],max_dist_metres,proj[2*4+2]*max_dist_metres,0);
	if (depthTextureStyle == crossplatform::PROJECTION)
		return vec4(proj.m[3][2], max_dist_metres, proj.m[2][2] * max_dist_metres, 0.0f);
	if (depthTextureStyle == crossplatform::DISTANCE_FROM_NEAR_PLANE)
	{
		Frustum f = simul::crossplatform::GetFrustumFromProjectionMatrix(proj);
		return vec4(f.nearZ / max_dist_metres, 0.0f, 1.f, (f.farZ - f.nearZ) / max_dist_metres);
	}
	return vec4(0, 0, 0, 0);
}

vec4 simul::crossplatform::GetDepthToDistanceParameters(const crossplatform::ViewStruct &viewStruct,float max_dist_metres)
{
	return GetDepthToDistanceParameters(viewStruct.depthTextureStyle, viewStruct, max_dist_metres);
}

static float x_sgn = -1.f;
static float y_sgn = -1.f;
Frustum simul::crossplatform::GetFrustumFromProjectionMatrix(const float *mat)
{
	Frustum frustum;
	Matrix4x4 M(mat);
	float z0=0.0f;
	// We do NOT want to divide by zero, although an infinite far plane is valid. So we use z0=0 to stand in for infinity.
	if(M._33!=0.0f)
		z0=-M._43/M._33;
	float z1=(M._43-M._44)/(M._34-M._33);
	if(z0>z1&&z0!=0.0f)
	{
		frustum.reverseDepth=false;
		frustum.nearZ		=-z0;
		frustum.farZ		=-z1;
	}
	else
	{
		frustum.reverseDepth=true;
		frustum.nearZ		=-z1;
		frustum.farZ		=-z0;
	}
	frustum.tanHalfFov.x=fabs(M._34/M._11);
	frustum.tanHalfFov.y=fabs(M._34/M._22);
	frustum.tanHalfFov.z= x_sgn*M._31/M._11;
	frustum.tanHalfFov.w= y_sgn*M._32/M._22;
	
	return frustum;
}

float simul::crossplatform::GetDepthAtDistance(float thresholdMetres, const float *proj)
{
	// let pos=(0,0,t)
	// then the vector after multiplying is (0 , 0 ,  (M33 t + M43), (M34 t + M44))
	// and the Z value is (M33 t + M43)/ (M34 t + M44).
	// e.g. suppose t is the far z, for a reverse proj.
	// then 0 = M33 t + M43, and t = -M43/M33, which is what we have above.
	// or if t is the near, we have M33 t + M43 = M34 t + M44
	// which gives t(M33-M34)=M44-M43, again, as above.
	Matrix4x4 M(proj);
	// But remember our right-handed projection matrices need the viewspace Z to be negative.
	// Obviously we can't assume we'll be given a RH matrix, so look at M34.
	if(M._34<0)
		thresholdMetres=-thresholdMetres;
	float z =fabs((M._33*(thresholdMetres)+ M._43) / (M._34*(thresholdMetres) + M._44));
	// in the case of forward projection we have
	//m._33 = zFar / (zNear - zFar);		m._34 = -1.f;
	//m._43 = zNear*zFar / (zNear - zFar);
	// this gives Z=(far*t+near*far)/(near-far)/(-t + 0)
	//           Z=(F+N*F/t)/(N-F)
	//            =(1+N/t)/(N/F-1)
	return z;
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
	simul::math::Matrix4x4 tmp1,tmp2,view(v),proj(p),world;
	if(w)
	{
		simul::math::Multiply4x4(tmp1,view,proj);
		world=w;
		simul::math::Multiply4x4(*((simul::math::Matrix4x4*)wvp),world,tmp1);
	}
	else
	{
		simul::math::Multiply4x4(*((simul::math::Matrix4x4*)wvp),view,proj);
	}
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
	simul::math::Matrix4x4 tmp1;
	const simul::math::Matrix4x4 &view(*((const simul::math::Matrix4x4*)v));
	
	view.Inverse(tmp1);
	
	if(dcam_pos)
	{
		dcam_pos[0]=tmp1._41;
		dcam_pos[1]=tmp1._42;
		dcam_pos[2]=tmp1._43;
	}
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
	
	simul::math::Matrix4x4 view(v);
	static float cam_pos[4],view_dir[4];
	GetCameraPosVector(view,(float*)cam_pos,(float*)view_dir);
	return cam_pos;
}

using namespace simul::math;
Matrix4x4 simul::crossplatform::MatrixLookInDirection(const float *dir,const float *view_up,bool lefthanded)
{ 
	Matrix4x4 M;
	M.ResetToUnitMatrix();
	Vector3 zaxis = dir;
	Vector3 up = view_up;
	if(!lefthanded)
	zaxis*=-1.f;
	zaxis.Normalize();
	Vector3 xaxis = up^zaxis;
	if(lefthanded)
		xaxis*=-1.0f;
	xaxis.Normalize();
	Vector3 yaxis = zaxis^xaxis;
	if(lefthanded)
		yaxis*=-1.0f;
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

vec4 simul::crossplatform::GetFrustumRangeOnCubeFace(int face,const float *invViewProj)
{
	vec4 range(1.0,1.0,-1.0,-1.0);
	mat4 faceMatrix;
	GetCubeMatrix((float*)&faceMatrix,face,true,false);

	mat4 &ivp=*((mat4*)invViewProj);
	static float sc = 1.0f;
	static const vec4 clips[]=			{vec4(-1.0f,-1.0f,1.0f,1.0f)
							,vec4( 1.0f,-1.0f,1.0f,1.0f)
							,vec4( 1.0f, 1.0f,1.0f,1.0f)
							,vec4(-1.0f, 1.0f,1.0f,1.0f)};
	for(int i=0;i<4;i++)
	{
		static int A=7;
		for(int j=0;j<A;j++)
		{
			float v		=float(j)/float(A);
			float u		=1.0f-v;
			vec4 c		=(clips[i]*u+clips[(i+1)%4]*v);
			c.x *= sc;
			c.y *= sc;
			vec4 w		=ivp*c;
			w.w=1.0f;
			vec4 tra	=faceMatrix*w;
		
			if(tra.z<0)
				continue;
			tra			/=tra.z;
			range.x		=std::min(range.x,tra.x);
			range.y		=std::min(range.y,tra.y);
			range.z		=std::max(range.z,tra.x);
			range.w		=std::max(range.w,tra.y);
		}
	}
	range.x*=-1.0f;
	range.z*=-1.0f;
	std::swap(range.x,range.z);

	range+=vec4(1.0f,1.0f,1.0f,1.0f);
	range*=0.5f;
	range.x=std::min(std::max(range.x,0.0f),1.0f);
	range.y=std::min(std::max(range.y,0.0f),1.0f);
	range.z=std::min(std::max(range.z,0.0f),1.0f);
	range.w=std::min(std::max(range.w,0.0f),1.0f);
	return range;
}

void simul::crossplatform::MakeCubeMatrices(simul::math::Matrix4x4 mat[],const float *cam_pos,bool ReverseDepth,bool ReverseDirection)
{
	vec3 vEyePt (cam_pos[0],cam_pos[1],cam_pos[2]);
    vec3 vLookDir;
    vec3 vUpDir;
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
		vLookDir	=lookf[i];
		vUpDir		=upf[i];
		mat[i]		=MatrixLookInDirection((const float*)&vLookDir,vUpDir,false);
		if(ReverseDirection)
		{
			mat[i]*=-1.0f;
		}
		Vector3 loc_cam_pos;
		simul::math::Multiply3(loc_cam_pos,mat[i],cam_pos);
		loc_cam_pos*=-1.f;
		mat[i].InsertRow(3,loc_cam_pos);
		mat[i]._44=1.f;
	}
}

void simul::crossplatform::GetCubeMatrix(float *mat4x4,int face,bool ReverseDepth,bool ReverseDirection)
{
	vec3 zero_pos(0,0,0);
	
	static math::Matrix4x4 view_matrices[4][6];
	static bool init[]={false,false,false,false};
	int combo=(ReverseDepth?2:0)+(ReverseDirection?1:0);
	if(!init[combo])
		{
		MakeCubeMatrices(view_matrices[combo],(const float*)&zero_pos,ReverseDepth,ReverseDirection);
		init[combo]=true;
		}
	memcpy(mat4x4,view_matrices[combo][face],sizeof(float)*16);
}

void simul::crossplatform::GetCubeMatrixAtPosition(float *mat4x4,int face,vec3 cam_pos,bool ReverseDepth,bool ReverseDirection)
{
	GetCubeMatrix(mat4x4,face,ReverseDepth,ReverseDirection);
	Vector3 loc_cam_pos;
	simul::math::Multiply3(loc_cam_pos,*((const math::Matrix4x4*)mat4x4),*((const math::Vector3*)&cam_pos));
	loc_cam_pos*=-1.f;
	((math::Matrix4x4*)mat4x4)->InsertRow(3,loc_cam_pos);
	((math::Matrix4x4*)mat4x4)->_44=1.f;
}

void MakeCubeInvViewProjMatrices(simul::math::Matrix4x4 mat[],bool ReverseDepth,bool ReverseDirection)
{
	static math::Matrix4x4 view,proj;
	proj			=crossplatform::Camera::MakeDepthReversedProjectionMatrix(SIMUL_PI_F/2.f,SIMUL_PI_F/2.f,1.0f,0.0f);
	for(int i=0;i<6;i++)
	{
		GetCubeMatrix((float*)&view,i,ReverseDepth,ReverseDirection);
		MakeInvViewProjMatrix((float*)&mat[i],view,proj);
	}
}

void simul::crossplatform::GetCubeInvViewProjMatrix(float *mat4x4,int face,bool ReverseDepth,bool ReverseDirection)
{
	static bool init[]={false,false,false,false};
	int combo=(ReverseDepth?2:0)+(ReverseDirection?1:0);
	static math::Matrix4x4 ivp_matrices[4][6];
	if(!init[combo])
		{
		MakeCubeInvViewProjMatrices(ivp_matrices[combo],ReverseDepth,ReverseDirection);
		init[combo]=true;
		}
	memcpy(mat4x4,ivp_matrices[combo][face],sizeof(float)*16);
}
math::Matrix4x4 simul::crossplatform::MakeOrthoProjectionMatrix(float left,
 	float right,
 	float bottom,
 	float top,
 	float nearVal,
 	float farVal)
{
	math::Matrix4x4 m;
	m.ResetToUnitMatrix();
	float rl=right-left;
	float tb=top-bottom;
	float fn=farVal-nearVal;
	m.m00=2.0f/(rl);
	m.m11=2.0f/(tb);
	m.m22=-2.0f/(fn);
	m.m03=(right+left)/(rl);
	m.m13=(top+bottom)/(tb);
	m.m23=(farVal+nearVal)/(fn);
	m.m33=1.f;
	return m;
}

Camera::Camera():Orientation()
{
	InitializeProperties();
	VerticalFieldOfViewInRadians=60.f*SIMUL_PI_F/180.f;
	HorizontalFieldOfViewInRadians=0;
	Orientation.Rotate(3.14f/2.f,simul::math::Vector3(1,0,0));
}

Camera::~Camera()
{
}

const float *Camera::MakeViewMatrix() const
{
	return Orientation.GetInverseMatrix().RowPointer(0);
}

// Really making projT here.
const float *Camera::MakeDepthReversedProjectionMatrix(float h,float v,float zNear,float zFar)
{
	float xScale= 1.f/tan(h/2.f);
	float yScale = 1.f/tan(v/2.f);
	/// TODO: Not thread-safe
	static math::Matrix4x4 m;
	memset(&m,0,16*sizeof(float));
	m._11	=xScale;
	m._22	=yScale;
	if(zFar>0)
	{
		m._33	=zNear/(zFar-zNear);	m._34	=-1.f;
		m._43	=zFar*zNear/(zFar-zNear);
		// i.e. z=((n/(f-n)*d+nf/(f-n))/(-d)
		//		 =(n+nf/d)/(f-n)
		//		 =n(1+f/d)/(f-n)
		// so at d=-f, z=0
		// and at d=-n, z=(n-f)/(f-n)=-1

		// suppose we say:
		//			-Z=(n-nf/d)/(f-n)
		// i.e.      Z=n(f/d-1)/(f-n)

		// then we would have:
		//			at d=n, Z	=n(f/n-1)/(f-n)
		//						=(f-n)/(f-n)=1.0
		// and at      d=f, Z	=n(f/f-1)/(f-n)
		//						=0.
	}
	else // infinite far plane.
	{
		m._33	=0.f;					m._34	=-1.f;
		m._43	=zNear;
		// z = (33 Z + 43) / (34 Z +44)
		//   = near/-Z
		// z = -n/Z
		// But if z = (33 Z + 34) / (43 Z +44)
		// z = -1/nZ
	}
	// testing:
	//GetFrustumFromProjectionMatrix(m);
	return m;
}

const float *Camera::MakeDepthReversedProjectionMatrix(const FovPort &fovPort, float zNear, float zFar)
{
	float xScale	= 2.0f / (fovPort.leftTan + fovPort.rightTan);
	float xOffset	 = (fovPort.leftTan - fovPort.rightTan) * xScale * 0.5f;
	float yScale	= 2.0f / (fovPort.upTan + fovPort.downTan);
	float yOffset	= (fovPort.upTan - fovPort.downTan) * yScale * 0.5f;

	static float handedness = 1.0f;// right-handed obviously.
	/// TODO: Not thread-safe
	static math::Matrix4x4 m;
	memset(&m, 0, 16 * sizeof(float));
	m._11 = xScale;
	m._22 = yScale;
	m._31 = -handedness*xOffset;
	m._32 = handedness *yOffset;
	if (zFar>0)
	{
		m._33 = zNear / (zFar - zNear);	m._34 = -1.f;
		m._43 = zFar*zNear / (zFar - zNear);
		// z = (33 Z + 43) / (34 Z +44)
		//  z = (N/(F-N)*Z+F*N/(F-N))/(-Z)
		// if Z=N then z = (N/(F-N)*N+F*N/(F-N))/(-N)
		//               = (N*N+F*N)/(-N(F-N))
		//				= (F+N)/(N-F)
		// What if z = (33 Z + 34) /  (43 Z + 44)
		//  then   z= (n/(f-n)*Z-1)/(n*f/(f-n) Z)	= (1/f-(1/n-1/f)/Z)		=  1/f-(1/n-1/f)/Z
		// so if Z=n,	z = 1/f - (1/n - 1/f)/n
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
		h=2.f*atan(tan(v/2.0f)*aspect);
	if(aspect&&h&&!v)
		v=2.f*atan(tan(h/2.0f)/aspect);
	static float max_fov=160.0f*SIMUL_PI_F/180.0f;
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
	static math::Matrix4x4 m;
	memset(&m, 0, 16 * sizeof(float));
	m._11 = xScale;
	m._22 = yScale;

	m._33 = zFar / (zNear - zFar);		m._34 = -1.f;
	m._43 = zNear*zFar / (zNear - zFar);
	//	Zp		=m33 Zv + m43
	// Wp		=-Zv.
	// so     z = (33 Z + 43)/-Z = (F/(N-F)Z+FN/(N-F))/-Z     =     (FZ+FN)/(-Z(N-F))
	//
	return m;
}
const float *Camera::MakeProjectionMatrix(const FovPort &fovPort, float zNear, float zFar)
{
	float xScale = 2.0f / (fovPort.leftTan + fovPort.rightTan);
	float xOffset = (fovPort.leftTan - fovPort.rightTan) * xScale * 0.5f;
	float yScale = 2.0f / (fovPort.upTan + fovPort.downTan);
	float yOffset = (fovPort.upTan - fovPort.downTan) * yScale * 0.5f;

	static float handedness = 1.0f;// right-handed obviously.
	static math::Matrix4x4 m;
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
		h=2.0f*atan(tan(v/2.0f)*aspect);
	if(aspect&&h&&!v)
		v=2.0f*atan(tan(h/2.0f)/aspect);
	static float max_fov=160.0f*SIMUL_PI_F/180.0f;
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
	return (const float *)(&Orientation.GetMatrix());
}

const float *Camera::GetRotationAsQuaternion() const
{
	static simul::math::Quaternion q;
	simul::math::MatrixToQuaternion(q,Orientation.GetMatrix());
	return (const float *)(&q);
}

const float *Camera::GetPosition() const
{ 
	return (const float *)(&Orientation.GetMatrix().GetRowVector(3));
}

void Camera::SetOrientationAsMatrix(const float *m)
{
	Orientation.SetFromMatrix(m);
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

void Camera::CreateViewMatrix(float *mat,const float *view_dir, const float *view_up,const float *pos)
{
	Matrix4x4 &M=*((Matrix4x4*)mat);
	Vector3 d(view_dir);
	d.Normalize();
	Vector3 u(view_up);
	Vector3 x = d^u;
	x.Normalize();
	u = x^d;
	simul::geometry::SimulOrientation ori;
	ori.DefineFromYZ(u,-d);
	if(pos)
		ori.SetPosition(pos);
	ori.GetMatrix().Inverse(M);
}

void Camera::LookInDirection(const float *view_dir,const float *view_up)
{
	Vector3 d(view_dir);
	d.Normalize();
	Vector3 u(view_up);
	Vector3 x=d^u;
	x.Normalize();
	u=x^d;
	Orientation.DefineFromYZ(u,-d);
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

	del	=cam->GetOrientation().Tx()*y_rotate*(-1.f);
	dir	=del;
	dir.Normalize();
	cam->Rotate(del.Magnitude(),dir);

	float tilt	=0;
	tilt		=asin(cam->GetOrientation().Tx().z);
	dir			=cam->GetOrientation().Tz();
	dir.Normalize();
	cam->Rotate(-0.5f*tilt,dir);
}