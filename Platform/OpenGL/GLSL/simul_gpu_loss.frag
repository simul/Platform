uniform sampler2D input_loss_texture;
uniform float zPosition;

varying vec2 texc;

void main(void)
{
	vec4 previous_loss=texture2D(input_loss_texture,texc.xy);
	previous_loss*=0.9;
    gl_FragColor=previous_loss;
}
