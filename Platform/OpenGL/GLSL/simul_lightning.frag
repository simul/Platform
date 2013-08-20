#version 130
uniform sampler1D lightningTexture;
uniform vec3 lightningColour;
in vec2 texcoord;
in float width;
out vec4 fragmentColour;

void main(void)
{
	float x=1.0-abs(texcoord.x*2.0-1.0)/width;
	x=clamp(x,0.0,1.0);
	x=pow(clamp(2.0*x,0.0,1.0),1.0)*width;
    vec4 c=vec4(lightningColour,1.0)*x;
    fragmentColour=c;
}
