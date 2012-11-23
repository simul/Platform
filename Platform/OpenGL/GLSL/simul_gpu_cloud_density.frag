//uniform vec2 texCoordOffset;
uniform sampler3D volumeNoiseTexture;
uniform float zPosition;
uniform int octaves;
uniform float persistence;
uniform float humidity;
uniform float time;
uniform float zPixel;
uniform vec3 noiseScale;

varying vec2 in_texcoord;

float saturate(float x)
{
	return clamp(x,0.0,1.0);
}

float GetHumidityMultiplier(float z)
{
	const float base_layer=0.46;
	const float transition=0.2;
	const float mul=0.7;
	const float default_=1.0;
	float i=saturate((z-base_layer)/transition);
	return default_*(1.0-i)+mul*i;
}

float NoiseFunction(vec3 pos)
{
	float dens=0.0;
	float mul=0.5;
	//int octave_3d=1;
	float this_grid_height=1.0;
	float sum=0.0;
	float t=time;
	for(int i=0;i<5;i++)
	{
		if(i>=octaves)
			break;
		//float kmax=max(1.0,(float)floor(this_grid_height+0.5));
		float lookup=texture3D(volumeNoiseTexture,pos).x;
		float val=lookup;//0.5+0.5*sin(2.0*pi*(lookup+t));//
		//val*=saturate(pos.z*8.0+.5f);
		//val*=saturate(kmax+0.5f-pos.z*8.0);
		dens+=mul*val;
		sum+=mul;
		mul*=persistence;
		pos*=2.0;
		this_grid_height*=2.0;
		t*=2.0;
	}
	dens/=sum;
	return dens;
}

void main()
{
	vec3 densityspace_texcoord=vec3(in_texcoord,zPosition);
	vec3 noisespace_texcoord=densityspace_texcoord*noiseScale+vec3(1.0,1.0,0);
	float noise_val=NoiseFunction(noisespace_texcoord);
	float hm=humidity*GetHumidityMultiplier(densityspace_texcoord.z);
	float diffusivity=0.02;
	float dens=saturate((noise_val+hm-1.0)/diffusivity);
	dens*=saturate(densityspace_texcoord.z/zPixel-0.5)*saturate((1.0-0.5*zPixel-densityspace_texcoord.z)/zPixel);
    gl_FragColor=vec4(dens,0,0,1.0);
}

