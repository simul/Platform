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
    for(int i=0;i<5;i++)
    {
		texcoords+=offset;
		vec4 v=texture(dens_texture,texcoords);
		dens_dist+=v.a;
    }
    float light=exp(-dens_dist);//=saturate(result*1.5);
    float sec=exp(-dens_dist/12.0);
    gl_FragColor=vec4(light,sec,1.0,c.a);
}
