#ifndef CPP_HLSL
#define CPP_HLSL

#define uniform
#define vec1 float1
#define vec2 float2
#define vec3 float3
#define vec4 float4
#define mat4 float4x4
#define uniform_buffer ALIGN cbuffer

#ifdef __cplusplus
	#define ALIGN __declspec( align( 16 ) )
	#define cbuffer struct
	#define sampler1D texture1D
	#define sampler2D texture2D
	#define sampler3D texture3D
	#define texture(tex,texc) tex.Sample(samplerState,texc)

	#define R0
	#define R1
	#define R2
	struct float4x4
	{
		float m[16];
		operator const float *()
		{
			return m;
		}
		void operator=(const float *v)
		{
			for(int i=0;i<16;i++)
				m[i]=v[i];
		}
	};
	typedef float FLOAT;
	struct float2
	{
		float x,y;
		float2(float X=0.f,float Y=0.f)
			:x(X),y(Y)
		{
		}
		void operator=(const float*f)
		{
			x=f[0];
			y=f[1];
		}
	};
	struct float3
	{
		float x,y,z;
		void operator=(const float*f)
		{
			x=f[0];
			y=f[1];
			z=f[2];
		}
	};
	struct float4
	{
		float x,y,z,w;
		void operator=(const float*f)
		{
			x=f[0];
			y=f[1];
			z=f[2];
			w=f[3];
		}
	};
#else
	#define R0 : register(b0)
	#define R1 : register(b1)
	#define R2 : register(b2)
	#define ALIGN
	#define FLOAT float
#endif

#endif