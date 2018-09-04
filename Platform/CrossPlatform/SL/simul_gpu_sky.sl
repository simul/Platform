//  Copyright (c) 2015 Simul Software Ltd. All rights reserved.

#ifndef SIMUL_PI_F
#define SIMUL_PI_F (3.1415926536)
#endif

int3 LinearThreadToPos2D(int linear_pos,int3 dims)
{
	int yy				=int(float(linear_pos)/float(dims.x));
	int xx				=linear_pos-yy*int(dims.x);
	return int3(xx,yy,0);
}

float texcToAltKm(float texc,float minOutputAltKm,float maxOutputAltKm)
{
	return minOutputAltKm+texc*texc*(maxOutputAltKm-minOutputAltKm);
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
	//if(sine_elevation<0&&U>0.0)
	//	return 1000000.0;
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
		haze_opt_len+=n/s*(saturate(exp(-h1/n))-saturate(exp(-h2/n)));
	// Now the outward path, in this case h2 -> infinity
	// and elevation is reversed.
	if(s<0.0)
		s*=-1.0;
	if(s<0.01)
		s=0.01;
	haze_opt_len+=n/s*(exp(-abs(h2)/n));
	return haze_opt_len;
}

vec4 getSunlightFactor(Texture2D optical_depth_texture,float alt_km,vec3 DirectionToLight,float sunRadiusRadians=0.0)
{
	vec4 mean_factor		=vec4(0,0,0,0);
	for(int i=0;i<3;i++)
	{
		float offsetR			=float(i-1)*sunRadiusRadians;
		float sine				=clamp(DirectionToLight.z+offsetR,-1.0,1.0);
		vec2 table_texc			=vec2(tableSize.x*(0.5+0.5*sine),tableSize.y*(alt_km/maxDensityAltKm));

		table_texc				+=vec2(texelOffset,texelOffset);
		table_texc				=vec2(table_texc.x/tableSize.x,table_texc.y/tableSize.y);
	
		vec4 lookup				=texture_clamp_lod(optical_depth_texture,table_texc,0);
		float illuminated_length=lookup.x;
		float vis				=lookup.y;
		float ozone_length		=lookup.w;
		float haze_opt_len		=getHazeOpticalLength(sine,alt_km);
		vec4 factor				=vec4(vis,vis,vis,vis);
		factor.rgb				*=exp(-rayleigh*illuminated_length-hazeMie*haze_opt_len-ozone*ozone_length);
		mean_factor				+=factor;
	}
	return saturate(mean_factor/3.0);
}

float GetOpticalDepth(Texture2D density_texture,float max_altitude_km,float alt_km,vec3 dir)
{
	float total				=0;
	// how far to the edge of the atmosphere?
	// RH^2 = d^2 +Rh^2 - 2 d Rh cos(90+e)
	// d=1/2 (-b + _/b^2-4ac )
	float RH				=planetRadiusKm+atmosphereThicknessKm;
	float Rh				=planetRadiusKm+max(0.0,alt_km);
	float cosine			=-dir.z;
	float b					=-2*Rh*cosine;
	float c					=Rh*Rh-RH*RH;
	float U					=max(0.0,b*b-4.0*c);
	float distance_to_edge	=0.5*(-b+sqrt(U));
	int Steps				=16;
	float step_				=distance_to_edge/float(Steps);
	float d					=step_/2.0;

	for(int i=0;i<Steps;i++)
	{
		float Ra			=sqrt(Rh*Rh+d*d-2*Rh*d*cosine);
		float new_alt_km	=Ra-planetRadiusKm;
		float dens_here		=texture_clamp_lod(density_texture,vec2(new_alt_km/max_altitude_km,new_alt_km/max_altitude_km),0).x;
		total				+=dens_here*step_;
		d					+=step_;
	}
	return max(0.0,total);
}

// Using scattering
vec4 getSunlightFactor2(Texture2D optical_depth_texture,Texture2D density_texture,float max_altitude_km,float alt_km,vec3 lightDir)
{
	float sine				=clamp(lightDir.z,-1.0,1.0);
	float angle				=asin(sine);
	vec2 table_texc			=vec2(0.5+0.5*sine,alt_km/maxDensityAltKm);

	table_texc				+=vec2(texelOffset/tableSize.x,texelOffset/tableSize.y);
	
	vec4 lookup				=texture_clamp_lod(optical_depth_texture,table_texc,0);
	float opt_depth_km		=GetOpticalDepth(density_texture,max_altitude_km,alt_km,lightDir);
	float vis				=1;//lookup.y;
	float ozone_length		=lookup.w;
	float haze_opt_len		=getHazeOpticalLength(sine,alt_km);
	vec4 factor				=vec4(vis,vis,vis,vis);
	factor.rgb				*=exp(-rayleigh*opt_depth_km-hazeMie*haze_opt_len-ozone*ozone_length);

	return saturate(factor);
}

vec4 getSunlightFactor3(Texture2D optical_depth_texture,Texture2D density_texture,float max_altitude_km,float alt_km,vec3 lightDir,float planetRadiusKm,float radiusRadians)
{
	// what part of the circle is visible above the horizon?

	// We will integrate the light contribution over the sun's circle.
	if(lightDir.z>0.707)
		return getSunlightFactor2(optical_depth_texture,density_texture,max_altitude_km,alt_km,lightDir);
	vec3 side			=normalize(cross(lightDir,vec3(0,0,1.0)));
	float sine			=clamp(lightDir.z,-1.0,1.0);
	float elevation		=asin(sine);
	vec4 result			=vec4(0,0,0,0);
	for(int i=0;i<17;i++)
	{
		float new_elev	=elevation+float(i-8)/8.0*radiusRadians;
		vec3 new_dir	=vec3(lightDir.xy,sin(new_elev));
		new_dir.xy		=normalize(new_dir.xy);
		new_dir.xy		*=sqrt(1.0-(new_dir.z*new_dir.z));
		float vis		=1.0;
		float horizon_rads			=-acos(min(planetRadiusKm/(planetRadiusKm+alt_km),1.0));
		float above_horizon_rads	=(elevation-horizon_rads);
		if(above_horizon_rads<-radiusRadians)
			vis=0;
		else if(above_horizon_rads<radiusRadians)
		{
			float h=clamp(above_horizon_rads/radiusRadians,-1.0,1.0);
			// angle:
			float a=acos(abs(h));
			float c=(a-abs(h*sin(a)))/SIMUL_PI_F;
			if(h>=0)
				vis=1.0-c;
			else
				vis=c;
		}
		vec4 fac	=getSunlightFactor2(optical_depth_texture,density_texture,max_altitude_km,alt_km,new_dir)*vis;
		result		+=fac/17.0;
	}
	return result;
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

vec4 PSLoss(Texture2D input_loss_texture,Texture2D density_texture,vec2 texCoords)
{
	vec4 previous_loss	=texture_clamp(input_loss_texture,texCoords);
	float sin_e			=clamp(1.0-2.0*(texCoords.y*texSize.y-texelOffset)/(texSize.y-1.0),-1.0,1.0);
	float cos_e			=sqrt(1.0-sin_e*sin_e);
	float altTexc		=(texCoords.x*texSize.x-texelOffset)/max(texSize.x-1.0,1.0);
	float viewAltKm		=texcToAltKm(altTexc,minOutputAltKm,maxOutputAltKm);
	float spaceDistKm	=getDistanceToSpace(sin_e,viewAltKm);
	float maxd			=min(spaceDistKm,distanceKm);
	float mind			=min(spaceDistKm,prevDistanceKm);
	float dist			=0.5*(mind+maxd);
	float stepLengthKm	=max(0.0,maxd-mind);
	float y				=planetRadiusKm+viewAltKm+dist*sin_e;
	float x				=dist*cos_e;
	float r				=sqrt(x*x+y*y);
	float alt_km		=r-planetRadiusKm;
	// lookups is: dens_factor,ozone_factor,haze_factor;
	float dens_texc		=clamp((alt_km/maxDensityAltKm*(tableSize.x-1.0)+texelOffset)/tableSize.x,0.0,1.0);
	vec4 lookups		=texture_clamp(density_texture,vec2(dens_texc,dens_texc));
	float dens_factor	=lookups.x;
	float ozone_factor	=lookups.y;
	float haze_factor	=getHazeFactorAtAltitude(alt_km);
	vec3 extinction		=(dens_factor*rayleigh+haze_factor*hazeMie+ozone*ozone_factor);
	vec4 loss;
	loss.rgb			=exp(-extinction*stepLengthKm);
	loss.a				=(loss.r+loss.g+loss.b)/3.0;
	loss				*=previous_loss;
    return			loss;
}

vec4 Insc(Texture2D input_texture,Texture3D loss_texture,Texture2D density_texture,Texture2D optical_depth_texture,vec2 texCoords)
{
	vec4 previous_insc	=texture_nearest_lod(input_texture,texCoords.xy,0);
	vec3 u				=vec3(texCoords.xy,pow(distanceKm/maxDistanceKm,0.5));
	vec3 previous_loss	=texture_3d_nearest_lod(loss_texture,u,0).rgb;// should adjust texCoords - we want the PREVIOUS loss!
	float sin_e			=clamp(1.0-2.0*(texCoords.y*texSize.y-texelOffset)/(texSize.y-1.0),-1.0,1.0);
	float cos_e			=sqrt(1.0-sin_e*sin_e);
	float altTexc		=(texCoords.x*texSize.x-texelOffset)/(texSize.x-1.0);
	float viewAltKm		=texcToAltKm(altTexc,minOutputAltKm,maxOutputAltKm);
	float spaceDistKm	=getDistanceToSpace(sin_e,viewAltKm);
	float maxd			=min(spaceDistKm,distanceKm);
	float mind			=min(spaceDistKm,prevDistanceKm);
	float dist			=0.5*(mind+maxd);
	float stepLengthKm	=max(0.0,maxd-mind);
	float y				=planetRadiusKm+viewAltKm+dist*sin_e;
	float x				=dist*cos_e;
	float r				=sqrt(x*x+y*y);
	float alt_km		=r-planetRadiusKm;
	
	// lookups is: dens_factor,ozone_factor,haze_factor;
	float dens_texc		=(alt_km/maxDensityAltKm*(tableSize.x-1.0)+texelOffset)/tableSize.x;
	vec4 lookups		=texture_clamp_lod(density_texture,vec2(dens_texc,0.5),0);
	float dens_factor	=lookups.x;
	float ozone_factor	=lookups.y;
	float haze_factor	=getHazeFactorAtAltitude(alt_km);
	vec4 light			=vec4(sunIrradiance,1.0)*getSunlightFactor(optical_depth_texture,alt_km,lightDir);
	light.rgb			*=RAYLEIGH_BETA_FACTOR;
	vec4 insc			=light;
	//insc				*=1.0-getOvercastAtAltitudeRange(alt_1_km,alt_2_km);
	vec3 extinction		=dens_factor*rayleigh+haze_factor*hazeMie;
	vec3 total_ext		=extinction+ozone*ozone_factor;
	vec3 loss			=exp(-extinction*stepLengthKm);
	insc.rgb			*=vec3(1.0,1.0,1.0)-loss;
	float mie_factor	=exp(-insc.w*stepLengthKm*haze_factor*hazeMie.x);
	insc.rgb			*=previous_loss.rgb;
	insc.rgb			+=previous_insc.rgb;

	insc.w				=saturate((1.0-mie_factor)/(1.0-total_ext.x+0.0001));
	float lossw			=1.0;
	insc.w				=(lossw)*(1.0-previous_insc.w)*insc.w+previous_insc.w;

    return				insc;
}

// What spectral radiance is added on a light path towards the viewer, due to illumination of
// a volume of air by the surrounding sky?
// in cpp:
//	float cos0=dir_to_sun.z;
//	Skylight=GetAnisotropicInscatterFactor(true,hh,pif/2.f,0,1e5f,dir_to_sun,dir_to_moon,haze,false,1);
//	Skylight*=GetInscatterAngularMultiplier(cos0,Skylight.w,hh);

vec3 getSkylight(float alt_km, Texture3D insc_texture)
{
// The inscatter factor, at this altitude looking straight up, is given by:
	vec4 insc		=texture_3d_clamp_lod(insc_texture,vec3(sqrt(alt_km/maxOutputAltKm),0.0,1.0),0);
	vec3 skylight	=InscatterFunction(insc,hazeEccentricity,0.0,mieRayleighRatio);
	return skylight;
}

vec3 Blackbody(Texture2D blackbody_texture,float T_K)
{
    float tc    =saturate((T_K-100.0)/400.0);
    tc          =saturate(tc+texelOffset/tableSize.x);
	vec3 bb		=texture_clamp_lod(blackbody_texture,vec2(tc,tc),0).rgb;
    return bb;
}

vec4 Skyl(Texture3D insc_texture
		,Texture2D density_texture
		,Texture2D blackbody_texture
		,vec3 previous_loss
		,vec4 previous_skyl
		,float maxDensityAltKm
		,float spaceDistKm
		,float viewAltKm
		,float dist_km
		,float prevDist_km
		,float sin_e
		,float cos_e)
{
	float maxd			=min(spaceDistKm,dist_km);
	float mind			=min(spaceDistKm,prevDist_km);
	float dist			=0.5*(mind+maxd);
	float stepLengthKm	=max(0.0,maxd-mind);
	float y				=planetRadiusKm+viewAltKm+dist*sin_e;
	float x				=dist*cos_e;
	float r				=sqrt(x*x+y*y);
	float alt_km		=r-planetRadiusKm;
	// lookups is: dens_factor,ozone_factor,haze_factor;
	float dens_texc		=(alt_km/maxDensityAltKm*(tableSize.x-1.0)+texelOffset)/tableSize.x;
	vec4 lookups		=texture_nearest_lod(density_texture,vec2(dens_texc,0.5),0);
	float dens_factor	=lookups.x;
	float ozone_factor	=lookups.y;
	float haze_factor	=getHazeFactorAtAltitude(alt_km);
	vec4 light			=vec4(starlight+getSkylight(alt_km,insc_texture),0.0);
	vec4 skyl			=light;
	vec3 extinction		=dens_factor*rayleigh+haze_factor*hazeMie;
	vec3 total_ext		=extinction+ozone*ozone_factor;
	vec3 loss			=exp(-extinction*stepLengthKm);
	skyl.rgb			*=vec3(1.0,1.0,1.0)-loss;
	float mie_factor	=exp(-skyl.w*stepLengthKm*haze_factor*hazeMie.x);
	skyl.w				=saturate((1.0-mie_factor)/(1.0-total_ext.x+0.0001));
#if 1//def BLACKBODY
	float dens_dist		=dens_factor*stepLengthKm;
	float emis_ext		=exp(-emissivity*dens_dist);
	vec3 bb;
	float T				=seaLevelTemperatureK*lookups.w;
	bb.xyz				=Blackbody(blackbody_texture,T);

	skyl				*=emis_ext;
	bb					*=1.0-emis_ext;
	skyl.rgb			+=bb;
 #endif
	skyl.rgb			*=previous_loss.rgb;
	skyl.rgb			+=previous_skyl.rgb;
	float lossw			=1.0;
	skyl.w				=(lossw)*(1.0-previous_skyl.w)*skyl.w+previous_skyl.w;
	return skyl;
}
