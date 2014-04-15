#version 140
#include "CppGlsl.hs"
uniform sampler2D imageTexture;
in vec2 texCoords;
in vec3 normal;
out vec4 gl_FragColor;

void main(void)
{
    vec4 c = texture_wrap(imageTexture,texCoords);
	c.a=1.0;
	float light=clamp(normal.z,0.0,1.0);
	c.rgb*=light;
    gl_FragColor=c;
}
