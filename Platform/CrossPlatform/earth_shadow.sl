#ifndef EARTH_SHADOW_SL
#define EARTH_SHADOW_SL

//fade_texc is (sqrt(dist), .5(1+sine)).
// This function is to determine whether the given position is in sunlight or not.
vec3 EarthShadowLight(vec2 fade_texc,vec3 view,float transitionDistance)
{
	// Project to the world position this represents, then compare it to the shadow radius.
	// true distance normalized to fade max.
	float dist=fade_texc.x*fade_texc.x*maxFadeDistance;
	// Now resolve this distance on the normal to the sun direction.
	float along=dot(sunDir.xyz,view);
	float in_shadow		=saturate(100.0*(terminatorDistance-dist*along));
	vec3 on_cross_section=view-along*sunDir.xyz;
	on_cross_section*=dist;
	vec3 viewer_pos=vec3(0.0,0.0,radiusOnCylinder);
	vec3 target_pos=viewer_pos+on_cross_section;
	float target_radius=length(target_pos);

	// Now, if target_radius<1.0, it's zero.
	float result=clamp((target_radius-1.0+transitionDistance)/transitionDistance,0,1.0);
	result=1.0-in_shadow*(1.0-result);
	return vec3(result,result,result);
}

// The intention of the Earth shadow function is to find the value of the inscatter coefficient
// from the air between the viewer and a given point in space.

// This is done by finding the intersection of the vector between them with the Earth's shadow,
// which is a cylinder of radius equal to the Earth's radius, whose axis is parallel to the
// direction of sunlight.

// The inscatter is given only by the part of the ray that's in sunlight.
// So, if we're outside the shadow looking away from it, it's all lit.
//		if we're outside the shadow looking towards it, the part between the cylinder and the viewer is lit.
//		if we're inside the shadow looking into it, it's all dark.
//		if we're inside the shadow looking out, the part beyond the cylinder is lit.

vec2 EarthShadowDistances(vec2 fade_texc,vec3 view)
{
	float dist			=fade_texc.x*fade_texc.x;
	// The Earth's shadow: 
	// First get the part of view that is along the light direction
	float along			=dot(sunDir.xyz,view.xyz);
	// The Terminator is the line between light and shadow on the Earth's surface,
	// and marks the beginning of the shadow volume.

	// We define terminatorDistance as the distance to the terminator towards the sun, as a proportion of the fade distance.
	// So we're in shadow provided that d*along<terminatorDistance;
	float in_shadow		=saturate(1.0*(terminatorDistance-dist*along));
	// subtract it to get the direction on the shadow-cylinder cross section.
	vec3 on_cross_section=view-along*sunDir.xyz;
	// Now get the part that's on the cylinder radius:
	// Let shadowNormal be the direction normal to the sunlight direction
	//						but in the plane of the sunlight and the vertical.
	float on_radius	=dot(on_cross_section,earthShadowNormal.xyz);
	vec3 on_x		=on_cross_section-on_radius*earthShadowNormal.xyz;
	float sine_phi	=on_radius/length(on_cross_section);
	// We avoid positive phi because the cosine discards sign information leading to
	// confusion between negative and positive angles.
	float cos2		=1.0-sine_phi*sine_phi;
	float r			=radiusOnCylinder;//min(radiusOnCylinder,1.0);
	// Normalized so that Earth radius is 1.0..
	float u			=1.0-r*r*cos2;
	float d			=0.0;
	float L			=0.0;
	float sine_gamma=0.0;
	if(u>=0.0)
	{
		L			=-r*sine_phi;
		if(r<=1.0)
			L		+=sqrt(u);
		else
			L		-=sqrt(u);
		L			=max(0.0,L);
		sine_gamma	=length(on_cross_section);
		L			=L/sine_gamma;
		// L is the distance to the outside of the Earth's shadow in this direction, normalized to the Earth's radius.
		// Renormalize it to the fade distance.
		d			=L/maxFadeDistance;
	}
	float a			=sqrt(d);
	a				=min(a,fade_texc.x);
	// what should the light be at distance d?
	// We subtract the inscatter to d if we're looking OUT FROM the cylinder,
	if(r<=1.0||a==0.0)
	{
		return vec2(a*in_shadow,1.0);
    }
	else
	{
	// but we just use the inscatter to d if we're looking INTO the cylinder.
		return vec2(0.0,mix(1.0,a,in_shadow));
    }
}

vec4 EarthShadowFunction(vec2 fade_texc,vec3 view)
{
	float dist			=fade_texc.x*fade_texc.x;
	// The Earth's shadow: 
	// First get the part of view that is along the light direction
	float along			=dot(sunDir.xyz,view.xyz);
	// The Terminator is the line between light and shadow on the Earth's surface,
	// and marks the beginning of the shadow volume.

	// We define terminatorDistance as the distance to the terminator towards the sun, as a proportion of the fade distance.
	// So we're in shadow provided that d*along<terminatorDistance;
	float in_shadow		=saturate(1.0*(terminatorDistance-dist*along));
	// subtract it to get the direction on the shadow-cylinder cross section.
	vec3 on_cross_section=view-along*sunDir.xyz;
	// Now get the part that's on the cylinder radius:
	// Let shadowNormal be the direction normal to the sunlight direction
	//						but in the plane of the sunlight and the vertical.
	float on_radius	=dot(on_cross_section,earthShadowNormal.xyz);
	vec3 on_x		=on_cross_section-on_radius*earthShadowNormal.xyz;
	float sine_phi	=on_radius/length(on_cross_section);
	// We avoid positive phi because the cosine discards sign information leading to
	// confusion between negative and positive angles.
	float cos2		=1.0-sine_phi*sine_phi;
	float r			=radiusOnCylinder;//min(radiusOnCylinder,1.0);
	// Normalized so that Earth radius is 1.0..
	float u			=1.0-r*r*cos2;
	float d			=0.0;
	float L			=0.0;
	float sine_gamma=0.0;
	if(u>=0.0)
	{
		L			=-r*sine_phi;
		if(r<=1.0)
			L		+=sqrt(u);
		else
			L		-=sqrt(u);
		L			=max(0.0,L);
		sine_gamma	=length(on_cross_section);
		L			=L/sine_gamma;
		// L is the distance to the outside of the Earth's shadow in this direction, normalized to the Earth's radius.
		// Renormalize it to the fade distance.
		d			=L/maxFadeDistance;
	}
	float a			=sqrt(d);
	a				=min(a,fade_texc.x);
	// Inscatter at distance d
	vec2 fade_texc_d=vec2(a,fade_texc.y);
	vec4 insc		=texture_clamp(inscTexture,fade_texc);
	vec4 insc_d		=texture_clamp(inscTexture,fade_texc_d);
	// what should the light be at distance d?
	// We subtract the inscatter to d if we're looking OUT FROM the cylinder,
	if(r<=1.0||a==0.0)
	{
		insc-=insc_d*saturate(in_shadow);
    }
	else
	{
	// but we just use the inscatter to d if we're looking INTO the cylinder.
		insc=mix(insc,insc_d,in_shadow);
    }
	return insc;
}

#endif