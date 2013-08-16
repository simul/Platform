#ifndef GPU_SKY_CONSTANTS_SL
#define GPU_SKY_CONSTANTS_SL
#ifdef __cplusplus
#define R8
#define cbuffer struct
#define uniform
#endif

uniform_buffer GpuSkyConstants R8
{
	uniform vec2 texSize;
	uniform vec2 tableSize;
	
	uniform uint3 threadOffset;
	uniform float emissivity;

	uniform float distanceKm;
	uniform float g,h,ii;
	uniform float texelOffset;
	uniform float prevDistanceKm;

	uniform float maxOutputAltKm;
	uniform float planetRadiusKm;
	uniform float maxDensityAltKm;
	uniform float hazeBaseHeightKm;

	uniform float hazeScaleHeightKm;
	uniform float seaLevelTemperatureK;

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
	uniform float previousZCoord;
	uniform vec3 mieRayleighRatio;
	uniform float iii;
	uniform vec4 yRange;
};

#ifndef __cplusplus

#define pi (3.1415926536)

uint3 LinearThreadToPos2D(uint linear_pos,uint3 dims)
{
	uint Y				=linear_pos/dims.x;
	uint X				=linear_pos-Y*dims.x;
	uint3 pos			=uint3(X,Y,0);
	return pos;
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
	float sine				=clamp(DirectionToLight.z,-1.0,1.0);
	vec2 table_texc			=vec2(tableSize.x*(0.5+0.5*sine),tableSize.y*(alt_km/maxDensityAltKm));
	//table_texc			*=tableSize;//vec2(tableSize.x-1.0,tableSize.y-1.0);
	table_texc				+=vec2(texelOffset,texelOffset);
	table_texc				=vec2(table_texc.x/tableSize.x,table_texc.y/tableSize.y);
	//return vec4(table_texc,sine,1.0);
	vec4 lookup				=texture_clamp_lod(optical_depth_texture,table_texc,0);
	float illuminated_length=lookup.x;
	float vis				=lookup.y;
	float ozone_length		=lookup.w;
	float haze_opt_len		=getHazeOpticalLength(sine,alt_km);
	vec4 factor				=vec4(vis,vis,vis,vis);
	factor.rgb				*=exp(-rayleigh*illuminated_length-hazeMie*haze_opt_len-ozone*ozone_length);
	return factor;
}

float getShortestDistanceToAltitude(float sine_elevation,float start_h_km,float finish_h_km)
{
	float RH		=planetRadiusKm+finish_h_km;
	float Rh		=planetRadiusKm+start_h_km;
	float cosine	=-sine_elevation;
	float b			=-2.0*Rh*cosine;
	float c			=Rh*Rh-RH*RH;
	float b24c		=b*b-4*c;
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
	//float alt2				=max(alt1_km,alt2_km);
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

vec4 Insc(Texture2D input_texture,Texture3D loss_texture,Texture1D density_texture,vec2 texCoords)
{
	vec4 previous_insc	=texture_nearest_lod(input_texture,texCoords.xy,0);
	vec3 previous_loss	=texture_nearest_lod(loss_texture,vec3(texCoords.xy,pow(distanceKm/maxDistanceKm,0.5)),0).rgb;// should adjust texCoords - we want the PREVIOUS loss!
	float sin_e			=clamp(1.0-2.0*(texCoords.y*texSize.y-texelOffset)/(texSize.y-1.0),-1.0,1.0);
	float cos_e			=sqrt(1.0-sin_e*sin_e);
	float altTexc		=(texCoords.x*texSize.x-texelOffset)/(texSize.x-1.0);
	float viewAltKm		=altTexc*altTexc*maxOutputAltKm;
	float spaceDistKm	=getDistanceToSpace(sin_e,viewAltKm);
	float maxd			=min(spaceDistKm,distanceKm);
	float mind			=min(spaceDistKm,prevDistanceKm);
	float dist			=0.5*(mind+maxd);
	float stepLengthKm	=max(0.0,maxd-mind);
	float y				=planetRadiusKm+viewAltKm+dist*sin_e;
	float x				=dist*cos_e;
	float r				=sqrt(x*x+y*y);
	float alt_km		=r-planetRadiusKm;
	
	float x1			=mind*cos_e;
	float r1			=sqrt(x1*x1+y*y);
	float alt_1_km		=r1-planetRadiusKm;
	
	float x2			=maxd*cos_e;
	float r2			=sqrt(x2*x2+y*y);
	float alt_2_km		=r2-planetRadiusKm;
	
	// lookups is: dens_factor,ozone_factor,haze_factor;
	float dens_texc		=(alt_km/maxDensityAltKm*(tableSize.x-1.0)+texelOffset)/tableSize.x;
	vec4 lookups		=texture_clamp_lod(density_texture,dens_texc,0);
	float dens_factor	=lookups.x;
	float ozone_factor	=lookups.y;
	float haze_factor	=getHazeFactorAtAltitude(alt_km);
	vec4 light			=vec4(sunIrradiance,1.0)*getSunlightFactor(alt_km,lightDir);
	vec4 insc			=light;
	insc				*=1.0-getOvercastAtAltitudeRange(alt_1_km,alt_2_km);
	vec3 extinction		=dens_factor*rayleigh+haze_factor*hazeMie;
	vec3 total_ext		=extinction+ozone*ozone_factor;
	vec3 loss			=exp(-extinction*stepLengthKm);
	insc.rgb			*=vec3(1.0,1.0,1.0)-loss;
	float mie_factor	=exp(-insc.w*stepLengthKm*haze_factor*hazeMie.x);
	insc.w				=saturate((1.f-mie_factor)/(1.f-total_ext.x+0.0001f));
	
	insc.rgb			*=previous_loss.rgb;
	insc.rgb			+=previous_insc.rgb;
	float lossw=1.0;
	insc.w				=(lossw)*(1.0-previous_insc.w)*insc.w+previous_insc.w;

    return			insc;
}
#endif

#endif