#pragma once
#ifndef SIMUL_EDITOR
#define SIMUL_EDITOR 0
#endif
#if SIMUL_EDITOR
#pragma warning(push)
#pragma warning(disable:4251)
#include "Export.h"
#include "Platform/CrossPlatform/RenderPlatform.h"
#include "Platform/Shaders/SL/sphere_constants.sl"

namespace simul
{
	namespace crossplatform
	{
		class SIMUL_CROSSPLATFORM_EXPORT SphereRenderer
		{
			RenderPlatform *renderPlatform;
		public:
			SphereRenderer();
			virtual ~SphereRenderer();
			void RestoreDeviceObjects(RenderPlatform *r);
			void InvalidateDeviceObjects();
			void RecompileShaders();

			void DrawLatLongSphere	(DeviceContext &deviceContext,int lat,int longi,vec3 origin,float sph_radius,vec4 colour);
			void DrawQuad			(DeviceContext &deviceContext,vec3 origin,vec4 orient_quat,float size,float sph_radius,vec4, vec4 fill_colour = vec4(0.f, 0.f, 0.f, 0.f));
			void DrawTexture		(DeviceContext &deviceContext, Texture *t, vec3 origin,vec4 orient_quat,float qsize,float sph_rad,vec4 colour=vec4(1.f,1.f,1.f,1.f));
			void DrawCircle			(DeviceContext &deviceContext, vec3 origin, vec4 orient_quat,  float crc_rad,float sph_rad,vec4 line_colour,vec4 fill_colour = vec4(0.f, 0.f, 0.f, 0.f));
			void DrawCrossSection	(DeviceContext &deviceContext, Effect *effect,Texture *t, vec2 texcOffset, vec3 origin, vec4 orient_quat, float qsize, float sph_rad, vec4 colour);
			void DrawMultipleCrossSections	(DeviceContext& deviceContext, Effect* effect, Texture* t, vec2 texcOffset, vec3 origin, vec4 orient_quat, float qsize, float sph_rad, vec4 colour, int slices = 4);
			void DrawArc			(DeviceContext &deviceContext, vec3 origin, vec4 q1, vec4 q2, float sph_rad, vec4 colour);

		protected:
			crossplatform::ConstantBuffer<SphereConstants> sphereConstants;
			crossplatform::Effect  *effect=nullptr;
		};
	}
}
#pragma warning(pop)
#endif