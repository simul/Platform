#ifndef EARTH_SHADOW_SL
#define EARTH_SHADOW_SL

//fade_texc is (sqrt(dist), .5(1+sine)).
// This function is to determine whether the given position is in sunlight or not.
vec3 EarthShadowLight(vec2 fade_texc,vec3 view,float transitionDistance)
{
	// Project to the world position this represents, then compare it to the shadow radius.
	// true distance normalized to fade max.
	float d=fade_texc.x*fade_texc.x*maxFadeDistance;
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

vec4 EarthShadowFunction(vec2 fade_texc,vec3 view)
{
	// The Earth's shadow: let shadowNormal be the direction normal to the sunlight direction
	//						but in the plane of the sunlight and the vertical.
	// First get the part of view that is along the light direction
	float along=dot(sunDir.xyz,view.xyz);
	float in_shadow=saturate(-along-terminatorCosine);
	// subtract it to get the direction on the shadow-cylinder cross section.
	vec3 on_cross_section=view-along*sunDir.xyz;
	// Now get the part that's on the cylinder radius:
	float on_radius=dot(on_cross_section,earthShadowNormal.xyz);
	vec3 on_x=on_cross_section-on_radius*earthShadowNormal.xyz;
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
	float a		=sqrt(d);
	a			=min(a,fade_texc.x);
	// Inscatter at distance d
	vec2 texcoord_d	=vec2(a,fade_texc.y);
	vec4 insc		=texture_clamp(inscTexture,fade_texc);
	vec4 inscb		=texture_clamp(inscTexture,texcoord_d);
/*	// what should the light be at distance d?
	// We subtract the inscatter to d if we're looking OUT FROM the cylinder,
	// but we just use the inscatter to d if we're looking INTO the cylinder.
	if(r<=1.0||a==0.0)
	{
		insc-=inscb*saturate(in_shadow);
    }
	else
	{
	// but we just use the inscatter to d if we're looking INTO the cylinder.
		insc=mix(insc,inscb,in_shadow);
    }*/
	insc*=along;
	return insc;
}

#endif