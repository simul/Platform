#version 140
uniform sampler2D image_texture;
uniform sampler2D depthAlphaTexture;

in vec2 texCoords;
uniform vec2 screenCoordOffset;

void main(void)
{
	vec2 depthTexCoord	=screenCoordOffset+texCoords;
	float depth			=texture(depthAlphaTexture,depthTexCoord).a;
#ifdef REVERSE_DEPTH
	//if(depth<=0.0)
	//	discard;
#else
	//if(depth>=1.0)
	//	discard;
#endif
    vec4 c = texture2D(image_texture,texCoords);
    gl_FragColor=c;
}
