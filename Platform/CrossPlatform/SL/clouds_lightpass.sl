
FarNearPixelOutput Lightpass(Texture3D cloudDensity
								,Texture3D noiseTexture3D
								,vec4 dlookup
								,vec3 view
								,vec4 clip_pos
								,vec3 sourcePosKm_w
								,float source_radius
								,vec3 spectralFluxOver1e6
								,float minCosine
								,float maxIlluminatedRadius
								,float threshold)
{
	FarNearPixelOutput res;
	res.farColour			=vec4(0,0,0,1.0);
	res.nearColour			=vec4(0,0,0,1.0);
	float max_spectral_flux	=max(max(spectralFluxOver1e6.r,spectralFluxOver1e6.g),spectralFluxOver1e6.b);

	vec3 dir_to_source		=normalize(sourcePosKm_w-viewPosKm);
	float cos0				=dot(dir_to_source.xyz,view.xyz);
	if (cos0 < minCosine)
		return res;
	float sine				=view.z;
	float min_z				=cornerPosKm.z-(fractalScale.z*1.5)/inverseScalesKm.z;
	float max_z				=cornerPosKm.z+(1.0+fractalScale.z*1.5)/inverseScalesKm.z;
	if(view.z<-0.1&&viewPosKm.z<cornerPosKm.z-fractalScale.z/inverseScalesKm.z)
		return res;
	
	vec2 solidDist_nearFar	=(dlookup.yx);
	float meanFadeDistance	=1.0;

	vec3 world_pos					=viewPosKm;

	// This provides the range of texcoords that is lit.
	int3 c_offset					=int3(sign(view.x),sign(view.y),sign(view.z));
	int3 start_c_offset				=-c_offset;
	start_c_offset					=int3(max(start_c_offset.x,0),max(start_c_offset.y,0),max(start_c_offset.z,0));
	vec3 viewScaled					=view/scaleOfGridCoordsKm;
	viewScaled						=normalize(viewScaled);

	vec3 offset_vec	=vec3(0,0,0);
	if(world_pos.z<min_z)
	{
		float a		=1.0/(view.z+0.00001);
		offset_vec	=(min_z-world_pos.z)*vec3(view.x*a,view.y*a,1.0);
	}
	if(view.z<0&&world_pos.z>max_z)
	{
		float a		=1.0/(-view.z+0.00001);
		offset_vec	=(world_pos.z-max_z)*vec3(view.x*a,view.y*a,-1.0);
	}
	world_pos						+=offset_vec;
	float viewScale					=length(viewScaled*scaleOfGridCoordsKm);
	// origin of the grid - at all levels of detail, there will be a slice through this in 3 axes.
	vec3 startOffsetFromOrigin		=viewPosKm-gridOriginPosKm;
	vec3 offsetFromOrigin			=world_pos-gridOriginPosKm;
	vec3 p0							=startOffsetFromOrigin/scaleOfGridCoordsKm;
	int3 c0							=int3(floor(p0) + start_c_offset);
	vec3 gridScale					=scaleOfGridCoordsKm;
	vec3 P0							=startOffsetFromOrigin/scaleOfGridCoordsKm/2.0;
	int3 C0							=c0>>1;
	
	float distanceKm				=length(offset_vec);
	int3 c							=c0;

	int idx 						=0;
	float W							=halfClipSize;
	const float start				=0.866*0.866;//0.707 for 2D, 0.866 for 3D;
	const float ends				=1.0;
	const float range				=ends-start;


	vec4 colour						=vec4(0.0,0.0,0.0,1.0);
	vec4 nearColour					=vec4(0.0,0.0,0.0,1.0);
	float lastFadeDistance			=0.0;
	int3 b							=abs(c-C0*2);
	for(int j=0;j<8;j++)
	{
		if(max(max(b.x,b.y),0)>=W)
		{
			// We want to round c and C0 downwards. That means that 3/2 should go to 1, but that -3/2 should go to -2.
			// Just dividing by 2 gives 3/2 -> 1, and -3/2 -> -1.
			c0			=	C0;
			c			+=	start_c_offset;
			c			-=	abs(c&int3(1,1,1));
			c			=	c>>1;
			gridScale	*=	2.0;
			viewScale	*=	2.0;
			if(idx==0)
				W*=2;
			p0			=	P0;
			P0			=	startOffsetFromOrigin/gridScale/2.0;
			C0			+=	start_c_offset;
			C0			-=	abs(C0&int3(1,1,1));
			C0			=	C0>>1;
			idx			++;
			b						=abs(c-C0*2);
		}
		else break;
	}
	for(int i=0;i<256;i++)
	{
		world_pos					+=0.001*view;
		if((view.z<0&&world_pos.z<min_z)||(view.z>0&&world_pos.z>max_z)||distanceKm>maxCloudDistanceKm)
			break;
		offsetFromOrigin			=world_pos-gridOriginPosKm;

		// Next pos.
		int3 c1						=c+c_offset;

		//find the correct d:
		vec3 p						=offsetFromOrigin/gridScale;
		vec3 p1						=c1;
		vec3 dp						=p1-p;
		vec3 D						=(dp/viewScaled);
		
		float e						=min(min(D.x,D.y),D.z);
		// All D components are positive. Only the smallest is equal to e. Step(x,y) returns (y>=x). 
		// So step(D.x,e) returns (e>=D.x), which is only true if e==D.x
		vec3 N						=step(D,vec3(e,e,e));

		int3 c_step					=c_offset*int3(N);
		float d						=e*viewScale;
		distanceKm					+=d;

		// What offset was the original position from the centre of the cube?
		p1							=p+e*viewScaled;
	
		// We fade out the intermediate steps as we approach the boundary of a detail change.
		// Now sample at the end point:
		world_pos					+=d*view;
		vec3 cloudWorldOffsetKm		=world_pos-cornerPosKm;
		vec3 cloudTexCoords			=(cloudWorldOffsetKm)*inverseScalesKm;
		c							+=c_step;
		int3 intermediate			=abs(c&int3(1,1,1));
		float is_inter				=dot(N,vec3(intermediate));
		// A spherical shell, whose outer radius is W, and, wholly containing the inner box, the inner radius must be sqrt(3 (W/2)^2).
		// i.e. from 0.5*(3)^0.5 to 1, from sqrt(3/16) to 0.5, from 0.433 to 0.5
		vec3 pw						=abs(p1-p0);
		float fade_inter			=saturate((length(pw.xy)/(float(W)*(2.0-is_inter)-1.0)-start)/range);// /(2.0-is_inter)
	
		float fade					=1.0-fade_inter;
		float fadeDistance			=saturate(distanceKm/maxFadeDistanceKm);

		b							=abs(c-C0*2);
		if(fade>0)
		{
			const float mip =0.0;
			vec3 noise_texc	=vec3(0.0,0.0,0.0);
			vec4 noiseval	=vec4(0,0,0,0);
			vec4 density 	=vec4(0.0,0.0,0.0,0.0);
			vec4 light   	=vec4(0.0,0.0,0.0,0.0);
			
			calcDensity(cloudDensity,cloudDensity,cloudTexCoords,noiseval,fractalScale,mip,density,light);
			
			density.z   	*=fade;
			if(density.z>0)
			{
				float brightness_factor =0.0;
				float cloud_density		=density.z;
				float cosine			=dot(N,abs(view));
				density.z				*=cosine;
				density.z				*=cosine;
				density.z				*=saturate(distanceKm/0.24);
			
				vec3 dist				=world_pos-sourcePosKm_w;
				float radius_km			=max(source_radius,length(dist));
				noise_texc 				=world_pos.xyz*noise3DTexcoordScale/1.3+noise3DTexcoordOffset;
				noiseval				=texture_3d_wrap_lod(noiseTexture3D,noise_texc,3.0*fadeDistance);
				float radiance			=(0.3+noiseval.x)*(1.0-density.z)*100.0/(4.0*3.14159*radius_km*radius_km);
				vec4 clr				=vec4(spectralFluxOver1e6.rgb*radiance*density.z,density.z);
				brightness_factor		=max(1.0,radiance*max_spectral_flux);
				{
					vec4 clr_n=clr;
					clr.a				*=saturate((solidDist_nearFar.y-fadeDistance)/0.01);
					clr_n.a				*=saturate((solidDist_nearFar.x-fadeDistance)/0.01);
					nearColour.rgb		+=clr_n.rgb*clr_n.a*(nearColour.a);
					nearColour.a		*=(1.0-clr_n.a);
				}
				colour.rgb				+=clr.rgb*clr.a*(colour.a);
				meanFadeDistance		=lerp(meanFadeDistance,fadeDistance,colour.a*cloud_density);
				colour.a				*=(1.0-clr.a);
				if(nearColour.a*brightness_factor<0.003)
				{
					colour.a			=0.0;
					break;
				}
			}
		}
		lastFadeDistance=fadeDistance;
		if(max(max(b.x,b.y),0)>=W)
		{
			// We want to round c and C0 downwards. That means that 3/2 should go to 1, but that -3/2 should go to -2.
			// Just dividing by 2 gives 3/2 -> 1, and -3/2 -> -1.
			c0			=	C0;
			c			+=	start_c_offset;
			c			-=	abs(c&int3(1,1,1));
			c			=	c>>1;
			gridScale	*=	2.0;
			viewScale	*=	2.0;
			if(idx==0)
				W*=2;
			p0			=	P0;
			P0			=	startOffsetFromOrigin/gridScale/2.0;
			C0			+=	start_c_offset;
			C0			-=	abs(C0&int3(1,1,1));
			C0			=	C0>>1;
			idx			++;
		}
	}
	float rad_mult		=saturate((cos0-minCosine)/(1.0-minCosine));
	colour.rgb			=max(vec3(0,0,0),(rad_mult*colour.rgb-vec3(threshold,threshold,threshold))/(1.0+threshold));
    res.farColour		=vec4(exposure*colour.rgb,1.0);
	return res;
}