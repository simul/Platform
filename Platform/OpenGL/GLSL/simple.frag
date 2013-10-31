uniform sampler2D image_texture;
varying vec2 texCoords;

void main(void)
{
    vec4 c = texture2D(image_texture,texCoords);
    gl_FragColor=c;
}
