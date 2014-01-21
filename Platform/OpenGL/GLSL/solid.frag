#version 130
uniform sampler2D imageTexture;
in vec2 texCoords;
in vec3 normal;
out vec4 gl_FragColor;

void main(void)
{
    // original image
    vec4 c = texture2D(imageTexture,texCoords);
	c.a=1.0;
	float light=normal.z;
	c.rgb*=light;
    gl_FragColor=c;

}
