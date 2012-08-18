uniform sampler3D cloud_density;
varying vec2 texCoords;
uniform float crossSectionOffset;
uniform vec4 lightResponse;
uniform float yz;

void main(void)
{
	vec3 texc=vec3(crossSectionOffset+texCoords.x,yz*(crossSectionOffset+texCoords.y),(1.0-yz)*(1.0-texCoords.y));//+yz*0.125
	int i=0;
	vec3 accum=vec3(0.0,0.5,1.0);
	for(i=0;i<32;i++)
	{
		vec4 density=texture3D(cloud_density,texc);
		vec3 colour=vec3(.5,.5,.5)*(lightResponse.x*density.z+lightResponse.y*density.w);
		colour.gb+=vec2(.125,.25)*(lightResponse.z*density.x);
		float opacity=density.y;
		colour*=opacity;
		accum*=1.0-opacity;
		accum+=colour;
		texc.y=texc.y+1.0/32.0*(1.0-yz);
		texc.z=texc.z+1.0/32.0*yz;
	}
	//accum=texture3D(cloud_density,vec3(crossSectionOffset+texCoords.x,crossSectionOffset+texCoords.y,0.35)).rgb;
    gl_FragColor=vec4(accum,1);
}
