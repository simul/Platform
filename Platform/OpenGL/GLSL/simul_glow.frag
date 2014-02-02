uniform sampler2D image_texture;
varying vec2 texCoords;
uniform vec2 offset;
uniform float exposure;

void main(void)
{
    // original image has double the resulution, so we sample 2x2
    vec4 c=vec4(0,0,0,0);
	c+=texture2D(image_texture,texCoords+offset/2.0);
	c+=texture2D(image_texture,texCoords-offset/2.0);
	vec2 offset2=offset;
	offset2.x=-offset.x;
	c+=texture2D(image_texture,texCoords+offset2/2.0);
	c+=texture2D(image_texture,texCoords-offset2/2.0);
	c=c*exposure/4.0;
	c-=1.0*vec4(1.0,1.0,1.0,1.0);
	c=clamp(c,vec4(0.0,0.0,0.0,0.0),vec4(10.0,10.0,10.0,10.0));
    gl_FragColor=c;
}
