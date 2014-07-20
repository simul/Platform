#include "RenderPlatform.h"
#include "Effect.h"

using namespace simul;
using namespace crossplatform;

void RenderPlatform::EnsureEffectIsBuiltPartialSpec(const char *filename_utf8,const std::vector<crossplatform::EffectDefineOptions> &options,const std::map<std::string,std::string> &defines)
{
	if(options.size())
	{
		std::vector<crossplatform::EffectDefineOptions> opts=options;
		opts.pop_back();
		crossplatform::EffectDefineOptions opt=options.back();
		for(int i=0;i<(int)opt.options.size();i++)
		{
			std::map<std::string,std::string> defs=defines;
			defs[opt.name]=opt.options[i];
			EnsureEffectIsBuiltPartialSpec(filename_utf8,opts,defs);
		}
	}
	else
	{
		crossplatform::Effect *e=CreateEffect(filename_utf8,defines);
		delete e;
	}
}

Effect *RenderPlatform::CreateEffect(const char *filename_utf8)
{
	std::map<std::string,std::string> defines;
	return CreateEffect(filename_utf8,defines);
}

void RenderPlatform::EnsureEffectIsBuilt(const char *filename_utf8,const std::vector<crossplatform::EffectDefineOptions> &opts)
{
	const std::map<std::string,std::string> defines;
	EnsureEffectIsBuiltPartialSpec(filename_utf8,opts,defines);
}

#include "Simul/Camera/Camera.h"
#include "Simul/Platform/CrossPlatform/DeviceContext.h"
namespace simul
{
	namespace crossplatform
	{
		void DrawGrid(crossplatform::DeviceContext &deviceContext,float square_size,float brightness,int numLines)
		{
			// 101 lines across, 101 along.
			numLines++;
			crossplatform::RenderPlatform::Vertext *lines=new crossplatform::RenderPlatform::Vertext[2*numLines*2];
			// one metre apart
			vec3 cam_pos=camera::GetCameraPosVector(deviceContext.viewStruct.view);
			vec3 centrePos(square_size*(int)(cam_pos.x/square_size),square_size*(int)(cam_pos.y/square_size),0);
			crossplatform::RenderPlatform::Vertext *vertex=lines;
			int halfOffset=numLines/2;
			float l10=brightness;
			float l5=brightness*0.5;
			float l25=brightness*2.5;
			for(int i=0;i<numLines;i++)
			{
				int j=i-numLines/2;
				vec3 pos1		=centrePos+vec3(square_size*j	,-square_size*halfOffset	,0);
				vec3 pos2		=centrePos+vec3(square_size*j	, square_size*halfOffset	,0);
				vertex->pos		=pos1;
				vertex->colour	=vec4(l5,l5,0.f,1.0f);
				vertex++;
				vertex->pos		=pos2;
				vertex->colour	=vec4(l5,0.1*l5,0.f,1.0f);
				vertex++;
			}
			for(int i=0;i<numLines;i++)
			{
				int j=i-numLines/2;
				vec3 pos1		=centrePos+vec3(-square_size*halfOffset	,square_size*j	,0);
				vec3 pos2		=centrePos+vec3( square_size*halfOffset	,square_size*j	,0);
				vertex->pos		=pos1;
				vertex->colour	=vec4(0.f,l10,l5,1.0f);
				vertex++;
				vertex->pos		=pos2;
				vertex->colour	=vec4(0.f,l25,l5,1.0f);
				vertex++;
			}
			deviceContext.renderPlatform->DrawLines(deviceContext,lines,2*numLines*2,false,true);
			delete[] lines;
		}
	}
}