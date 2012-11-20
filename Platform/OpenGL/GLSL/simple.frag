uniform sampler2D image_texture;
varying vec2 texc;

void main(void)
{
    vec4 c = texture2D(image_texture,texc);
    gl_FragColor=c;
}
