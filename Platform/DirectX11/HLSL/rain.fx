#include "CppHlsl.hlsl"
#include "states.hlsl"
float particleZoneSize=20.0;
#include "../../CrossPlatform/rain_constants.sl"
#include "../../CrossPlatform/simul_inscatter_fns.sl"
#include "../../CrossPlatform/depth.sl"
#include "../../CrossPlatform/noise.sl"
texture3D randomTexture3D;
// Same as transformedParticle, but with semantics
struct particleVertexOutput
{
    float4 position0		:SV_POSITION;
    float4 position1		:TEXCOORD2;
	float pointSize			:PSIZE;
	float brightness		:TEXCOORD0;
	vec3 view				:TEXCOORD1;
	float fade				:TEXCOORD3;
};

texture2D randomTexture;
TextureCube cubeTexture;
texture2D rainTexture;
texture2D showTexture;
// The RESOLVED depth texture at full resolution
texture2D depthTexture;
Texture2DArray rainTextureArray;

StructuredBuffer<vec3> positions;
RWStructuredBuffer<vec3> targetVertexBuffer;

SamplerState rainSampler
{
	Filter = MIN_MAG_MIP_LINEAR;
	AddressU = Wrap;
	AddressV = Wrap;
};

struct vertexInputRenderRainTexture
{
    float4 position		: POSITION;
    float2 texCoords	: TEXCOORD0;
};

struct vertexInput
{
    float3 position		: POSITION;
    float4 texCoords	: TEXCOORD0;
};

struct PosAndId
{
    float3 position		: POSITION;
	uint vertex_id		: SV_VertexID;
};

struct PosOnly
{
    float3 position		: POSITION;
};

struct VSParticleIn
{   
    float3 pos			: POSITION;         //position of the particle
    //float3 seed		: SEED;
    //float3 speed		: SPEED;
    //float random		: RAND;
    //uint Type			: TYPE;             //particle type
};


struct vertexOutput
{
    float4 hPosition	: SV_POSITION;
    float4 texCoords	: TEXCOORD0;		/// z is intensity!
    float3 viewDir		: TEXCOORD1;
};

struct rainVertexOutput
{
    float4 position			: SV_POSITION;
    float2 clip_pos			: TEXCOORD1;
    float2 texCoords		: TEXCOORD0;
};

struct particleGeometryOutput
{
    float4 position			:SV_POSITION;
    float2 texCoords		:TEXCOORD0;
	float brightness		:TEXCOORD1;
	vec3 view				:TEXCOORD2;
	float fade				:TEXCOORD3;
};

vec3 Frac(vec3 pos,float scale)
{
	vec3 unity		=vec3(1.0,1.0,1.0);
	return scale*(2.0*frac(0.5*(pos/scale+unity))-unity);
}

vec3 Frac(vec3 pos,vec3 p1,float scale)
{
	vec3 unity	=vec3(1.0,1.0,1.0);
	vec3 p2		=scale*(2.0*frac(0.5*(p1/scale+unity))-unity);
	pos			+=p2-p1;
	return pos;
}

posTexVertexOutput VS_ShowTexture(idOnly id)
{
    return VS_ScreenQuad(id,rect);
}

vec4 PS_ShowTexture(posTexVertexOutput In): SV_TARGET
{
    return texture_wrap_lod(showTexture,In.texCoords,0);
}

void transf(out TransformedParticle p,in vec3 position,int i)
{
	vec3 particlePos	=position.xyz;
	//particlePos		*=10.0;
	float sc			=1.0+0.7*rand3(position.xyz);
	//vec3 pp1			=particlePos-viewPos[1].xyz+offset[1].xyz*sc;
	//particlePos		-=viewPos[i].xyz;
	//particlePos		+=offset[i].xyz*sc;
	//particlePos		=Frac(particlePos,pp1,10.0);
	float ph			=flurryRate*phase;
	vec3 rand1			=randomTexture3D.SampleLevel(wrapSamplerState,particlePos/100.0,0).xyz;
	vec3 rand2			=randomTexture3D.SampleLevel(wrapSamplerState,particlePos/100.0*5.0,0).xyz;
	particlePos			+=2.5*flurry*rand1;
	particlePos			+=.7*flurry*rand2;
	p.position			=mul(worldViewProj[i],vec4(particlePos.xyz,1.0));
	p.view				=normalize(particlePos.xyz);
	p.pointSize			=snowSize*(1.0+0.4*rand2.y);
	p.brightness		=1.0;
	float dist			=length(particlePos.xyz-viewPos[1].xyz);
	
	p.fade				=saturate(10000.0/dist);///length(clip_pos-viewPos);
}

[numthreads(10,10,10)]
void CS_MakeVertexBuffer(uint3 idx	: SV_DispatchThreadID )
{
	vec3 r						=vec3(idx)/50.0;
	vec3 pos					=vec3(rand3(r),rand3(11.01*r),rand3(587.087*r));
	pos							*=2.0;
	pos							-=vec3(1.0,1.0,1.0);
	int i						=idx.z*2500+idx.y*50+idx.x;
	targetVertexBuffer[i]		=particleZoneSize*pos;
}

[numthreads(10,10,10)]
void CS_MoveParticles(uint3 idx	: SV_DispatchThreadID )
{
	int i						=idx.z*2500+idx.y*50+idx.x;
	vec3 pos					=targetVertexBuffer[i];
	pos							+=meanFallVelocity*timeStepSeconds;
	if(pos.z<-particleZoneSize)
		pos.z+=2.0*particleZoneSize;
	if(pos.x<-particleZoneSize)
		pos.x+=2.0*particleZoneSize;
	else if(pos.x>particleZoneSize)
		pos.x-=2.0*particleZoneSize;
	if(pos.y<-particleZoneSize)
		pos.y+=2.0*particleZoneSize;
	else if(pos.y>particleZoneSize)
		pos.y-=2.0*particleZoneSize;
	targetVertexBuffer[i]		=pos;
}

particleVertexOutput VS_Particles(PosAndId IN)
{
	particleVertexOutput OUT;
	TransformedParticle p0;
	transf(p0,IN.position,0);
	TransformedParticle p1;
	transf(p1,IN.position,1);
	
    OUT.position0	=p0.position;
    OUT.position1	=p1.position;
	OUT.pointSize	=p1.pointSize;
	OUT.brightness	=p1.brightness;
	OUT.fade		=p1.fade;
	OUT.view		=p1.view;
	return OUT;
}

cbuffer cbImmutable
{
    float4 g_positions[4] : packoffset(c0) =
    {
        float4(-0.5,-0.5,0,0),
        float4( 0.5,-0.5,0,0),
        float4(-0.5, 0.5,0,0),
        float4( 0.5, 0.5,0,0),
    };
    float2 g_texcoords[4] : packoffset(c4) = 
    { 
        float2(0,0),
        float2(1,0), 
        float2(0,1),
        float2(1,1),
    };
};

[maxvertexcount(6)]
void GS_Particles(point particleVertexOutput input[1], inout TriangleStream<particleGeometryOutput> SpriteStream)
{
    particleGeometryOutput	output;
	// Emit four new triangles.

	// The two centres of the streak positions.
	vec4 pos1=input[0].position0;
	vec4 pos2=input[0].position1;
	
	if(pos1.y/pos1.w>pos2.y/pos2.w)
	{
		vec4 pos_temp=pos2;
		pos2=pos1;
		pos1=pos_temp;
	}
	float sz=input[0].pointSize;
	output.brightness	=input[0].brightness;  
	output.fade			=input[0].fade;  
	output.view			=input[0].view;
#if 0
	vec2 diff				=pos2.yx-pos1.yx;
	diff.y					=-diff.y;
	diff					=sz*diff/(0.0001+length(diff));
	vec4 side				=vec4(diff/2.0,0,0);
	{
		output.position		=pos1+side; 
		output.texCoords	=g_texcoords[0];
		SpriteStream.Append(output);
		output.position		=pos1-side; 
		output.texCoords	=g_texcoords[1];
		SpriteStream.Append(output);
		output.position		=pos2+side; 
		output.texCoords	=g_texcoords[2];
		SpriteStream.Append(output);
		output.position		=pos2-side; 
		output.texCoords	=g_texcoords[3];
		SpriteStream.Append(output);
	}
#else
	if(pos1.x/pos1.w<=pos2.x/pos2.w)
	{
		// bottom-left quadrant:
		output.position		=pos1+vec4(g_positions[0].xy*sz,0,0); 
		output.texCoords	=g_texcoords[0];
		SpriteStream.Append(output);
		output.position		=pos1+vec4(g_positions[1].xy*sz,0,0); 
		output.texCoords	=g_texcoords[1];
		SpriteStream.Append(output);
		output.position		=pos1+vec4(g_positions[2].xy*sz,0,0); 
		output.texCoords	=g_texcoords[2];
		SpriteStream.Append(output);
		output.position		=pos2+vec4(g_positions[1].xy*sz,0,0);  
		output.texCoords	=g_texcoords[1];
		SpriteStream.Append(output);
		output.position		=pos2+vec4(g_positions[2].xy*sz,0,0); 
		output.texCoords	=g_texcoords[2];
		SpriteStream.Append(output);
		output.position		=pos2+vec4(g_positions[3].xy*sz,0,0); 
		output.texCoords	=g_texcoords[3];
		SpriteStream.Append(output);
	}
	else
	{
		// bottom-left quadrant:
		output.position		=pos2+vec4(g_positions[2].xy*sz,0,0); 
		output.texCoords	=g_texcoords[2];
		SpriteStream.Append(output);
		output.position		=pos2+vec4(g_positions[3].xy*sz,0,0); 
		output.texCoords	=g_texcoords[3];
		SpriteStream.Append(output);
		output.position		=pos2+vec4(g_positions[0].xy*sz,0,0); 
		output.texCoords	=g_texcoords[0];
		SpriteStream.Append(output);
		output.position		=pos1+vec4(g_positions[3].xy*sz,0,0); 
		output.texCoords	=g_texcoords[3];
		SpriteStream.Append(output);
		output.position		=pos1+vec4(g_positions[0].xy*sz,0,0); 
		output.texCoords	=g_texcoords[0];
		SpriteStream.Append(output);
		output.position		=pos1+vec4(g_positions[1].xy*sz,0,0); 
		output.texCoords	=g_texcoords[1];
		SpriteStream.Append(output);
	}
#endif
    SpriteStream.RestartStrip();
}

vec4 PS_Particles(particleGeometryOutput IN): SV_TARGET
{
	vec4 result		=IN.brightness*cubeTexture.Sample(wrapSamplerState,-IN.view);
	vec2 pos		=IN.texCoords*2.0-1.0;
	float radius	=length(pos.xy);
	float angle		=atan2(pos.x,pos.y);
	//float spoke		=fract(angle/pi*3.0)-0.5;
	float opacity	=IN.fade*saturate(1.0-radius);//-spoke*spoke);
	
	return (vec4(result.rgb,opacity));
}

rainVertexOutput VS_FullScreen(idOnly IN)
{
	rainVertexOutput OUT;
	float2 poss[4]=
	{
		{ 1.0,-1.0},
		{ 1.0, 1.0},
		{-1.0,-1.0},
		{-1.0, 1.0},
	};
	OUT.clip_pos	=poss[IN.vertex_id];
	OUT.position	=vec4(OUT.clip_pos,0.0,1.0);
	// Set to far plane
#ifdef REVERSE_DEPTH
	OUT.position.z	=0.0; 
#else
	OUT.position.z	=OUT.position.w; 
#endif
    OUT.texCoords	=0.5*(vec2(1.0,1.0)+vec2(OUT.clip_pos.x,-OUT.clip_pos.y));
//OUT.texCoords	+=0.5*texelOffsets;
	return OUT;
}

#define NUM (1)

float4 PS_RenderRainTexture(rainVertexOutput IN): SV_TARGET
{
	float r=0;
	vec2 t=IN.texCoords.xy;
	for(int i=0;i<NUM;i++)
	{
		r+=saturate(rand(frac(t.xy))-0.97)*12.0;
		t.y+=1.0/64.0;
	}
	r=saturate(r);
	float4 result=vec4(r,r,r,r);
    return result;
}

float4 PS_RenderRandomTexture(rainVertexOutput IN): SV_TARGET
{
	float r=0;
    vec4 result=vec4(rand(IN.texCoords),rand(1.7*IN.texCoords),rand(0.11*IN.texCoords),rand(513.1*IN.texCoords));
	result=result*2.0-vec4(1.0,1.0,1.0,1.0);
    return result;
}

float4 PS_Overlay(rainVertexOutput IN) : SV_TARGET
{
	vec3 view				=normalize(mul(invViewProj[1],vec4(IN.clip_pos.xy,1.0,1.0)).xyz);
	
	vec2 depth_texc			=viewportCoordToTexRegionCoord(IN.texCoords.xy,viewportToTexRegionScaleBias);
	float depth				=texture_clamp(depthTexture,depth_texc).x;
	float dist				=depthToFadeDistance(depth,IN.clip_pos.xy,depthToLinFadeDistParams,tanHalfFov);

	vec3 light				=cubeTexture.Sample(wrapSamplerState,-view).rgb;
	vec4 result				=vec4(light.rgb,0);
	vec2 texc				=vec2(atan2(view.x,view.y)/(2.0*pi)*5.0,tan(view.z*pi/2.0));
	float layer_distance	=nearRainDistance;
	float mult				=2.0;
	float step_range		=layer_distance;
	float br				=1.0-abs(view.z*view.z);
	for(int i=0;i<4;i++)
	{
		vec2 layer_texc		=vec2(texc.x,texc.y-.5*offset[1].z);
		vec4 r				=rainTexture.Sample(wrapSamplerState,layer_texc.xy);
		float a				=(br*(saturate(r.x+intensity-1.0)));
		//a					*=saturate(10000.0*(dist-layer_distance)/step_range);
		texc				*=mult;
		layer_distance		*=mult;
		step_range			*=mult;
		br					*=0.5;
		result.a			+=a;
	}
	result.a=saturate(result.a);
	return result;
}

//--------------------------------------------------------------------------------------------
// draw rain
//--------------------------------------------------------------------------------------------

struct PSSceneIn
{
    float4 pos : SV_Position;
    float3 lightDir   : LIGHT;
    float3 pointLightDir : LIGHT2;
    float3 eyeVec     : EYE;
    float2 tex : TEXTURE0;
    uint type  : TYPE;
    float random : RAND;
};


PosOnly VS_ParticleRain(PosAndId input )
{
	PosOnly p;
	p.position=input.position;
    return p;
}

float g_Near=1.0;
float g_Far=100.0;
bool cullSprite( float3 position, float SpriteSize)
{
    float4 vpos = mul( worldView[1],float4(position,1));
    
    
    if( (vpos.z < (g_Near - SpriteSize )) || ( vpos.z > (g_Far + SpriteSize)) ) 
    {
        return true;
    }
    else 
    {
        float4 ppos = mul( worldViewProj[1],float4(position,1));
			//mul(vpos, proj[1]);
        float wext = ppos.w + SpriteSize;
        if( (ppos.x < -wext) || (ppos.x > wext) ||
            (ppos.y < -wext) || (ppos.y > wext) ) {
            return true;
        }
        else 
        {
            return false;
        }
    }
    
    return false;
}
float g_SpriteSize=0.5;
void GenRainSpriteVertices(float3 worldPos, float3 velVec, float3 eyePos, out float3 outPos[4])
{
    float height = g_SpriteSize/2.0;
    float width = height/10.0;

    velVec = normalize(velVec);
    float3 eyeVec =  - worldPos;
    float3 eyeOnVelVecPlane =  - ((dot(eyeVec, velVec)) * velVec);
    float3 projectedEyeVec = eyeOnVelVecPlane - worldPos;
    float3 sideVec = normalize(cross(projectedEyeVec, velVec));
    
    outPos[0] =  worldPos - (sideVec * 0.5*width);
    outPos[1] = outPos[0] + (sideVec * width);
    outPos[2] = outPos[0] + (velVec * height);
    outPos[3] = outPos[2] + (sideVec * width );
}
    
// GS for rendering rain as point sprites.  Takes a point and turns it into 2 tris.
[maxvertexcount(4)]
void GS_ParticleRain(point PosOnly input[1], inout TriangleStream<PSSceneIn> SpriteStream)
{
    float totalIntensity = 1.0;//g_PointLightIntensity*g_ResponsePointLight + dirLightIntensity*g_ResponseDirLight;
	//if(!cullSprite(input[0].position.xyz,2*g_SpriteSize) && totalIntensity > 0)
    {    
        PSSceneIn output = (PSSceneIn)0;
        //output.type = input[0].Type;
       // output.random = input[0].random;
       
        float3 pos[4];
		vec3 spd=vec3(0,0,0);//input[0].speed
		float g_FrameRate=60.f;
        GenRainSpriteVertices(input[0].position.xyz, spd.xyz/g_FrameRate + meanFallVelocity, viewPos[1], pos);
        
        float3 closestPointLight=vec3(0,0,500);
        float closestDistance	=length(closestPointLight - pos[0]);
        
        output.pos				=mul(  worldViewProj[1],float4(pos[0],1.0));
        output.lightDir			=lightDir;
        output.pointLightDir	=closestPointLight	-pos[0];
        output.eyeVec			=viewPos[1].xyz		-pos[0];
        output.tex				=g_texcoords[0];
        SpriteStream.Append(output);
                
        output.pos				=mul(worldViewProj[1],float4(pos[1],1.0));
        output.lightDir			=lightDir;
        output.pointLightDir	=closestPointLight	-pos[1];
        output.eyeVec			=viewPos[1].xyz		-pos[1];
        output.tex				=g_texcoords[1];
        SpriteStream.Append(output);
        
        output.pos				=mul(worldViewProj[1],float4(pos[2],1.0));
        output.lightDir			=lightDir;
        output.pointLightDir	=closestPointLight	-pos[2];
        output.eyeVec			=viewPos[1].xyz		-pos[2];
        output.tex				=g_texcoords[2];
        SpriteStream.Append(output);
                
        output.pos				=mul(worldViewProj[1],float4(pos[3],1.0));
        output.lightDir			=lightDir;
        output.pointLightDir	=closestPointLight	-pos[3];
        output.eyeVec			=viewPos[1].xyz		-pos[3];
        output.tex				=g_texcoords[3];
        SpriteStream.Append(output);
        
        SpriteStream.RestartStrip();
    }   
}

SamplerState samAniso
{
    Filter = ANISOTROPIC;
    AddressU = Wrap;
    AddressV = Wrap;
};

void rainResponse(PSSceneIn input, float3 lightVector, float lightIntensity, float3 lightColor, float3 eyeVector, bool fallOffFactor, inout float4 rainResponseVal)
{
    float opacity = 0.0;

    float fallOff;
    if(fallOffFactor)
    {  
        float distToLight = length(lightVector);
        fallOff = 1.0/( distToLight * distToLight);
        fallOff = saturate(fallOff);   
    }
    else
    {
		fallOff = 1;
    }

    if(fallOff > 0.01 && lightIntensity > 0.01 )
    {
        float3 dropDir = meanFallVelocity;

        #define MAX_VIDX 4
        #define MAX_HIDX 8
        // Inputs: lightVector, eyeVector, dropDir
        float3 L = normalize(lightVector);
        float3 E = normalize(eyeVector);
        float3 N = normalize(dropDir);
        
        bool is_EpLp_angle_ccw = true;
        float hangle = 0;
        float vangle = abs( (acos(dot(L,N)) * 180/pi) - 90 ); // 0 to 90
        
        {
            float3 Lp = normalize( L - dot(L,N)*N );
            float3 Ep = normalize( E - dot(E,N)*N );
            hangle = acos( dot(Ep,Lp) ) * 180/pi;  // 0 to 180
            hangle = (hangle-10)/20.0;           // -0.5 to 8.5
            is_EpLp_angle_ccw = dot( N, cross(Ep,Lp)) > 0;
        }
        
        if(vangle>=88.0)
        {
            hangle = 0;
            is_EpLp_angle_ccw = true;
        }
                
        vangle = (vangle-10.0)/20.0; // -0.5 to 4.5
        
        // Outputs:
        // verticalLightIndex[1|2] - two indices in the vertical direction
        // t - fraction at which the vangle is between these two indices (for lerp)
        int verticalLightIndex1 = floor(vangle); // 0 to 5
        int verticalLightIndex2 = min(MAX_VIDX, (verticalLightIndex1 + 1) );
        verticalLightIndex1 = max(0, verticalLightIndex1);
        float t = frac(vangle);

        // textureCoordsH[1|2] used in case we need to flip the texture horizontally
        float textureCoordsH1 = input.tex.x;
        float textureCoordsH2 = input.tex.x;
        
        // horizontalLightIndex[1|2] - two indices in the horizontal direction
        // s - fraction at which the hangle is between these two indices (for lerp)
        int horizontalLightIndex1 = 0;
        int horizontalLightIndex2 = 0;
        float s = 0;
        
        s = frac(hangle);
        horizontalLightIndex1 = floor(hangle); // 0 to 8
        horizontalLightIndex2 = horizontalLightIndex1+1;
        if( horizontalLightIndex1 < 0 )
        {
            horizontalLightIndex1 = 0;
            horizontalLightIndex2 = 0;
        }
                   
        if( is_EpLp_angle_ccw )
        {
            if( horizontalLightIndex2 > MAX_HIDX ) 
            {
                horizontalLightIndex2 = MAX_HIDX;
                textureCoordsH2 = 1.0 - textureCoordsH2;
            }
        }
        else
        {
            textureCoordsH1 = 1.0 - textureCoordsH1;
            if( horizontalLightIndex2 > MAX_HIDX ) 
            {
                horizontalLightIndex2 = MAX_HIDX;
            }
			else 
            {
                textureCoordsH2 = 1.0 - textureCoordsH2;
            }
        }
                
        if(verticalLightIndex1 >= MAX_VIDX)
        {
            textureCoordsH2 = input.tex.x;
            horizontalLightIndex1 = 0;
            horizontalLightIndex2 = 0;
            s = 0;
        }
        
        // Generate the final texture coordinates for each sample
        uint type = input.type;
        uint2 texIndicesV1 = uint2(verticalLightIndex1*90 + horizontalLightIndex1*10 + type,
                                     verticalLightIndex1*90 + horizontalLightIndex2*10 + type);
        float3 tex1 = float3(textureCoordsH1, input.tex.y, texIndicesV1.x);
        float3 tex2 = float3(textureCoordsH2, input.tex.y, texIndicesV1.y);
        if( (verticalLightIndex1<4) && (verticalLightIndex2>=4) ) 
        {
            s = 0;
            horizontalLightIndex1 = 0;
            horizontalLightIndex2 = 0;
            textureCoordsH1 = input.tex.x;
            textureCoordsH2 = input.tex.x;
        }
        
        uint2 texIndicesV2 = uint2(verticalLightIndex2*90 + horizontalLightIndex1*10 + type,
                                     verticalLightIndex2*90 + horizontalLightIndex2*10 + type);
        float3 tex3 = float3(textureCoordsH1, input.tex.y, texIndicesV2.x);        
        float3 tex4 = float3(textureCoordsH2, input.tex.y, texIndicesV2.y);

        // Sample opacity from the textures
        float col1 = rainTextureArray.Sample( samAniso, tex1);//* g_rainfactors[texIndicesV1.x];
        float col2 = rainTextureArray.Sample( samAniso, tex2);//* g_rainfactors[texIndicesV1.y];
        float col3 = rainTextureArray.Sample( samAniso, tex3);//* g_rainfactors[texIndicesV2.x];
        float col4 = rainTextureArray.Sample( samAniso, tex4);//* g_rainfactors[texIndicesV2.y];

        // Compute interpolated opacity using the s and t factors
        float hOpacity1 = lerp(col1,col2,s);
        float hOpacity2 = lerp(col3,col4,s);
        opacity = lerp(hOpacity1,hOpacity2,t);
        opacity = pow(opacity,0.7); // inverse gamma correction (expand dynamic range)
        opacity = 4*lightIntensity * opacity * fallOff;
    }
         
   rainResponseVal = float4(lightColor,opacity);

}

vec4 PS_ParticleRain(PSSceneIn input) : SV_Target
{     
      //return float4(1,0,0,0.1);
       
      //directional lighting---------------------------------------------------------------------------------
      vec4 directionalLight=vec4(1,1,1,1);
      //rainResponse(input, input.lightDir, 2.0*dirLightIntensity*g_ResponseDirLight*input.random, float3(1.0,1.0,1.0), input.eyeVec, false, directionalLight);

      //point lighting---------------------------------------------------------------------------------------
      vec4 pointLight = vec4(0,0,0,0);
     /* 
      vec3 L = normalize( input.pointLightDir );
      float angleToSpotLight = dot(-L, g_SpotLightDir);
      
      if( !g_useSpotLight || g_useSpotLight && angleToSpotLight > g_cosSpotlightAngle )
          rainResponse(input, input.pointLightDir, 2*g_PointLightIntensity*g_ResponsePointLight*input.random, pointLightColor.xyz, input.eyeVec, true,pointLight);
      */
      float totalOpacity = pointLight.a+directionalLight.a;
      return vec4( vec3(pointLight.rgb*pointLight.a/totalOpacity + directionalLight.rgb*directionalLight.a/totalOpacity), totalOpacity);
}

technique11 rain_particles
{
    pass p0
    {
        SetRasterizerState( RenderNoCull );
        SetVertexShader( CompileShader(   vs_4_0, VS_ParticleRain() ) );
        SetGeometryShader( CompileShader( gs_4_0, GS_ParticleRain() ) );
        SetPixelShader( CompileShader(    ps_4_0, PS_ParticleRain() ) );
        
        SetBlendState( AlphaBlend, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetDepthStencilState( DisableDepth, 0 );
    }  
}

technique11 simul_particles
{
    pass p0 
    {
		SetRasterizerState(RenderNoCull);
		//SetRasterizerState( wireframeRasterizer );
        SetGeometryShader(CompileShader(gs_4_0,GS_Particles()));
		SetVertexShader(CompileShader(vs_4_0,VS_Particles()));
		SetPixelShader(CompileShader(ps_4_0,PS_Particles()));
		SetDepthStencilState(DisableDepth,0);
		SetBlendState(AddAlphaBlend,float4(0.0f,0.0f,0.0f,0.0f),0xFFFFFFFF);
    }
}

technique11 simul_rain
{
    pass p0 
    {
		SetRasterizerState(RenderNoCull);
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,VS_FullScreen()));
		SetPixelShader(CompileShader(ps_4_0,PS_Overlay()));
		SetDepthStencilState(DisableDepth,0);
		SetBlendState(AddAlphaBlend,float4(0.0f,0.0f,0.0f,0.0f),0xFFFFFFFF);
    }
}

technique11 make_vertex_buffer
{
    pass p0 
    {
		SetComputeShader(CompileShader(cs_5_0,CS_MakeVertexBuffer()));
    }
}

technique11 move_particles_compute
{
    pass p0 
    {
		SetComputeShader(CompileShader(cs_5_0,CS_MoveParticles()));
    }
}

technique11 create_rain_texture
{
    pass p0 
    {
		SetRasterizerState( RenderNoCull );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,VS_FullScreen()));
		SetPixelShader(CompileShader(ps_4_0,PS_RenderRainTexture()));
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(DoBlend, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
    }
}

technique11 create_random_texture
{
    pass p0 
    {
		SetRasterizerState( RenderNoCull );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,VS_FullScreen()));
		SetPixelShader(CompileShader(ps_4_0,PS_RenderRandomTexture()));
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(DoBlend, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
    }
}


technique11 show_texture
{
    pass p0 
    {
		SetRasterizerState( RenderNoCull );
        SetGeometryShader(NULL);
		SetVertexShader(CompileShader(vs_4_0,VS_ShowTexture()));
		SetPixelShader(CompileShader(ps_4_0,PS_ShowTexture()));
		SetDepthStencilState( DisableDepth, 0 );
		SetBlendState(DontBlend, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
    }
}

//--------------------------------------------------------------------------------------------
// advance rain
//--------------------------------------------------------------------------------------------

VSParticleIn VS_MoveParticles(PosOnly input)
{
     //if(moveParticles)
     {
         //move forward
         //input.pos.xyz += input.speed.xyz/g_FrameRate + g_TotalVel.xyz;
		 input.position.z-=.01;//
         //if the particle is outside the bounds, move it to random position near the eye         
         if(input.position.z <=-particleZoneSize)//  g_eyePos.y-g_heightRange )
         {
            //float x =/*input.seed.x*/+ g_eyePos.x;
            //float z =/*input.seed.z*/+ g_eyePos.z;
            //float y =/*input.seed.y*/+ g_eyePos.y;
            //input.pos = float3(x,y,z);
			 input.position.z+=2.0*particleZoneSize;
         }
    }

    return input;
    
}

GeometryShader gsStreamOut = ConstructGSWithSO( CompileShader( vs_4_0, VS_MoveParticles() ), "POSITION.xyz");//; SEED.xyz; SPEED.xyz; RAND.x; TYPE.x" );

technique11 move_particles
{
    pass p0
    {
        SetVertexShader( CompileShader( vs_4_0, VS_MoveParticles() ) );
        SetGeometryShader( gsStreamOut );
        SetPixelShader( NULL );
        
        SetDepthStencilState( DisableDepth, 0 );
    }  
}