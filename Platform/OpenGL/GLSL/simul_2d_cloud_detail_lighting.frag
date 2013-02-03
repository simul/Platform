#version 140
uniform sampler2D dens_texture;
uniform vec3 lightDir;
varying vec2 texc;

void main(void)
{
    vec4 c=texture(dens_texture,texc);
	vec4 result=c;
	vec2 texcoords=texc;
	float mul=0.5;
	vec2 offset=lightDir.xy/512.0;
	float dens_dist=0.0;
    for(int i=0;i<16;i++)
    {
		texcoords+=offset;
		vec4 v=texture(dens_texture,texcoords);
		dens_dist+=v.a;
		//if(v.a==0)
			//dens_dist*=0.9;
    }
    float l=c.a*exp(-dens_dist/2.0);
    gl_FragColor=vec4(dens_dist/32.0,dens_dist/32.0,dens_dist/32.0,c.a);
}
