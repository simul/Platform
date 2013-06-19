uniform sampler2D image_texture;
uniform vec2 offset;
varying vec2 texc;

void main(void)
{
	vec4 colour	=vec4(0.0,0.0,0.0,0.0);
	vec2 offs	=offset;
    // original image
	for(int i=-25;i<25;i++)
	{
		vec2 d=offs*float(i);
		vec2 t=texc+d;
		float ii=float(i*i);
		float str=0.04*exp(-ii/125.0);
		vec4 c=str*texture2D(image_texture,t);
		colour+=c;
	}
    gl_FragColor=colour;
}
