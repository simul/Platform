#ifndef SIMUL_EARTHSHADOW
#define SIMUL_EARTHSHADOW

uniform_buffer EarthShadowUniforms R9
{
	vec3 sunDir;
	float radiusOnCylinder;
	vec3 earthShadowNormal;
	float maxFadeDistance;
	float terminatorCosine;
};

#ifndef __cplusplus
SamplerState inscSamplerState
{
	Filter = MIN_MAG_MIP_LINEAR;
	AddressU = Clamp;
	AddressV = Mirror;
};
static const float transitionDistance=0.001;

// This function is to determine whether the given position is in sunlight or not.
vec3 EarthShadowLight(vec2 texc2,vec3 view)
{
	// Project to the world position this represents, then compare it to the shadow radius.
	// true distance normalized to fade max.
	float d=texc2.x*texc2.x*maxFadeDistance;
	// Now resolve this distance on the normal to the sun direction.
	float along=dot(sunDir.xyz,view);
	float in_shadow=saturate(-along-terminatorCosine);
	vec3 on_cross_section=view-along*sunDir.xyz;
	on_cross_section*=d;
	vec3 viewer_pos=vec3(0.0,0.0,radiusOnCylinder);
	vec3 target_pos=viewer_pos+on_cross_section;
	float target_radius=length(target_pos);

	// Now, if target_radius<1.0, it's zero.
	float result=clamp((target_radius-1.0+transitionDistance)/transitionDistance,0,1.0);
	result=1.0-in_shadow*(1.0-result);
	return vec3(result,result,result);
}

float4 EarthShadowFunction(float2 texc2,float3 view)
{
	
	// The Earth's shadow: let shadowNormal be the direction normal to the sunlight direction
	//						but in the plane of the sunlight and the vertical.
	// First get the part of view that is along the light direction
	float along=dot(sunDir,view);
	float in_shadow=saturate(-along-terminatorCosine);
	// subtract it to get the direction on the shadow-cylinder cross section.
	float3 on_cross_section=view-along*sunDir;
	// Now get the part that's on the cylinder radius:
	float on_radius=dot(on_cross_section,earthShadowNormal);
	float3 on_x=on_cross_section-on_radius*earthShadowNormal;
	float sine_phi=on_radius/length(on_cross_section);
	// We avoid positive phi because the cosine discards sign information leading to
	// confusion between negative and positive angles.
	float cos2=1.0-sine_phi*sine_phi;
	float r=radiusOnCylinder;//min(radiusOnCylinder,1.0);
	// Normalized so that Earth radius is 1.0..
	float u=1.0-r*r*cos2;
	float d=0.0;
	float L=0.0;
	float sine_gamma=0.0;
	if(u>=0.0)
	{
		L=-r*sine_phi;
		if(r<=1.0)
			L+=sqrt(u);
		else
			L-=sqrt(u);
		L=max(0.0,L);
		sine_gamma=length(on_cross_section);
		L=L/sine_gamma;
		// L is the distance to the outside of the Earth's shadow in this direction, normalized to the Earth's radius.
		// Renormalize it to the fade distance.
		d=L/maxFadeDistance;
	}
	float a=sqrt(d);
	a=min(a,texc2.x);
	// Inscatter at distance d
	float2 texcoord_d=float2(a,texc2.y);
	float4 inscb=inscTexture.Sample(inscSamplerState,texcoord_d);
	// what should the light be at distance d?
	// We subtract the inscatter to d if we're looking OUT FROM the cylinder,
	// but we just use the inscatter to d if we're looking INTO the cylinder.
	float4 insc=inscTexture.Sample(inscSamplerState,texc2);
	if(r<=1.0||a==0.0)
	{
		insc-=inscb*saturate(in_shadow);
    }
	else
	{
	// but we just use the inscatter to d if we're looking INTO the cylinder.
		insc=lerp(insc,inscb,in_shadow);
    }
   return insc;
}
#endif
#endif