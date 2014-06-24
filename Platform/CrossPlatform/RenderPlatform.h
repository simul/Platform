#ifndef SIMUL_PLATFORM_CROSSPLATFORM_RENDERPLATFORM_H
#define SIMUL_PLATFORM_CROSSPLATFORM_RENDERPLATFORM_H
#include <set>
#include <map>
#include <string>
#include "Export.h"
#include "Simul/Base/PropertyMacros.h"
#include "Simul/Platform/CrossPlatform/BaseRenderer.h"
#include "Simul/Platform/CrossPlatform/PixelFormat.h"

#ifdef _MSC_VER
#pragma warning(disable:4251)
#endif
struct ID3D11Device;
namespace simul
{
	namespace crossplatform
	{
		class Material;
		class Effect;
		class EffectTechnique;
		class Light;
		class Texture;
		class Mesh;
		class PlatformConstantBuffer;
		class Buffer;
		class Layout;
		struct DeviceContext;
		struct LayoutDesc;
		/// Base class for API-specific rendering.
		/// Be sure to make the following calls at the appropriate place: RestoreDeviceObjects(), InvalidateDeviceObjects(), RecompileShaders(), SetReverseDepth()
		class RenderPlatform
		{
		public:
			/// To be called when the device has initialized or changed.

			/// Restore device objects.
			///
			/// \param [in,out]	platform_device	If non-null, the platform-specific device pointer.
			virtual void RestoreDeviceObjects(void * platform_device)=0;
			/// To be called when the device will be lost or destroyed.
			virtual void InvalidateDeviceObjects()=0;
			/// To be called if shader source code has changed.
			virtual void RecompileShaders()=0;
			/// A simple vertex type for debug displays.
			struct Vertext
			{
				vec3 pos;
				vec4 colour;
			};

			/// Converts this object to a ID3D11Device pointer.
			///
			/// \return	NULL if it fails, else an ID3D11Device*.
			virtual ID3D11Device *AsD3D11Device()=0;

			/// Pushes a path to the texture paths stack.
			///
			/// \param	pathUtf8	The path as a UTF8 string.
			virtual void PushTexturePath	(const char *pathUtf8)=0;
			/// Pops the texture path.
			virtual void PopTexturePath		()=0;
			/// Setup the device to start object rendering.
			virtual void StartRender		()=0;
			/// Mark the end of object rendering.
			virtual void EndRender			()=0;

			/// Sets reverse depth.
			///
			/// \param	rev	true if the depth values are 0 at the far plane.
			virtual void SetReverseDepth	(bool rev)=0;

			/// Intialize lighting environment.
			///
			/// \param	pAmbientLight	The ambient light.
			virtual void IntializeLightingEnvironment(const float pAmbientLight[3])		=0;

			/// Dispatch the current compute shader.
			///
			/// \param [in,out]	deviceContext	cross-platform device context.
			/// \param	w					 	The width of the compute block.
			/// \param	l					 	The length of the compute block.
			/// \param	d					 	The depth of the compute block.
			virtual void DispatchCompute	(DeviceContext &deviceContext,int w,int l,int d)=0;

			/// Applies the shader pass.
			///
			/// \param [in,out]	deviceContext	cross-platform device context.
			/// \param [in,out]	parameter2   	The effect.
			/// \param [in,out]	parameter3   	The technique.
			/// \param	pass			 		The pass index.
			virtual void ApplyShaderPass	(DeviceContext &deviceContext,Effect *,EffectTechnique *,int pass)=0;

			/// Draws.
			///
			/// \param [in,out]	deviceContext	cross-platform device context.
			/// \param	num_verts			 	Number of vertices.
			/// \param	start_vert			 	The start vertex.
			virtual void Draw				(DeviceContext &deviceContext,int num_verts,int start_vert)=0;

			/// Draw a marker.
			///
			/// \param [in,out]	context	The platform-specific device context.
			/// \param	matrix		   	The orientation/position matrix.
			virtual void DrawMarker			(void *context,const double *matrix)			=0;

			/// Draw line.
			///
			/// \param [in,out]	context	   	The platform-specific device context.
			/// \param	pGlobalBasePosition	The global base position.
			/// \param	pGlobalEndPosition 	The global end position.
			/// \param	colour			   	The colour.
			/// \param	width			   	The width.
			virtual void DrawLine			(void *context,const double *pGlobalBasePosition, const double *pGlobalEndPosition,const float *colour,float width)=0;

			/// Draw cross hair.
			///
			/// \param [in,out]	context	The platform-specific device context.
			/// \param	pGlobalPosition	The global position.
			virtual void DrawCrossHair		(void *context,const double *pGlobalPosition)	=0;

			/// Draw camera.
			///
			/// \param [in,out]	context	The platform-specific device context.
			/// \param	pGlobalPosition	The global position matrix.
			/// \param	pRoll		   	The roll.
			virtual void DrawCamera			(void *context,const double *pGlobalPosition, double pRoll)=0;

			/// Draw a line loop.
			///
			/// \param [in,out]	context	The platform-specific device context.
			/// \param	mat			   	The matrix.
			/// \param	num			   	Number of.
			/// \param	vertexArray	   	Array of vertices.
			/// \param	colr		   	The colr.
			virtual void DrawLineLoop		(void *context,const double *mat,int num,const double *vertexArray,const float colr[4])=0;

			/// Draw texture.
			///
			/// \param [in,out]	deviceContext	cross-platform device context.
			/// \param	x1					 	The left x value in pixels.
			/// \param	y1					 	The top y value in pixels.
			/// \param	dx					 	The width of the rectangle.
			/// \param	dy					 	The height of the rectangle.
			/// \param [in,out]	tex			 	The texture.
			/// \param	mult				 	(Optional) the multiplier.
			virtual void DrawTexture		(DeviceContext &deviceContext,int x1,int y1,int dx,int dy,crossplatform::Texture *tex,float mult=1.f)=0;

			/// Draw depth.
			///
			/// \param [in,out]	deviceContext	cross-platform device context.
			/// \param	x1					 	The left x value in pixels.
			/// \param	y1					 	The top y value in pixels.
			/// \param	dx					 	The width of the rectangle.
			/// \param	dy					 	The height of the rectangle.
			/// \param [in,out]	tex			 	If non-null, the tex.
			virtual void DrawDepth			(DeviceContext &deviceContext,int x1,int y1,int dx,int dy,crossplatform::Texture *tex)=0;

			/// Draw an onscreen quad without passing vertex positions, but using the "rect" constant
			/// from the shader to pass the position and extent of the quad.
			///
			/// \param [in,out]	deviceContext	cross-platform device context.
			/// \param	x1					 	The first x value.
			/// \param	y1					 	The first y value.
			/// \param	dx					 	The dx.
			/// \param	dy					 	The dy.
			/// \param [in,out]	effect		 	If non-null, the effect.
			/// \param [in,out]	technique	 	If non-null, the technique.
			virtual void DrawQuad			(DeviceContext &deviceContext,int x1,int y1,int dx,int dy,crossplatform::Effect *effect,crossplatform::EffectTechnique *technique)=0;

			/// Draw quad.
			///
			/// \param [in,out]	deviceContext	cross-platform device context.
			virtual void DrawQuad			(DeviceContext &deviceContext)=0;

			/// Prints.
			///
			/// \param [in,out]	deviceContext	cross-platform device context.
			/// \param	x					 	The x coordinate.
			/// \param	y					 	The y coordinate.
			/// \param	text				 	The text.
			virtual void Print				(DeviceContext &deviceContext,int x,int y,const char *text)							=0;

			/// Draw lines.
			///
			/// \param [in,out]	deviceContext	cross-platform device context.
			/// \param [in,out]	lines		 	If non-null, the lines.
			/// \param	count				 	Number of.
			/// \param	strip				 	(Optional) true to strip.
			virtual void DrawLines			(DeviceContext &deviceContext,Vertext *lines,int count,bool strip=false)		=0;

			/// Draw 2d lines.
			///
			/// \param [in,out]	deviceContext	cross-platform device context.
			/// \param [in,out]	lines		 	If non-null, the lines.
			/// \param	vertex_count		 	Number of vertices.
			/// \param	strip				 	true to strip.
			virtual void Draw2dLines		(DeviceContext &deviceContext,Vertext *lines,int vertex_count,bool strip)		=0;

			/// Draw circle.
			///
			/// \param [in,out]	deviceContext	cross-platform device context.
			/// \param	dir					 	The dir.
			/// \param	rads				 	The radians.
			/// \param	colr				 	The colr.
			/// \param	fill				 	(Optional) true to fill.
			virtual void DrawCircle			(DeviceContext &deviceContext,const float *dir,float rads,const float *colr,bool fill=false)		=0;

			/// Print at 3D position.
			///
			/// \param [in,out]	context	The platform-specific device context.
			/// \param	p			   	The const float * to process.
			/// \param	text		   	The text.
			/// \param	colr		   	The colr.
			/// \param	offsetx		   	(Optional) the offsetx.
			/// \param	offsety		   	(Optional) the offsety.
			virtual void PrintAt3dPos		(void *context,const float *p,const char *text,const float* colr,int offsetx=0,int offsety=0)		=0;

			/// Sets model matrix.
			///
			/// \param [in,out]	deviceContext	cross-platform device context.
			/// \param	mat					 	The matrix.
			virtual void					SetModelMatrix					(crossplatform::DeviceContext &deviceContext,const double *mat)	=0;
			/// Applies the default material.
			virtual void					ApplyDefaultMaterial			()	=0;

			/// Creates the material.
			///
			/// \return	null if it fails, else the new material.
			virtual Material				*CreateMaterial					()	=0;

			/// Creates the mesh.
			///
			/// \return	null if it fails, else the new mesh.
			virtual Mesh					*CreateMesh						()	=0;

			/// Creates the light.
			///
			/// \return	null if it fails, else the new light.
			virtual Light					*CreateLight					()	=0;

			/// Creates a texture.
			///
			/// \param	lFileNameUtf8	(Optional) the file name UTF 8.
			///
			/// \return	null if it fails, else the new texture.
			virtual Texture					*CreateTexture					(const char *lFileNameUtf8=NULL)	=0;

			/// Creates an effect.
			///
			/// \param	filename_utf8	The filename UTF 8.
			/// \param	defines		 	The defines.
			///
			/// \return	null if it fails, else the new effect.
			virtual Effect					*CreateEffect					(const char *filename_utf8,const std::map<std::string,std::string> &defines)=0;

			/// Creates platform constant buffer.
			///
			/// \return	null if it fails, else the new platform constant buffer.
			virtual PlatformConstantBuffer	*CreatePlatformConstantBuffer	()	=0;

			/// Creates the buffer.
			///
			/// \return	null if it fails, else the new buffer.
			virtual Buffer					*CreateBuffer					()	=0;

			/// Creates a layout.
			///
			/// \param	num_elements	  	Number of elements.
			/// \param [in,out]	parameter2	If non-null, the second parameter.
			/// \param [in,out]	parameter3	If non-null, the third parameter.
			///
			/// \return	null if it fails, else the new layout.
			virtual Layout					*CreateLayout					(int num_elements,LayoutDesc *,Buffer *)	=0;

			/// Gets the device.
			///
			/// \return	null if it fails, else the device.
			virtual void					*GetDevice						()	=0;

			/// Sets vertex buffers.
			///
			/// \param [in,out]	deviceContext	cross-platform device context.
			/// \param	slot				 	The slot.
			/// \param	num_buffers			 	Number of buffers.
			/// \param [in,out]	buffers		 	If non-null, the buffers.
			virtual void					SetVertexBuffers				(DeviceContext &deviceContext,int slot,int num_buffers,Buffer **buffers)=0;
		};
	}
}
#endif