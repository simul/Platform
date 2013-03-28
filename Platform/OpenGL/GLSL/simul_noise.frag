varying vec2 texc;

float rand(vec2 co)
{
    return fract(sin(dot(co.xy ,vec2(12.9898,78.233))) * 43758.5453);
}

void main(void)
{
    vec4 c=vec4(rand(texc),rand(1.7*texc),rand(0.11*texc),rand(513.1*texc));
    gl_FragColor=c;
}
