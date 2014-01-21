#version 130
uniform sampler2D imageTexture;
in vec2 texCoords;
out vec4 gl_FragColor;

void main(void)
{
    // original image
    vec4 c = texture2D(imageTexture,texCoords);
    gl_FragColor=c;
}
