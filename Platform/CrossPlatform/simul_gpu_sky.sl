#ifdef __cplusplus
#define R0
#define cbuffer struct
#define uniform
#endif

uniform_buffer GpuSkyConstants R2
{
	uniform vec2 texSize;
	uniform float texelOffset;
uniform float distanceKm;
	uniform vec2 tableSize;
uniform float prevDistanceKm;
	uniform float emissivity;

	uniform float maxOutputAltKm;
	uniform float planetRadiusKm;
	uniform float maxDensityAltKm;
	uniform float hazeBaseHeightKm;

	uniform vec3 ColourWavelengthsNm;
	uniform float hazeScaleHeightKm;

	uniform vec3 rayleigh;
	uniform float overcastBaseKm;
	uniform vec3 hazeMie;
	uniform float overcastRangeKm;
	uniform vec3 ozone;
	uniform float overcast;

	uniform vec3 sunIrradiance;
	uniform float maxDistanceKm;
	uniform vec3 lightDir;
	uniform float hazeEccentricity;

	uniform vec3 starlight;
	uniform float h;
	uniform vec3 mieRayleighRatio;
	uniform float i;
};

#ifndef __cplusplus

uniform float distKm;
uniform float prevDistKm;

#define pi (3.1415926536)

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
		haze_opt_len=2.0*sqrt(u);
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
	if(s<0.0)
		haze_opt_len+=n/s*(exp(-h1/n)-exp(-h2/n));
	// Now the outward path, in this case h2 -> infinity
	// and elevation is reversed.
	if(s<0.0)
		s*=-1.0;
	if(s<0.01)
		s=0.01;
	haze_opt_len+=n/s*(exp(-abs(h2)/n));
	return haze_opt_len;
}

vec4 getSunlightFactor(float alt_km,vec3 DirectionToLight)
{
	float sine=clamp(DirectionToLight.z,-1.0,1.0);
	vec2 table_texc=vec2(tableSize.x*(0.5+0.5*sine),tableSize.y*(alt_km/maxDensityAltKm));
	//table_texc*=tableSize;//vec2(tableSize.x-1.0,tableSize.y-1.0);
	table_texc+=vec2(texelOffset,texelOffset);
	table_texc=vec2(table_texc.x/tableSize.x,table_texc.y/tableSize.y);
	//return vec4(table_texc,sine,1.0);
	vec4 lookup=texture_clamp(optical_depth_texture,table_texc);
	float illuminated_length=lookup.x;
	float vis=lookup.y;
	float ozone_length=lookup.w;
	float haze_opt_len=getHazeOpticalLength(sine,alt_km);
	vec4 factor=vec4(vis,vis,vis,vis);
	factor.rgb*=exp(-rayleigh*illuminated_length-hazeMie*haze_opt_len-ozone*ozone_length);
	return factor;
}

float getShortestDistanceToAltitude(float sine_elevation,float start_h_km,float finish_h_km)
{
	float RH=planetRadiusKm+finish_h_km;
	float Rh=planetRadiusKm+start_h_km;
	float cosine=-sine_elevation;
	float b=-2.0*Rh*cosine;
	float c=Rh*Rh-RH*RH;
	float b24c=b*b-4*c;
	if(b24c<0)
		return -1.0;
	float dist;
	float s=sqrt(b24c);
	//if(b+s>=0.0)
		dist=0.5*(-b+s);
	//else
	//	dist=0.5*(-b-s);
	return dist;
}

float getDistanceToSpace(float sine_elevation,float h_km)
{
	return getShortestDistanceToAltitude(sine_elevation,h_km,maxDensityAltKm);
}

float getOvercastAtAltitude(float h_km)
{
	return overcast*saturate((overcastBaseKm+overcastRangeKm-h_km)/overcastRangeKm);
}

float getOvercastAtAltitudeRange(float alt1_km,float alt2_km)
{
	// So now alt1 is Definitely lower than alt2.
	float alt1				=min(alt1_km,alt2_km);
	float alt2				=max(alt1_km,alt2_km);
	//if(alt1==alt2)
		return getOvercastAtAltitude(alt1);
	/*float diff_km			=alt2-alt1;
	float const_start_km	=min(alt1,overcastBaseKm);
	float const_end_km		=min(alt2,overcastBaseKm);
	float x1				=min(max(alt1-overcastBaseKm,0.0),overcastRangeKm);
	float x2				=min(max(alt2-overcastBaseKm,0.0),overcastRangeKm);
	float oc1_km			=const_end_km-const_start_km;
	// In the varying part, we integrate o=1-x/overcastRangeKm wrt x
	float oc2_km			=(x2-x1)+(x1*x1-x2*x2)/(2.0*overcastRangeKm);
	float oc				=(oc1_km+oc2_km)/diff_km;
	oc						*=overcast;
	return 1.0*oc;*/
}

#endif
