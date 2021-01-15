#include "AxesStandard.h"
using namespace simul;
using namespace crossplatform;
/*
void ConvertTransform(AxesStandard fromStandard, AxesStandard toStandard, Transform& transform)
{
	ConvertPosition(fromStandard, toStandard, transform.position);
	ConvertRotation(fromStandard, toStandard, transform.rotation);
	ConvertScale(fromStandard, toStandard, transform.scale);
}
*/
Quaternionf simul::crossplatform::ConvertRotation(AxesStandard fromStandard, AxesStandard toStandard, const Quaternionf& rotation)
{
	if (fromStandard == toStandard)
		return rotation;
	Quaternionf q;
	if ((fromStandard & (AxesStandard::LeftHanded)) != (toStandard & (AxesStandard::LeftHanded)))
	{
		//rotation = { -rotation.x,-rotation.y,-rotation.z,rotation.w };
	}
	if (fromStandard == AxesStandard::Unreal)
	{
		if (toStandard == AxesStandard::OpenGL)
		{
			q = Quaternionf( -rotation.y, -rotation.z, +rotation.x, rotation.s );
		}

		if (toStandard == AxesStandard::Engineering)
		{
			q = Quaternionf( -rotation.y, -rotation.x,-rotation.z, rotation.s );
		}
	}
	else if (fromStandard == AxesStandard::Engineering)
	{
		if (toStandard == AxesStandard::Unreal)
		{
			q = Quaternionf(-rotation.y, -rotation.x, -rotation.z, rotation.s );
		}
		else if (toStandard == AxesStandard::Unity)
		{
			q = Quaternionf(-rotation.x, -rotation.z, -rotation.y, rotation.s );
		}
	}
	else if (fromStandard == AxesStandard::OpenGL)
	{
		if (toStandard == AxesStandard::Unreal)
		{
			q = Quaternionf(+rotation.z, -rotation.x, -rotation.y, rotation.s );
		}
		else if (toStandard == AxesStandard::Unity)
		{
			q = Quaternionf(-rotation.x, -rotation.y, rotation.z, rotation.s );
		}
		else if (toStandard == AxesStandard::Engineering)
		{
			q = Quaternionf( rotation.x, -rotation.z, +rotation.y, rotation.s);
		}
	}
	else if (fromStandard == AxesStandard::Unity)
	{
		if (toStandard == AxesStandard::OpenGL)
		{
			q = Quaternionf(-rotation.x, -rotation.y, rotation.z, rotation.s );
		}

		if (toStandard == AxesStandard::Engineering)
		{
			q = Quaternionf(-rotation.x, -rotation.z, -rotation.y, rotation.s );
		}
	}
	return q;
}

int8_t simul::crossplatform::ConvertAxis(AxesStandard fromStandard, AxesStandard toStandard, int8_t axis)
{
	int8_t a = (axis) % 3;
	//int8_t sn=(axis>=3)?-1:1;
	static int8_t ue_e[] = { 1, 0, 2 };
	static int8_t gl_2[] = { 1, 2, 3 };

	static int8_t unt[] = { 0, 2, 1 };
	static int8_t ue_gl[] = { 1, 2, 3 };

	static int8_t uy_gl[] = { 0, 1, 5 };
	static int8_t uy_en[] = { 0, 2, 1 };
	if (fromStandard == toStandard)
	{
		return axis;
	}
	if (fromStandard == AxesStandard::Unreal)
	{
		if (toStandard == AxesStandard::OpenGL)
		{
			return ue_gl[axis];
		}
		if (toStandard == AxesStandard::Engineering)
		{
			return ue_e[axis];
		}
	}
	else if (fromStandard == AxesStandard::Unity)
	{
		if (toStandard == AxesStandard::OpenGL)
		{
			return uy_gl[axis];
		}
		if (toStandard == AxesStandard::Engineering)
		{
			return uy_en[axis];
		}
	}
	else if (fromStandard == AxesStandard::Engineering)
	{
		if (toStandard == AxesStandard::Unreal)
		{
			static int8_t en_ue[] = { 1, 0,2 };
			return en_ue[axis];
		}
		else if (toStandard == AxesStandard::Unity)
		{
			static int8_t en_uy[] = { 0, 2, 1 };
			return en_uy[axis];
		}
	}
	else if (fromStandard == AxesStandard::OpenGL)
	{
		if (toStandard == AxesStandard::Unreal)
		{
			static int8_t gl_ue[] = { 5, 0, 1 };
			return gl_ue[axis];
		}
		else if (toStandard == AxesStandard::Unity)
		{
			static int8_t gl_uy[] = { 0, 1, 5 };
			return gl_uy[axis];
		}
		else if (toStandard == AxesStandard::Engineering)
		{
			static int8_t gl_e[] = { 0, 5, 1 };
			return gl_e[axis];
		}
	}
	return -1;
}

vec3 simul::crossplatform::ConvertScale(AxesStandard fromStandard, AxesStandard toStandard, const vec3& scale)
{
	if (fromStandard == toStandard)
	{
		return scale;
	}
	vec3 s;
	if (fromStandard == AxesStandard::Unreal)
	{
		if (toStandard == AxesStandard::OpenGL)
		{
			s = { +scale.y, +scale.z, scale.x };
		}
		if (toStandard == AxesStandard::Engineering)
		{
			s = { scale.y, scale.x,scale.z };
		}
	}
	else if (fromStandard == AxesStandard::Unity)
	{
		if (toStandard == AxesStandard::OpenGL)
		{
			s = { scale.x, scale.y, scale.z };
		}
		if (toStandard == AxesStandard::Engineering)
		{
			s = { scale.x, scale.z, scale.y };
		}
	}
	else if (fromStandard == AxesStandard::Engineering)
	{
		if (toStandard == AxesStandard::Unreal)
		{
			s = { scale.y, scale.x,scale.z };
		}
		else if (toStandard == AxesStandard::Unity)
		{
			s = { scale.x, scale.z, scale.y };
		}
	}
	else if (fromStandard == AxesStandard::OpenGL)
	{
		if (toStandard == AxesStandard::Unreal)
		{
			s = { scale.z, +scale.x, +scale.y };
		}
		else if (toStandard == AxesStandard::Unity)
		{
			s = { scale.x, scale.y, scale.z };
		}
		else if (toStandard == AxesStandard::Engineering)
		{
			s = { scale.x, scale.z, scale.y };
		}
	}
	return s;
}
mat4 simul::crossplatform::ConvertMatrix(AxesStandard fromStandard, AxesStandard toStandard, const mat4& m)
{
	int8_t ax[3];
	ax[0]=simul::crossplatform::ConvertAxis( fromStandard, toStandard, 0);
	ax[1]=simul::crossplatform::ConvertAxis( fromStandard, toStandard, 1);
	ax[2]=simul::crossplatform::ConvertAxis( fromStandard, toStandard, 2);
	float s[3]={float((ax[0]<3)-(ax[0]>2)),float((ax[1] < 3) - (ax[1] > 2)),float((ax[2] < 3) - (ax[2] > 2))};
	ax[0]=ax[0]%3;
	ax[1]=ax[1]%3;
	ax[2]=ax[2]%3;
	mat4 n;
	n={  m.m[ax[0]*4+ax[0]]				,m.m[ax[0]*4+ax[1]]*s[0]*s[1]	,m.m[ax[0]*4+ax[2]]*s[0]*s[2]	,m.m[ax[0]*4+3] * s[0]
		,m.m[ax[1]*4+ax[0]]*s[1]*s[0]	,m.m[ax[1]*4+ax[1]]				,m.m[ax[1]*4+ax[2]]*s[1]*s[2]	,m.m[ax[1]*4+3] * s[1]
		,m.m[ax[2]*4+ax[0]]*s[2]*s[0]	,m.m[ax[2]*4+ax[1]]*s[2]*s[1]	,m.m[ax[2]*4+ax[2]]				,m.m[ax[2]*4+3] * s[2]
		,m.m[   3 *4+ax[0]]*s[0]		,m.m[   3 *4+ax[1]]*s[1]		,m.m[   3 *4+ax[2]]*s[2]		,m.m[   3 *4+3]};
	return n;
}

vec3 simul::crossplatform::ConvertPosition(AxesStandard fromStandard, AxesStandard toStandard, const vec3& position)
{
	if (fromStandard == toStandard)
	{
		return position;
	}
	vec3 p;
	if (fromStandard == AxesStandard::Unreal)
	{
		if (toStandard == AxesStandard::OpenGL)
		{
			p = { +position.y, +position.z, -position.x };
		}
		if (toStandard == AxesStandard::Engineering)
		{
			p = { position.y, position.x,position.z };
		}
	}
	else if (fromStandard == AxesStandard::Unity)
	{
		if (toStandard == AxesStandard::OpenGL)
		{
			p = { position.x, position.y, -position.z };
		}
		if (toStandard == AxesStandard::Engineering)
		{
			p = { position.x, position.z, position.y };
		}
	}
	else if (fromStandard == AxesStandard::Engineering)
	{
		if (toStandard == AxesStandard::Unreal)
		{
			p = { position.y, position.x,position.z };
		}
		else if (toStandard == AxesStandard::Unity)
		{
			p = { position.x, position.z, position.y };
		}
	}
	else if (fromStandard == AxesStandard::OpenGL)
	{
		if (toStandard == AxesStandard::Unreal)
		{
			p = { -position.z, +position.x, +position.y };
		}
		else if (toStandard == AxesStandard::Unity)
		{
			p = { position.x, position.y, -position.z };
		}
		else if (toStandard == AxesStandard::Engineering)
		{
			p = { +position.x, -position.z, +position.y };
		}
	}
	return p;
}
