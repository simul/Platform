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
		void DrawGrid(crossplatform::DeviceContext &deviceContext)
		{
			// 101 lines across, 101 along.
			static int numLines=101;
			crossplatform::RenderPlatform::Vertext *lines=new crossplatform::RenderPlatform::Vertext[2*numLines*2];
			// one metre apart
			static float d=1.f;
			vec3 cam_pos=camera::GetCameraPosVector(deviceContext.viewStruct.view);
			vec3 centrePos((int)(cam_pos.x/d),(int)(cam_pos.y/d),0);
			crossplatform::RenderPlatform::Vertext *vertex=lines;
			int halfOffset=numLines/2;
			for(int i=0;i<numLines;i++)
			{
				int j=i-numLines/2;
				math::Vector3 pos1	=centrePos+vec3(d*j	,-d*halfOffset	,0);
				math::Vector3 pos2	=centrePos+vec3(d*j	, d*halfOffset	,0);
				vertex->pos		=pos1;
				vertex->colour	=vec4(5.f,5.f,0.f,1.0f);
				vertex++;
				vertex->pos		=pos2;
				vertex->colour	=vec4(5.f,0.5f,0.f,1.0f);
				vertex++;
			}
			for(int i=0;i<numLines;i++)
			{
				int j=i-numLines/2;
				math::Vector3 pos1	=centrePos+vec3(-d*halfOffset	,d*j	,0);
				math::Vector3 pos2	=centrePos+vec3( d*halfOffset	,d*j	,0);
				vertex->pos		=pos1;
				vertex->colour	=vec4(0.f,1.f,5.f,1.0f);
				vertex++;
				vertex->pos		=pos2;
				vertex->colour	=vec4(0.f,2.5f,5.f,1.0f);
				vertex++;
			}
			deviceContext.renderPlatform->DrawLines(deviceContext,lines,2*numLines*2,false);
			delete[] lines;
		}
	}
}