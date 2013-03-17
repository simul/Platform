#version 130
uniform sampler1D lightningTexture;
in vec2 texcoord;

out vec4 fragmentColour;
void main(void)
{
    vec4 c=texcoord.y*texture1D(lightningTexture,texcoord.x);
    c.r=texcoord.x;
    fragmentColour=c;
}
