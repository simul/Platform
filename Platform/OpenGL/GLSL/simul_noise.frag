varying vec2 texCoords;

float rand(vec2 co)
{
    return fract(sin(dot(co.xy ,vec2(12.9898,78.233))) * 43758.5453);
}

void main(void)
{
    vec4 c=vec4(rand(texCoords),rand(1.7*texCoords),rand(0.11*texCoords),rand(513.1*texCoords));
    gl_FragColor=c;
}
