uniform sampler2D noise_texture;
uniform float persistence;
uniform int octaves;
varying vec2 texc;

float saturate(float x)
{
	return clamp(x,0.0,1.0);
}

vec4 saturate(vec4 x)
{
	return clamp(x,vec4(0.0,0.0,0.0,0.0),vec4(1.0,1.0,1.0,1.0));
}

void main(void)
{
	vec4 result=vec4(0,0,0,0);
	vec2 texcoords=texc;
	float mul=.5;
	vec4 hal=vec4(0.5,0.5,0.5,0.5);
    for(int i=0;i<octaves;i++)
    {
		vec4 c=texture2D(noise_texture,texcoords)-hal;
		texcoords*=2.0;
		result+=mul*c;
		mul*=persistence;
    }
	result*=0.5;
	result+=hal;
    result=saturate(result);
    gl_FragColor=result;
}
