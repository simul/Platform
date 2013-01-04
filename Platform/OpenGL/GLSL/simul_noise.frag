varying vec2 texc;

float rand(vec2 co)
{
    return fract(sin(dot(co.xy ,vec2(12.9898,78.233))) * 43758.5453);
}

void main(void)
{
    vec4 c=rand(texc)*vec4(1.0,1.0,1.0,1.0);
    gl_FragColor=c;
}
