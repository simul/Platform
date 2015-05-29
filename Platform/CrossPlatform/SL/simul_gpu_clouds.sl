//  Copyright (c) 2015 Simul Software Ltd. All rights reserved.
#ifndef SIMUL_GPU_CLOUDS_SL
#define SIMUL_GPU_CLOUDS_SL

uint3 LinearThreadToPos3D(uint linear_pos,uint3 dims)
{
	uint Z			=linear_pos/dims.x/dims.y;
	float c			=(float(linear_pos)-float(Z)*float(dims.x)*float(dims.y));
	float y			=c/float(dims.x);
	uint yy			=uint(y);
	uint X			=uint(float(linear_pos)-float(yy)*(float(dims.x)+float(Z)*float(dims.y)));
	uint3 pos		=uint3(X,y,Z);
	return pos;
}

vec3 assemble3dTexcoord(vec2 texcoord2,float zPixel,float zSize)
{
	vec2 texcoord	=texcoord2.xy;
	texcoord.y		*=zSize;
	float zPos		=floor(texcoord.y)/zSize+0.5*zPixel;
	texcoord.y		=fract(texcoord.y);
	vec3 texcoord3	=vec3(texcoord.xy,zPos);
	return texcoord3;
}

float GetShapeFunction(float z,float ztop,float baseLayer,float transition,float upperDensity)
{
	// i is the lerp variable between the top of the base layer and the upper layer.
	float i		=smoothstep(baseLayer,baseLayer+transition,z);
	float m		=lerp(1.0,upperDensity,i);
	float j		=1.0-saturate((0.8+0.2*ztop-z)/transition);
	m			*=sqrt(1.0-j*j);
	return m;
}

float GetHumidityMultiplier(float z,float baseLayer,float transition,float upperDensity)
{
	float i		=saturate((z-baseLayer)/transition);
	float m		=(1.0-i)+upperDensity*i;
	m			*=lerp(0.75,1.0,saturate((1.0-z)/transition));
	return m;
}

float GetHumidityMultiplier2(float z,float baseLayer,float transition,float upperDensity)
{
//	float i		=saturate((z-baseLayer)/transition);
	float i		=pow(saturate((z-baseLayer)/transition),0.5);
	float m		=(1.0-i)+upperDensity*i;
	//m			*=lerp(0.75,1.0,saturate((1.0-z)/transition));
	m			*=pow(saturate((1.0-z)/transition),0.5);
	return m;
}

/// scale should represent the desired noise size - either the size of the noise texture, or of the subset we're using.
/// top means the maximum z, above which we consider the texture to be blank. If top>=scale, it has no effect.
float CircularLookup(Texture3D volumeNoiseTexture,vec3 texCoords,float t,int scale,int top)
{
	vec3 s			=fract(texCoords)*scale;
	//..s.xyz		+=texture_wrap_lod(volumeNoiseTexture,texCoords,0).xyz-0.5;
	int3 pos3		=int3(trunc(s));
	vec3 offs		=fract(s);
#ifdef GLSL
	int3 poss[8]=int3[8](
						int3(0,0,0),
						int3(1,0,0),
						int3(0,1,0),
						int3(1,1,0),
						int3(0,0,1),
						int3(1,0,1),
						int3(0,1,1),
						int3(1,1,1)
					);
#else
	int3 poss[8]	=	{
							{0,0,0},
							{1,0,0},
							{0,1,0},
							{1,1,0},
							{0,0,1},
							{1,0,1},
							{0,1,1},
							{1,1,1},
						};
#endif
	float lookup		=0.0;
	for(int i=0;i<8;i++)
	{
		vec3 corner		=poss[i];
		vec3 dist3		=offs-corner;
		float dist		=length(dist3);
		float mult		=saturate(1.0-(dist/0.866));
		// Each texel "vertex" is the centre of a sphere, of radius 0.866 texels.
		if(mult>0)
		{
			int3 p		=pos3+poss[i];
			p			=p%scale;
			if(p.z>=top)
				continue;
			float val	=texelFetch3d(volumeNoiseTexture, p, 0).x;
			//	volumeNoiseTexture.Load(int4(p,0));
		//	float h		=sin(3.1415926536/2.0*mult);
			// The magnitude of the "peak" at the texel centre is m^2.
			float m		=cos(1.0*3.1415926536*(val+t));
			// It is always positive, and its average is 0.5:
			// The local magnitude depends on the distance to the centre, it is 0 at the edges and m^2 at the centre.
			float c		=sin(3.1415926536/2.0*mult);
			float u		=c*m*m;
			lookup		=max(lookup,u);
			//lookup	+=1.0;
		}
	}
	return 2.0*saturate(lookup)-1.0;//texture_wrap_lod(volumeNoiseTexture,texCoords,0)-1.0;//
}

// height is the height of the total cloud volume as a proportion of the initial noise volume
float ModifierNoiseFunction(Texture3D volumeNoiseTexture,vec3 pos,int octaves,float persistence,float t,float height)
{
	float dens=0.0;
	float mult=persistence;
	float sum=0.0;
	int noiseDimension=8;
	int noiseHeight=12;
	for(int i=0;i<5;i++)
	{
		if(i>=octaves)
			break;
		float lookup	=2.0*texture_3d_wrap_lod(volumeNoiseTexture,pos,0).x-1.0;
		//(CircularLookup(volumeNoiseTexture,pos,1.0,t,noiseDimension,noiseHeight);
		dens			+=mult*lookup;
		sum				+=mult;
		mult			*=persistence;
		pos				*=2.0;
		t				*=2.0;
		height			*=2.0;
		noiseDimension	*=2;
		noiseHeight		*=2;
	}
	if(sum==0.0)
		return 0.0;
	//dens=(dens/sum);
	return dens;
}
float DensityFunction(Texture3D volumeNoiseTexture,vec3 noisespace_texcoord,float t)
{
	int noiseDimension=8;
	int noiseHeight=8;
	// We want to distort the lookup by up to half a noise texel at the specified dimension.
	vec3 u					=vec3(noisespace_texcoord.xy,0);
	noisespace_texcoord.xy	+=2.0/float(noiseDimension)*(texture_3d_wrap_lod(volumeNoiseTexture,u,0).xy-0.5);
	float lookup			=CircularLookup(volumeNoiseTexture,vec3(noisespace_texcoord.xy,0),t,noiseDimension,noiseHeight);
	return lookup;
}

// height is the height of the total cloud volume as a proportion of the initial noise volume
float NoiseFunction(Texture3D volumeNoiseTexture,vec3 pos,int octaves,float persistence,float t,float height,float texel)
{
	float dens=0.0;
	float mult=0.5;
	float sum=0.0;
	for(int i=0;i<5;i++)
	{
		if(i>=octaves)
			break;
		vec3 pos2	=pos;
		// We will limit the z-value of pos2 in order to prevent unwanted blending to out-of-range texels.
		float zmin	=0.5*texel;
		float zmax	=height-0.5*texel;
		pos2.z		=clamp(pos2.z,zmin,zmax);
		pos2.z		*=saturate(i);
		float lookup=texture_3d_wrap_lod(volumeNoiseTexture,pos2,0).x;
		float val	=cos(2.0*3.1415926536*(lookup+t));
		dens		=dens+mult*val;
		sum			=sum+mult;
		mult		=mult*persistence;
		pos			=pos*2.0;
		t			=t*2.0;
		height		*=2.0;
	}
	dens=(dens/sum);
	return dens;
}

// height is the height of the total cloud volume as a proportion of the initial noise volume
float NoiseFunction2(Texture3D volumeNoiseTexture,vec2 pos,int octaves,float persistence,float t,float texel)
{
	float dens	=0.0;
	float mult	=0.5;
	float sum	=0.0;
	for(int i=0;i<5;i++)
	{
		if(i>=octaves)
			break;
		vec2 pos2		=pos;
		float lookup	=texture_3d_wrap_lod(volumeNoiseTexture,vec3(pos2,0.5*texel),0).x;
		float val		=cos(2.0*3.1415926536*(lookup+t));
		dens			=dens+mult*val;
		sum				=sum+mult;
		mult			=mult*persistence;
		pos				=pos*2.0;
		t				=t*2.0;
	}
	dens=(dens/sum);
	return dens;
}


float GpuCloudMask(vec2 texCoords, vec2 maskCentre, float maskRadius, float maskFeather, float maskThickness, mat4 cloudspaceToWorldspaceMatrix)
{
	vec3 wPos	= (mul(cloudspaceToWorldspaceMatrix, vec4(texCoords.xy,0, 1))).xyz;
    float dr	=maskFeather;
	vec2 pos	=wPos.xy - maskCentre;
    float r		=length(pos)/maskRadius;
    float dens	=maskThickness*saturate((1.0-r)/dr);
    return dens;
}

float CloudDensity(	Texture3D volumeNoiseTexture
					,Texture2D maskTexture
					,vec2 texCoords
					,float humidity
					,float diffusivity
					,int octaves
					,float persistence
					,float time
					,float zPixel
					,float zSize
					,float noise_dims_z
					,vec3 noiseScale
					,float baseLayer
					,float transition
					,float upperDensity)
{
	vec3 densityspace_texcoord	=assemble3dTexcoord(texCoords.xy,zPixel,zSize);
	vec3 noisespace_texcoord	=(densityspace_texcoord+vec3(0,0,0.0*zPixel))*noiseScale+vec3(1.0,1.0,0);
	float noise_texel			=1.0/noise_dims_z;
	float height				=noiseScale.z;
#if 0
	// main_density goes from -1 to +1.
	float main_density			=DensityFunction(volumeNoiseTexture,noisespace_texcoord,time);
	vec4 mask					=texture_clamp_lod(maskTexture,densityspace_texcoord.xy,0);
	float hm					=humidity*GetShapeFunction(densityspace_texcoord.z,main_density,baseLayer,transition,upperDensity)*mask.x;//
	float noise_mod				=ModifierNoiseFunction(volumeNoiseTexture,noisespace_texcoord,octaves,persistence,time,height,noise_texel);
	float dens					=main_density+noise_mod+2.0*hm-1.0;
	dens						=saturate(dens/diffusivity);
#else
	float noise_val				=NoiseFunction(volumeNoiseTexture,noisespace_texcoord,octaves,persistence,time,height,noise_texel);
	float hm					=humidity*GetHumidityMultiplier2(densityspace_texcoord.z,baseLayer,transition,upperDensity);//*texture_clamp_lod(maskTexture,densityspace_texcoord.xy,0).x;
	float dens					=saturate((noise_val+hm-1.0)/diffusivity);
#endif
	//dens						=saturate((dens+hm)/diffusivity);
	dens						*=saturate(densityspace_texcoord.z/zPixel-0.5)*saturate((1.0-0.5*zPixel-densityspace_texcoord.z)/zPixel);
    return dens;
}

#endif