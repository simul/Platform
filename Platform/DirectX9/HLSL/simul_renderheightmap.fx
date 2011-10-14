
texture noiseTexture;
sampler2D noise_texture= sampler_state 
{
    Texture = <noiseTexture>;
    MipFilter = Linear;
    MinFilter = Linear;
    MagFilter = Linear;
	AddressU = Wrap;
	AddressV = Wrap;
};

texture heightTexture;
sampler2D height_texture=sampler_state
{
    Texture = <heightTexture>;
    MipFilter = Point;
    MinFilter = Point;
    MagFilter = Point;
	AddressU = Clamp;
	AddressV = Clamp;
};

texture addTexture;
sampler2D add_texture=sampler_state
{
    Texture = <addTexture>;
    MipFilter = Point;
    MinFilter = Point;
    MagFilter = Point;
	AddressU = Clamp;
	AddressV = Clamp;
};
texture waterTexture;
sampler2D water_texture=sampler_state
{
    Texture = <waterTexture>;
    MipFilter = Point;
    MinFilter = Point;
    MagFilter = Point;
	AddressU = Border;
	AddressV = Border;
	BorderColor=0xFF000000;
};


texture remapHeightsTexture;
sampler1D remap_heights_texture=sampler_state
{
    Texture = <remapHeightsTexture>;
    MipFilter = Linear;
    MinFilter = Linear;
    MagFilter = Linear;
	AddressU = Clamp;
	AddressV = Clamp;
};

int octaves;
float persistence;
float2 heightRange;
float texcoordScale;
float worldSize;
float gridSize;
float talusTangent;
float rainfall;
float evaporation;

// tools
float3 toolCentre;
float toolRadius;		//metres
float toolStrength;


float3 eight_offsets[8];
float offsetSize;

struct a2v
{
    float4 position  : POSITION;
    float2 texcoord  : TEXCOORD0;
};

struct v2f
{
    float4 position  : POSITION;
    float2 texcoord  : TEXCOORD0;
};

v2f MainVS(a2v IN)
{
	v2f OUT;
	OUT.position = IN.position;
	OUT.texcoord = IN.texcoord;
    return OUT;
}

float4 FractalHeightPS(v2f IN) : COLOR
{
	float h=0;
	float scale=.5f;
	int i;
	float2 texcoord=IN.texcoord*texcoordScale;
	float low=1.0/32.0/texcoordScale;
	for(i=0;i<octaves;i++)
	{
		float4 lookup=tex2D(noise_texture,texcoord);
		lookup.r*=saturate(IN.texcoord.x/low)*saturate(IN.texcoord.y/low)
			*saturate((1.0-IN.texcoord.x)/low)*saturate((1.0-IN.texcoord.y)/low);
		float newc=scale*2.0*(lookup.r-0.5);
		h+=newc;
		scale*=persistence;
		texcoord*=2.f;
		low/=2.f;
	}
	h+=0.5f;
	h*=heightRange.y;
	h+=heightRange.x;
    return float4(h,0,0,0);
}

float4 RidgePS(v2f IN) : COLOR
{
	float h=tex2D(height_texture,IN.texcoord).r;
	h-=heightRange.x;
	h/=heightRange.y;
	if(h>1.0)
		h=1.0;
	h=tex1D(remap_heights_texture,h).r;
	h*=heightRange.y;
	h+=heightRange.x;
	return float4(h,h,h,0);
}
//For each cell, how much matter is to be transported out of it?
float4 ThermalErosionOutPS(v2f IN) : COLOR
{
	float2 t=IN.texcoord;
	float4 texel=tex2D(height_texture,t);
	// The ground height
	float h=texel.r;
	// soil depth
	float soil=texel.w;
	float max_change=h/2.0;
	if(max_change<0)
		max_change=0;
	float D=offsetSize*worldSize;
	float total=0.f;
	for(int i=0;i<8;i++)
	{
		float3 offset=eight_offsets[i];
		float2 t00=t+offset.xy*offsetSize;
		float H=tex2D(height_texture,t00).r;
		float DH=h-H;
		float talusDH=talusTangent*D*offset.z;
		if(DH>talusDH)
		{
			total+=DH-talusDH;
		}
	}
	return float4(total,h,soil,0);
}

// Here, add_texture is the output of ThermalErosionOutPS
float4 ThermalErosionInPS(v2f IN) : COLOR 
{
	float2 t=IN.texcoord;
	float4 texel=tex2D(add_texture,t);
	float h=texel.y;
	float soil=texel.z;
	float max_change=h/2.0;
	float D=offsetSize*worldSize;
	float total=0;
	for(int i=0;i<8;i++)
	{
		float3 offset=eight_offsets[i];
		float2 t00=t-offset.xy*offsetSize;
		float4 h00=tex2D(add_texture,t00).y;
		float DH=h00-h;
		float talusDH=talusTangent*D*offset.z;
		if(DH>talusDH)
		{
			total+=DH-talusDH;
		}
	}
	float ds=0.1f*total-0.1f*texel.x;
	if(soil+ds<0)
	{
		h+=ds;
		ds=-soil;
	}
	return float4(h-soil,0,0,soil+ds);
}

// This updates the water texture
// Here, add_texture is the texture containing the water depth
// w=water_depth
// x=outflow
// y=suspended sediment
float4 HydraulicErosionOutPS(v2f IN) : COLOR
{
	float2 t=IN.texcoord;
	float d=offsetSize;
	float4 water_texel=tex2D(water_texture,t);
	float4 height_texel=tex2D(height_texture,t);
	float W=water_texel.w;
	float sediment=water_texel.y;
	float H=W+height_texel.x;
	float outflow=0.0;
	float4 outflows_0=tex2D(add_texture,t);
	float4 outflows_1=outflows_0;
	float of1[4];
	for(int i=0;i<4;i++)
	{
		float3 offset=eight_offsets[i];
		float2 t_=t+offset.xy*d;
		float4 water_texel_=tex2D(water_texture,t_);
		float4 height_texel_=tex2D(height_texture,t_);
		float h=water_texel_.w+height_texel_.x;
		// The difference in height means a difference in pressure.
		float b_=0.2*(H-h);
		// We want to accumulate the outflow. So we only add up if H>h.
		of1[i]=b_;
		outflow=outflow+b_;
	}
	float xx=0.01f;
	outflows_1=max(float4(0,0,0,0),(1.f-xx)*outflows_0+float4(of1[0],of1[1],of1[2],of1[3]));
	/*float prop=1.0;
	W+=rainfall;*/
	outflow=dot(outflows_1,float4(1.0,1.0,1.0,1.0));
	if(outflow>W)
	{
		outflows_1*=W/outflow;
	}
	return outflows_1;
}

// This uses the water texture to modify the landscape
// add_texture is the output of HydraulicErosionOutPS, the new outflow texture
// height_texture is the existing height texture.
float4 HydraulicErosionInPS(v2f IN) : COLOR
{
	float2 t=IN.texcoord;
	float d=offsetSize;
	float4 outflows=tex2D(add_texture,IN.texcoord);
	float4 flow_texel=tex2D(water_texture,IN.texcoord);
	float4 height_texel=tex2D(height_texture,t);
	float slope_sine=sqrt(dot(height_texel.yz,height_texel.yz));
	float W=flow_texel.w;
	float H=W+height_texel.x;
	float sediment=flow_texel.y;
	float total_outflow=dot(outflows,float4(1.0,1.0,1.0,1.0));
	float total_inflow=0.0;
	for(int i=0;i<4;i++)
	{
		float3 offset=eight_offsets[i];
		float2 t_=t+offset.xy*d;
		float4 other_outflow=tex2D(add_texture,t_);
		float b_=other_outflow[3-i];
		total_inflow+=b_;
		float4 flow_texel_=tex2D(water_texture,t_);
		// sediment flows in proportionally to the water, and the sediment density at the offset
		sediment+=b_*flow_texel_.y/flow_texel_.w;
	}
	W+=rainfall+total_inflow-total_outflow;
	if((t.x==0||t.y==0||t.x>=1.0-d||t.y>=1.0-d)&&height_texel.x<0) 
		W=-height_texel.x;
	//W-=total_outflow;
	W*=1.0-evaporation;
	if(W<=0)
		W=0;
	else
		sediment-=total_outflow*sediment/W;
	// We calculate how much, if any, sediment should be removed or deposited:
	float sed_change=0;
	float sed_capacity=clamp(slope_sine*saturate(total_outflow),0,1.0);
	sed_change=.1*clamp(sed_capacity-sediment,0,1.0);
	float next_sediment=sediment+sed_change;
	if(next_sediment<0)
		next_sediment=0;
	sed_change=saturate(next_sediment-sediment);
	return float4(flow_texel.x,next_sediment,sed_change,W);
}

// Now add_texture is water_texture, the output from HydraulicErosionInPS.
// We use this to determine how much rock or soil is eroded from the ground.
float4 HydraulicErosionApplyPS(v2f IN) : COLOR
{
	float d=offsetSize;
	float2 t=IN.texcoord;
	// height_texture has the total height in the x, and the soil depth in the w.
	float4 height_texel=tex2D(height_texture,t);
	float soil=height_texel.w;
	float h=height_texel.x;
	// add_texture contains the water amount, and sediment amount.
	float4 water_texel=tex2D(add_texture,t);
	float W=water_texel.w;
	float sediment=water_texel.y;
	float sed_change=water_texel.z;
	soil-=10.0*sed_change;
	if(soil<0)
	{
		h+=soil;
		soil=0;
	}
	// We write the rock height and soil depth, ready for input into HeightToNormalsPS
	return float4(h-soil,0,0,soil);
}

float4 RaiseTerrainPS(v2f IN) : COLOR
{
	float4 height_texel=tex2D(height_texture,IN.texcoord);
	float2 offset=IN.texcoord-toolCentre.xy;
	float r=length(offset);
	float strength=toolStrength*saturate(1.f-r/toolRadius);
	return float4(height_texel.x-height_texel.w+strength,height_texel.yzw);
}

float4 FlattenTerrainPS(v2f IN) : COLOR
{
	float2 t=IN.texcoord;
	float4 height_texel=tex2D(height_texture,t);
	float2 offset=IN.texcoord-toolCentre.xy;
	float r=length(offset);
	float strength=1.0-exp(-toolStrength*saturate(1.f-r/toolRadius));
	float h=0.0;
	float tot_weight=0;
	for(int i=0;i<8;i++)
	{
		float3 offset=eight_offsets[i]*offsetSize;
		float2 t_=t+offset.xy*offsetSize;
		float4 height_texel_=tex2D(height_texture,t_);
		float weight=strength/offset.z;
		h+=(height_texel_.x-height_texel_.w)*weight;
		tot_weight+=weight;
	}
	h+=(height_texel.x-height_texel.w);
	tot_weight+=1.0;
	h/=tot_weight;
	return float4(h,height_texel.yzw);
}

float4 HeightToNormalsPS(v2f IN) : COLOR
{
	float4 texel=tex2D(height_texture,IN.texcoord);
	float W=tex2D(add_texture,IN.texcoord).w;
	float soil=texel.w;
	float h=texel.r+soil+W;
	float dx=ddx(h);
	float dy=ddy(h);
	float3 n=float3(-dx,-dy,worldSize/gridSize);
	n=normalize(n);
	return float4(h-W,n.xy,soil);
}

technique fractal_height
{
    pass p0
    {
		cullmode = none;
		ZEnable = false;
		ZWriteEnable = false;
		AlphaBlendEnable = false;
		VertexShader = compile vs_3_0 MainVS();
		PixelShader = compile ps_3_0 FractalHeightPS();
    }
}

// Ridge filter:
technique ridge
{
    pass p0
    {
		cullmode = none;
		ZEnable = false;
		ZWriteEnable = false;
		AlphaBlendEnable = false;
		VertexShader = compile vs_3_0 MainVS();
		PixelShader = compile ps_3_0 RidgePS();
    }
}

technique height_to_normals
{
    pass p0
    {
		cullmode = none;
		ZEnable = false;
		ZWriteEnable = false;
		AlphaBlendEnable = false;
		VertexShader = compile vs_3_0 MainVS();
		PixelShader = compile ps_3_0 HeightToNormalsPS();
    }
}

technique thermal_erosion_out
{
    pass p0
    {
		cullmode = none;
		ZEnable = false;
		ZWriteEnable = false;
		AlphaBlendEnable = false;
		VertexShader = compile vs_3_0 MainVS();
		PixelShader = compile ps_3_0 ThermalErosionOutPS();
    }
}
technique thermal_erosion_in
{
    pass p0
    {
		cullmode = none;
		ZEnable = false;
		ZWriteEnable = false;
		AlphaBlendEnable = false;
		VertexShader = compile vs_3_0 MainVS();
		PixelShader = compile ps_3_0 ThermalErosionInPS();
    }
}

technique hydraulic_erosion_out
{
    pass p0
    {
		cullmode = none;
		ZEnable = false;
		ZWriteEnable = false;
		AlphaBlendEnable = false;
		VertexShader = compile vs_3_0 MainVS();
		PixelShader = compile ps_3_0 HydraulicErosionOutPS();
    }
}

technique hydraulic_erosion_in
{
    pass p0
    {
		cullmode = none;
		ZEnable = false;
		ZWriteEnable = false;
		AlphaBlendEnable = false;
		VertexShader = compile vs_3_0 MainVS();
		PixelShader = compile ps_3_0 HydraulicErosionInPS();
    }
}

technique hydraulic_erosion_apply
{
    pass p0
    {
		cullmode = none;
		ZEnable = false;
		ZWriteEnable = false;
		AlphaBlendEnable = false;
		VertexShader = compile vs_3_0 MainVS();
		PixelShader = compile ps_3_0 HydraulicErosionApplyPS();
    }
}

technique raise_terrain
{
    pass p0
    {
		cullmode = none;
		ZEnable = false;
		ZWriteEnable = false;
		AlphaBlendEnable = false;
		VertexShader = compile vs_3_0 MainVS();
		PixelShader = compile ps_3_0 RaiseTerrainPS();
    }
}

technique flatten_terrain
{
    pass p0
    {
		cullmode = none;
		ZEnable = false;
		ZWriteEnable = false;
		AlphaBlendEnable = false;
		VertexShader = compile vs_3_0 MainVS();
		PixelShader = compile ps_3_0 FlattenTerrainPS();
    }
}