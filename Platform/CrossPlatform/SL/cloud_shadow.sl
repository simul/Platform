#ifndef CLOUDSHADOW_SL
#define CLOUDSHADOW_SL

float GetSimpleIlluminationAt(Texture2D cloudShadowTexture,mat4 invShadowMatrix,vec3 wpos)
{
	vec3 texc			=mul(invShadowMatrix,vec4(wpos,1.0)).xyz;
	vec4 texel			=texture_wrap_lod(cloudShadowTexture,texc.xy,0);
	float above			=saturate((texc.z-0.5)/0.5);
	texel.a				+=above;
	return saturate(texel.a);
}

float GetCloudIlluminationAt(Texture3D cloudTexture,mat4 invCloudMatrix,vec3 wpos)
{
	vec3 texc			=mul(invCloudMatrix,vec4(wpos,1.0)).xyz;
	vec4 texel			=texture_wwc_lod(cloudTexture,texc,0);
	float above			=saturate((texc.z-0.5)/0.5);
	texel.a				+=above;
	return saturate(texel.x);
}
#endif
