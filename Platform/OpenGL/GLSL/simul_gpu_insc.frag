#version 140

uniform sampler2D input_insc_texture;
uniform sampler1D density_texture;
uniform sampler3D loss_texture;
uniform sampler2D optical_depth_texture;

uniform float distKm;
uniform float stepLengthKm;
uniform float maxDistanceKm;
uniform float planetRadiusKm;
uniform float maxAltKm;
uniform float maxDensityAltKm;
uniform float haze;
uniform float hazeBaseHeightKm;
uniform float hazeScaleHeightKm;
uniform vec3 rayleigh;
uniform vec3 hazeMie;
uniform vec3 ozone;
uniform vec3 sunIrradiance;
uniform vec3 lightDir;

varying vec2 texc;
float saturate(float x)
{
	return clamp(x,0.0,1.0);
}
float getHazeFactorAtAltitude(float alt_km)
{
	if(alt_km<hazeBaseHeightKm)
		alt_km=hazeBaseHeightKm;
	float val=exp((hazeBaseHeightKm-alt_km)/hazeScaleHeightKm);
	return val;
}
float getHazeOpticalLength(float sine_elevation,float h_km)
{
	float R=planetRadiusKm;
	float Rh=R+h_km;
	float RH=R+hazeBaseHeightKm;
	float c=sqrt(1.0-sine_elevation*sine_elevation);
	float u=RH*RH-Rh*Rh*c*c;
	float U=R*R-Rh*Rh*c*c;
	// If the ray passes through the earth, infinite optical length.
	if(sine_elevation<0&&U>0.0)
		return 1000000.0;
	float haze_opt_len=0.0;
	// If we have a solution, there exists a path through the constant haze area.
	if(sine_elevation<0&&u>0)
	{
		haze_opt_len=2.0*sqrt(u);
	}
	// Next, the inward path, if that exists.
	float Rmin=Rh*c;
	if(sine_elevation>0.0)
		Rmin=Rh;
	// But if the inward path goes into the constant area, include only the part outside that area.
	if(sine_elevation<0.0&&u>0.0)
		Rmin=RH;
		
	float h1=Rh-RH;
	float h2=Rmin-RH;
	float s=sine_elevation;
	float n=hazeScaleHeightKm;
	// s<0, and h2<h1
	if(s<0)
		haze_opt_len+=n/s*(exp(-h1/n)-exp(-h2/n));
	
	// Now the outward path, in this case h2 -> infinity
	// and elevation is reversed.
	if(s<0)
		s*=-1.0;
	if(s<0.01)
		s=0.01;
	haze_opt_len+=n/s*(exp(-abs(h2)/n));
	haze_opt_len*=haze;
	return haze_opt_len;
}


vec4 getSunlightFactor(float alt_km,vec3 DirectionToLight)
{
	float sine=DirectionToLight.z;
	vec4 lookup=texture(optical_depth_texture,vec2(0.5+0.5*sine,alt_km/maxDensityAltKm));
	float illuminated_length=lookup.x;
	float vis=lookup.y;
	float ozone_length=lookup.w;
	float haze_opt_len=getHazeOpticalLength(sine,alt_km);
	vec4 output=vec4(1.0,1.0,1.0,0);
	output.rgb*=exp(-rayleigh*illuminated_length-hazeMie*haze_opt_len-ozone*ozone_length);
	output.w=vis;
	return output;
}

void main(void)
{
	vec4 previous_insc	=texture(input_insc_texture,texc.xy);
	vec3 previous_loss	=texture(loss_texture,vec3(texc.xy,distKm/maxDistanceKm)).rgb;// should adjust texc - we want the PREVIOUS loss!
	vec4 mie_factor		=vec4(1.0,1.0,1.0,1.0);
	vec4 final			=previous_insc;
	float sin_e			=1.0-2.0*texc.y;
	float cos_e			=sqrt(1.0-sin_e*sin_e);
	float viewAltKm		=texc.x*texc.x*maxAltKm;
	
	float y				=planetRadiusKm+viewAltKm+distKm*sin_e;
	float x				=distKm*cos_e;
	float r				=sqrt(x*x+y*y);
	float alt_km		=r-planetRadiusKm;
	// lookups is: dens_factor,ozone_factor,haze_factor;
	vec4 lookups		=texture(density_texture,alt_km/maxDensityAltKm);
	float dens_factor	=lookups.x;
	float ozone_factor	=lookups.y;
	float haze_factor	=getHazeFactorAtAltitude(alt_km);

	vec4 light			=vec4(sunIrradiance,1.0)*getSunlightFactor(alt_km,lightDir);
	mie_factor			*=exp(-light.w*stepLengthKm*haze_factor*hazeMie);
	
	float dens_dist		=dens_factor*stepLengthKm;
	vec3 extinction		=dens_factor*rayleigh+haze_factor*hazeMie;
	vec3 ozone_ext		=ozone*ozone_factor;
	vec3 ext			=exp(extinction*(-stepLengthKm));
	vec3 total_ext		=extinction+ozone_ext;
	vec3 ext2			=exp(total_ext*(-stepLengthKm));
	final.rgb			*=previous_loss;
	light.rgb			*=vec3(1.0,1.0,1.0)-ext;
	final.rgb			+=light.rgb;
	final.w				=saturate((1.f-mie_factor.x)/(1.f-total_ext.x+0.0001f));
    gl_FragColor		=final;
}
