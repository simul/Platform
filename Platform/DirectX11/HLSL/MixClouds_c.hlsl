
Texture3D<float4> cloudDensity1 : register(t0);
Texture3D<float4> cloudDensity2 : register(t1);
RWTexture3D<float4> output : register(u0);
cbuffer MixCloudsConstants : register(b10)
{
	float interpolation;
	float pad1,pad2,pad3;
};

[numthreads(8, 8, 4)]
void main( uint3 pos : SV_DispatchThreadID )
{
    output[pos] = lerp(cloudDensity1[pos],cloudDensity2[pos],interpolation);
}
