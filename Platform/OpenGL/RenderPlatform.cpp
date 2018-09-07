#include "Simul/Platform/OpenGL/RenderPlatform.h"
#include "Simul/Platform/OpenGL/Mesh.h"
#include "Simul/Platform/OpenGL/Texture.h"
#include "Simul/Platform/OpenGL/Effect.h"
#include "Simul/Platform/OpenGL/Light.h"
#include "Simul/Platform/OpenGL/Buffer.h"
#include "Simul/Platform/OpenGL/FramebufferGL.h"
#include "Simul/Platform/OpenGL/Layout.h"
#include "Simul/Platform/CrossPlatform/DeviceContext.h"
#include "Simul/Platform/CrossPlatform/RenderPlatform.h"
#include "Simul/Base/DefaultFileLoader.h"
#include "Simul/Platform/CrossPlatform/Macros.h"
#include "Simul/Platform/CrossPlatform/Texture.h"
#include "Simul/Platform/OpenGL/Texture.h"

using namespace simul;
using namespace opengl;

RenderPlatform::RenderPlatform():
    mNullVAO(0)
	,mDummy2D(nullptr)
    ,mDummy3D(nullptr)
{
    mirrorY     = true;
    mirrorY2    = true;
    mirrorYText = true;

    mCachedState.Vao            = 0;
    mCachedState.Program        = 0;
    mCachedState.Framebuffer    = 0;
}

RenderPlatform::~RenderPlatform()
{
	InvalidateDeviceObjects();
}

const char* RenderPlatform::GetName()const
{
    return "OpenGL";
}

void RenderPlatform::RestoreDeviceObjects(void* unused)
{
    if (!gladLoadGL())
    {
        std::cout << "[ERROR] Could not initialize glad.\n";
        return;
    }

    // Generate and bind a dummy vao:
    glGenVertexArrays(1, &mNullVAO);
    glBindVertexArray(mNullVAO);

    // Query limits:
    glGetIntegerv(GL_MAX_VIEWPORTS, &mMaxViewports);
    glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS, &mMaxColorAttatch);

    // Lets configure OpenGL:
    //  glClipControl() needs OpenGL >= 4.5
    glClipControl(GL_UPPER_LEFT, GL_ZERO_TO_ONE);
    

    crossplatform::RenderPlatform::RestoreDeviceObjects(nullptr);
    RecompileShaders();
}

void RenderPlatform::InvalidateDeviceObjects()
{
    // glDeleteVertexArrays(1, &mNullVAO);
}

void RenderPlatform::StartRender(crossplatform::DeviceContext &deviceContext)
{
}

void RenderPlatform::EndRender(crossplatform::DeviceContext &deviceContext)
{
}

void RenderPlatform::BeginEvent(crossplatform::DeviceContext& deviceContext, const char* name)
{
    glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 123, -1, name);
}

void RenderPlatform::EndEvent(crossplatform::DeviceContext& deviceContext)
{
    glPopDebugGroup();
}

void RenderPlatform::StoreGLState()
{
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING,      &mCachedState.Vao);
    glGetIntegerv(GL_CURRENT_PROGRAM,           &mCachedState.Program);
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,  &mCachedState.Framebuffer); //draw, read col0,1 etc -_-

    // TO-DO: blend,depth,raster

    // Lets bind our dummy vao
    glBindVertexArray(mNullVAO);
}

void RenderPlatform::RestoreGLState()
{
    glBindVertexArray(mCachedState.Vao);
    glUseProgram(mCachedState.Program);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, mCachedState.Framebuffer);
}

void RenderPlatform::DispatchCompute(crossplatform::DeviceContext &deviceContext,int w,int l,int d)
{
    BeginEvent(deviceContext, ((opengl::EffectPass*)deviceContext.contextState.currentEffectPass)->PassName.c_str());
    ApplyCurrentPass(deviceContext);
    glDispatchCompute(w, l, d);
    InsertFences(deviceContext);
    EndEvent(deviceContext);
}

void RenderPlatform::DrawLine(crossplatform::DeviceContext &,const double *pGlobalBasePosition, const double *pGlobalEndPosition,const float *colour,float width)
{
}

void RenderPlatform::DrawLineLoop(crossplatform::DeviceContext &,const double *mat,int lVerticeCount,const double *vertexArray,const float colr[4])
{
}

void RenderPlatform::DrawTexture(crossplatform::DeviceContext &deviceContext, int x1, int y1, int dx, int dy, crossplatform::Texture *tex, vec4 mult, bool blend, float gamma, bool debug)
{
    crossplatform::RenderPlatform::DrawTexture(deviceContext, x1, y1, dx, dy, tex, mult, blend,gamma);
}

void RenderPlatform::DrawQuad(crossplatform::DeviceContext& deviceContext)   
{
    BeginEvent(deviceContext, ((opengl::EffectPass*)deviceContext.contextState.currentEffectPass)->PassName.c_str());
    ApplyCurrentPass(deviceContext);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    EndEvent(deviceContext);
}

void RenderPlatform::ApplyCurrentPass(crossplatform::DeviceContext & deviceContext)
{
    crossplatform::ContextState* cs = &deviceContext.contextState;
    opengl::EffectPass* pass    = (opengl::EffectPass*)cs->currentEffectPass;
    
    pass->SetTextureHandles(deviceContext);
}

void RenderPlatform::InsertFences(crossplatform::DeviceContext& deviceContext)
{
    auto pass = (opengl::EffectPass*)deviceContext.contextState.currentEffectPass;

    //if (pass->usesRwSBs())
    //{
    //    for (int i = 0; i < pass->numRwSbResourceSlots; i++)
    //    {
    //        int slot    = pass->rwSbResourceSlots[i];
    //        auto rwsb   = (opengl::PlatformStructuredBuffer*)deviceContext.contextState.applyRwStructuredBuffers[slot];
    //        if (rwsb && pass->usesRwTextureSlotForSB(slot))
    //        {
    //            rwsb->AddFence(deviceContext);
    //        }
    //    }
    //}
}

crossplatform::Mesh* RenderPlatform::CreateMesh()
{
	return new opengl::Mesh;
}

crossplatform::Light* RenderPlatform::CreateLight()
{
	return new opengl::Light();
}

crossplatform::Texture* RenderPlatform::CreateTexture(const char *fileNameUtf8)
{
	crossplatform::Texture* tex= new opengl::Texture;
    if (fileNameUtf8 && strlen(fileNameUtf8) > 0 && strcmp(fileNameUtf8, "ESRAM") != 0)
	{
		std::string str(fileNameUtf8);
        if (str.find(".") < str.length())
        {
            tex->LoadFromFile(this, fileNameUtf8);
        }
	}
    tex->SetName(fileNameUtf8);
	return tex;
}

crossplatform::BaseFramebuffer* RenderPlatform::CreateFramebuffer(const char *n)
{
	opengl::FramebufferGL* b=new opengl::FramebufferGL(n);
	return b;
}

GLenum simul::opengl::RenderPlatform::toGLMinFiltering(crossplatform::SamplerStateDesc::Filtering f)
{
    if (f == simul::crossplatform::SamplerStateDesc::LINEAR)
    {
        return GL_LINEAR_MIPMAP_LINEAR;
    }
    return GL_NEAREST_MIPMAP_NEAREST;
}

GLenum simul::opengl::RenderPlatform::toGLMaxFiltering(crossplatform::SamplerStateDesc::Filtering f)
{
    if (f == simul::crossplatform::SamplerStateDesc::LINEAR)
    {
        return GL_LINEAR;
    }
    return GL_NEAREST;
}

GLint RenderPlatform::toGLWrapping(crossplatform::SamplerStateDesc::Wrapping w)
{
	switch(w)
	{
	case crossplatform::SamplerStateDesc::WRAP:
		return GL_REPEAT;
		break;
	case crossplatform::SamplerStateDesc::CLAMP:
		return GL_CLAMP_TO_EDGE;
		break;
	case crossplatform::SamplerStateDesc::MIRROR:
		return GL_MIRRORED_REPEAT;
		break;
	}
	return GL_REPEAT;
}

crossplatform::SamplerState* RenderPlatform::CreateSamplerState(crossplatform::SamplerStateDesc* desc)
{
	opengl::SamplerState* s = new opengl::SamplerState();
    s->Init(desc);
	return s;
}

crossplatform::Effect* RenderPlatform::CreateEffect()
{
	opengl::Effect* e=new opengl::Effect();
	return e;
}

crossplatform::Effect* RenderPlatform::CreateEffect(const char *filename_utf8,const std::map<std::string,std::string> &defines)
{
	crossplatform::Effect* e=crossplatform::RenderPlatform::CreateEffect(filename_utf8,defines);
	return e;
}

crossplatform::PlatformConstantBuffer* RenderPlatform::CreatePlatformConstantBuffer()
{
	return new opengl::PlatformConstantBuffer();
}

crossplatform::PlatformStructuredBuffer* RenderPlatform::CreatePlatformStructuredBuffer()
{
	crossplatform::PlatformStructuredBuffer* b=new opengl::PlatformStructuredBuffer();
	return b;
}

crossplatform::Buffer* RenderPlatform::CreateBuffer()
{
	return new opengl::Buffer();
}

GLuint RenderPlatform::ToGLFormat(crossplatform::PixelFormat p)
{
	using namespace crossplatform;
	switch(p)
	{
    case RGB_11_11_10_FLOAT:
        return GL_R11F_G11F_B10F;
	case RGBA_16_FLOAT:
		return GL_RGBA16F;
	case RGBA_32_FLOAT:
		return GL_RGBA32F;
	case RGBA_32_UINT:
		return GL_RGBA32UI;
	case RGB_32_FLOAT:
		return GL_RGB32F;
	case RG_32_FLOAT:
		return GL_RG32F;
	case R_32_FLOAT:
		return GL_R32F;
	case R_16_FLOAT:
		return GL_R16F;
	case LUM_32_FLOAT:
		return GL_LUMINANCE32F_ARB;
	case INT_32_FLOAT:
		return GL_INTENSITY32F_ARB;
	case RGBA_8_UNORM:
		return GL_RGBA8;
	case RGBA_8_UNORM_SRGB:
		return GL_SRGB8_ALPHA8;
	case RGBA_8_SNORM:
		return GL_RGBA8_SNORM;
	case RGB_8_UNORM:
		return GL_RGB8;
	case RGB_8_SNORM:
		return GL_RGB8_SNORM;
	case R_8_UNORM:
		return GL_R8;
	case R_32_UINT:
		return GL_R32UI;
	case RG_32_UINT:
		return GL_RG32UI;
	case RGB_32_UINT:
		return GL_RGB32UI;
	case D_32_FLOAT:
		return GL_DEPTH_COMPONENT32F;
	case D_24_UNORM_S_8_UINT:
		return GL_DEPTH_COMPONENT24;
	case D_16_UNORM:
		return GL_DEPTH_COMPONENT16;
	default:
		return 0;
	};
}

crossplatform::PixelFormat RenderPlatform::FromGLFormat(GLuint p)
{
	using namespace crossplatform;
	switch(p)
	{
		case GL_RGBA16F:
			return RGBA_16_FLOAT;
		case GL_RGBA32F:
			return RGBA_32_FLOAT;
		case GL_RGBA32UI:
			return RGBA_32_UINT;
		case GL_RGB32F:
			return RGB_32_FLOAT;
		case GL_RG32F:
			return RG_32_FLOAT;
		case GL_R32F:
			return R_32_FLOAT;
		case GL_R16F:
			return R_16_FLOAT;
		case GL_LUMINANCE32F_ARB:
			return LUM_32_FLOAT;
		case GL_INTENSITY32F_ARB:
			return INT_32_FLOAT;
		case GL_RGBA8:
			return RGBA_8_UNORM;
		case GL_SRGB8_ALPHA8:
			return RGBA_8_UNORM_SRGB;
		case GL_RGBA8_SNORM:
			return RGBA_8_SNORM;
		case GL_RGB8:
			return RGB_8_UNORM;
		case GL_RGB8_SNORM:
			return RGB_8_SNORM;
		case GL_R8:
			return R_8_UNORM;
		case GL_R32UI:
			return R_32_UINT;
		case GL_RG32UI:
			return RG_32_UINT;
		case GL_RGB32UI:
			return RGB_32_UINT;
		case GL_DEPTH_COMPONENT32F:
			return D_32_FLOAT;
		case GL_DEPTH_COMPONENT24:
			return D_24_UNORM_S_8_UINT;
		case GL_DEPTH_COMPONENT16:
			return D_16_UNORM;
	default:
		return UNKNOWN;
	};
}

GLuint RenderPlatform::ToGLExternalFormat(crossplatform::PixelFormat p)
{
	using namespace crossplatform;
	switch(p)
	{
    case RGB_11_11_10_FLOAT:
        return GL_RGB;
	case RGBA_16_FLOAT:
		return GL_RGBA;
	case RGBA_32_FLOAT:
		return GL_RGBA;
	case RGB_32_FLOAT:
		return GL_RGB;
	case RGBA_32_UINT:
		return GL_RGBA_INTEGER;
	case R_32_UINT:
		return GL_RED_INTEGER;
	case RG_32_UINT:
		return GL_RG_INTEGER;
	case RGB_32_UINT:
		return GL_RGB_INTEGER;
	case RG_32_FLOAT:
		return GL_RG;
	case R_16_FLOAT:
		return GL_RED;
	case R_32_FLOAT:
		return GL_RED;
	case LUM_32_FLOAT:
		return GL_RGBA;
	case INT_32_FLOAT:
		return GL_RGBA;
	case RGBA_8_UNORM:
		return GL_RGBA;
	case RGBA_8_UNORM_SRGB:
		return GL_SRGB8_ALPHA8;
	case RGBA_8_SNORM:
		return GL_RGBA;
	case RGB_8_UNORM:
		return GL_RGB;
	case RGB_8_SNORM:
		return GL_RGB;
	case R_8_UNORM:
		return GL_RED;// not GL_R...!
	case D_32_FLOAT:
		return GL_DEPTH_COMPONENT;
	case D_24_UNORM_S_8_UINT:
		return GL_DEPTH_COMPONENT24;
	case D_16_UNORM:
		return GL_DEPTH_COMPONENT;
	default:
		return GL_RGBA;
	};
}

int RenderPlatform::FormatCount(crossplatform::PixelFormat p)
{
	using namespace crossplatform;
	switch(p)
	{
    case RGB_11_11_10_FLOAT:
        return 3;
	case RGBA_16_FLOAT:
		return 4;
	case RGBA_32_FLOAT:
		return 4;
	case RGB_32_FLOAT:
		return 3;
	case RG_32_FLOAT:
		return 2;
	case R_32_FLOAT:
		return 1;
	case R_16_FLOAT:
		return 1;
	case LUM_32_FLOAT:
		return 1;
	case INT_32_FLOAT:
		return 1;
	case RGBA_8_UNORM:
	case RGBA_8_UNORM_SRGB:
	case RGBA_8_SNORM:
		return 4;
	case RGB_8_UNORM:
	case RGB_8_SNORM:
		return 3;
	case R_8_UNORM:
	case R_8_SNORM:
	case R_32_UINT:
		return 1;
	case RG_32_UINT:
		return 2;
	case RGB_32_UINT:
		return 3;
	case RGBA_32_UINT:
		return 4;
	case D_32_FLOAT:
		return 1;
	case D_16_UNORM:
		return 1;
	case D_24_UNORM_S_8_UINT:
		return 3;
	default:
		return 0;
	};
}

void RenderPlatform::MakeTextureResident(GLuint64 handle)
{
    if (mResidentTextures.find(handle) == mResidentTextures.end())
    {
        mResidentTextures.insert(handle);
        glMakeTextureHandleResidentARB(handle);
    }
}

const float whiteTexel[4] = { 1.0f,1.0f,1.0f,1.0f};

opengl::Texture* RenderPlatform::GetDummy2D()
{
    if (!mDummy2D)
    {
        mDummy2D = (opengl::Texture*)CreateTexture("dummy2d");
        mDummy2D->ensureTexture2DSizeAndFormat(this, 1, 1, crossplatform::PixelFormat::RGBA_8_UNORM);
        mDummy2D->setTexels(immediateContext, &whiteTexel[0], 0, 1);
    }
    return mDummy2D;
}

opengl::Texture* RenderPlatform::GetDummy3D()
{
    if (!mDummy3D)
    {
        mDummy3D = (opengl::Texture*)CreateTexture("dummy3d");
        mDummy3D->ensureTexture3DSizeAndFormat(this, 1, 1,1, crossplatform::PixelFormat::RGBA_8_UNORM);
        mDummy3D->setTexels(immediateContext, &whiteTexel[0], 0, 1);
    }
    return mDummy3D;
}

GLenum RenderPlatform::DataType(crossplatform::PixelFormat p)
{
	using namespace crossplatform;
	switch(p)
	{
    case RGB_11_11_10_FLOAT:
        return GL_FLOAT;
	case RGBA_16_FLOAT:
		return GL_FLOAT;
	case RGBA_32_FLOAT:
		return GL_FLOAT;
	case RGB_32_FLOAT:
		return GL_FLOAT;
	case RG_32_FLOAT:
		return GL_FLOAT;
	case R_32_FLOAT:
		return GL_FLOAT;
	case R_16_FLOAT:
		return GL_FLOAT;
	case LUM_32_FLOAT:
		return GL_FLOAT;
	case INT_32_FLOAT:
		return GL_FLOAT;
	case RGBA_8_UNORM_SRGB:
	case RGBA_8_UNORM:
		return GL_UNSIGNED_INT;
	case RGBA_8_SNORM:
		return GL_UNSIGNED_INT;
	case RGB_8_UNORM:
		return GL_UNSIGNED_INT;
	case RGB_8_SNORM:
		return GL_UNSIGNED_INT;
	case R_8_UNORM:
		return GL_UNSIGNED_BYTE;
	case R_8_SNORM:
		return GL_UNSIGNED_BYTE;
	case R_32_UINT:
		return GL_UNSIGNED_INT;
	case RG_32_UINT:
		return GL_UNSIGNED_INT;
	case RGB_32_UINT:
		return GL_UNSIGNED_INT;
	case RGBA_32_UINT:
		return GL_UNSIGNED_INT;
	case D_32_FLOAT:
		return GL_FLOAT;
	case D_16_UNORM:
		return GL_UNSIGNED_SHORT;
	case D_24_UNORM_S_8_UINT:
		return GL_UNSIGNED_INT_24_8;
	default:
		return 0;
	};
}

crossplatform::Layout* RenderPlatform::CreateLayout(int num_elements,const crossplatform::LayoutDesc *desc)
{
    opengl::Layout* l = new opengl::Layout();
    l->SetDesc(desc, num_elements);
	return l;
}

crossplatform::RenderState* RenderPlatform::CreateRenderState(const crossplatform::RenderStateDesc &desc)
{
	opengl::RenderState* s  = new opengl::RenderState;
	s->desc                 = desc;
	s->type                 = desc.type;
	return s;
}

crossplatform::Query* RenderPlatform::CreateQuery(crossplatform::QueryType type)
{
	opengl::Query* q=new opengl::Query(type);
	return q;
}

static GLenum toGlCullFace(crossplatform::CullFaceMode c)
{
    switch (c)
    {
    case simul::crossplatform::CULL_FACE_FRONT:
        return GL_FRONT;
    case simul::crossplatform::CULL_FACE_BACK:
        return GL_BACK;
    case simul::crossplatform::CULL_FACE_FRONTANDBACK:
        return GL_FRONT_AND_BACK;
    default:
        break;
    }
    return GL_FRONT;
}

static GLenum toGlFun(crossplatform::BlendOperation o)
{
    switch (o)
    {
    case simul::crossplatform::BLEND_OP_ADD:
        return GL_FUNC_ADD;
    case simul::crossplatform::BLEND_OP_SUBTRACT:
        return GL_FUNC_SUBTRACT;
    case simul::crossplatform::BLEND_OP_MAX:
        return GL_MAX;
    case simul::crossplatform::BLEND_OP_MIN:
        return GL_MIN;
    default:
        break;
    }
    return GL_FUNC_ADD;
}

static GLenum toGlComparison(crossplatform::DepthComparison d)
{
	switch(d)
	{
	case crossplatform::DEPTH_LESS:
		return GL_LESS;
	case crossplatform::DEPTH_EQUAL:
		return GL_EQUAL;
	case crossplatform::DEPTH_LESS_EQUAL:
		return GL_LEQUAL;
	case crossplatform::DEPTH_GREATER:
		return GL_GREATER;
	case crossplatform::DEPTH_NOT_EQUAL:
		return GL_NOTEQUAL;
	case crossplatform::DEPTH_GREATER_EQUAL:
		return GL_GEQUAL;
	default:
		break;
	};
	return GL_LESS;
}

static GLenum toGlBlendOp(crossplatform::BlendOption o)
{
	switch(o)
	{
	case crossplatform::BLEND_ZERO:
		return GL_ZERO;
	case crossplatform::BLEND_ONE:
		return GL_ONE;
	case crossplatform::BLEND_SRC_COLOR:
		return GL_SRC_COLOR;
	case crossplatform::BLEND_INV_SRC_COLOR:
		return GL_ONE_MINUS_SRC_COLOR;
	case crossplatform::BLEND_SRC_ALPHA:
		return GL_SRC_ALPHA;
	case crossplatform::BLEND_INV_SRC_ALPHA:
		return GL_ONE_MINUS_SRC_ALPHA;
	case crossplatform::BLEND_DEST_ALPHA:
		return GL_DST_ALPHA;
	case crossplatform::BLEND_INV_DEST_ALPHA:
		return GL_ONE_MINUS_DST_ALPHA;
	case crossplatform::BLEND_DEST_COLOR:
		return GL_DST_COLOR;
	case crossplatform::BLEND_INV_DEST_COLOR:
		return GL_ONE_MINUS_DST_COLOR;
	case crossplatform::BLEND_SRC_ALPHA_SAT:
		return 0;
	case crossplatform::BLEND_BLEND_FACTOR:
		return 0;
	case crossplatform::BLEND_INV_BLEND_FACTOR:
		return 0;
	case crossplatform::BLEND_SRC1_COLOR:
		return GL_SRC1_COLOR;
	case crossplatform::BLEND_INV_SRC1_COLOR:
		return GL_ONE_MINUS_SRC1_COLOR;
	case crossplatform::BLEND_SRC1_ALPHA:
		return GL_SRC1_ALPHA;
	case crossplatform::BLEND_INV_SRC1_ALPHA:
		return GL_ONE_MINUS_SRC1_ALPHA;
	default:
		break;
	};
	return GL_ONE;
}

void RenderPlatform::SetRenderState(crossplatform::DeviceContext& deviceContext,const crossplatform::RenderState* s)
{
    auto state = (opengl::RenderState*)s;
    if (state->type == crossplatform::RenderStateType::BLEND)
    {
        crossplatform::BlendDesc bdesc = state->desc.blend;
        // We need to iterate over all the rts as we may have some settings
        // from older passes:
        const int kBlendMaxRt = 8;
        for (int i = 0; i < kBlendMaxRt; i++)
        {
            if (i >= bdesc.numRTs || bdesc.RenderTarget[i].blendOperation == crossplatform::BlendOperation::BLEND_OP_NONE)
            {
                glDisablei(GL_BLEND, i);
            }
            else
            {
                glEnablei(GL_BLEND, i);
                glBlendEquationSeparate(toGlFun(bdesc.RenderTarget[i].blendOperation), toGlFun(bdesc.RenderTarget[i].blendOperationAlpha));
                glBlendFuncSeparate
                (
                    toGlBlendOp(bdesc.RenderTarget[i].SrcBlend), toGlBlendOp(bdesc.RenderTarget[i].DestBlend),
                    toGlBlendOp(bdesc.RenderTarget[i].SrcBlendAlpha), toGlBlendOp(bdesc.RenderTarget[i].DestBlendAlpha)
                );
                unsigned char msk = bdesc.RenderTarget[i].RenderTargetWriteMask;
                glColorMaski
                (
                    i, 
                    (GLboolean)(msk & (1 << 0)), 
                    (GLboolean)(msk & (1 << 1)), 
                    (GLboolean)(msk & (1 << 2)), 
                    (GLboolean)(msk & (1 << 3))
                );
            }
        }
    }
    else if (state->type == crossplatform::RenderStateType::DEPTH)
    {
        crossplatform::DepthStencilDesc ddesc = state->desc.depth;
        glDepthMask((GLboolean)ddesc.write);
        if (ddesc.test)
        {
            glEnable(GL_DEPTH_TEST);
            glDepthFunc(toGlComparison(ddesc.comparison));
        }
        else
        {
            glDisable(GL_DEPTH_TEST);
        }
    }
    else if (state->type == crossplatform::RenderStateType::RASTERIZER)
    {
        crossplatform::RasterizerDesc rdesc = state->desc.rasterizer;
        if (rdesc.cullFaceMode == crossplatform::CullFaceMode::CULL_FACE_NONE)
        {
            glDisable(GL_CULL_FACE);
        }
        else
        {
            glEnable(GL_CULL_FACE);
            glCullFace(toGlCullFace(rdesc.cullFaceMode));
        }
        // Reversed
        glFrontFace(rdesc.frontFace == crossplatform::FrontFace::FRONTFACE_CLOCKWISE ? GL_CCW : GL_CW);
    }
    else
    {
        SIMUL_CERR << "Trying to set an invalid render state \n";
    }
}

void RenderPlatform::SetStandardRenderState(crossplatform::DeviceContext& deviceContext, crossplatform::StandardRenderState s)
{
    SetRenderState(deviceContext, standardRenderStates[s]);
}

void RenderPlatform::Resolve(crossplatform::DeviceContext &,crossplatform::Texture *destination,crossplatform::Texture *source)
{
}

void RenderPlatform::SaveTexture(crossplatform::Texture *texture,const char *lFileNameUtf8)
{
}

void* RenderPlatform::GetDevice()
{
	return nullptr;
}

void RenderPlatform::SetVertexBuffers(crossplatform::DeviceContext& deviceContext, int slot, int num_buffers, crossplatform::Buffer* const* buffers, const crossplatform::Layout* layout, const int* vertexSteps)
{
    if (!buffers)
    {
        return;
    }
    for (int i = 0; i < num_buffers; i++)
    {
        opengl::Buffer* glBuffer = (opengl::Buffer*)buffers[i];
        if (glBuffer)
        {
            glBuffer->BindVBO(deviceContext);
        }
    }
}

void RenderPlatform::SetStreamOutTarget(crossplatform::DeviceContext&,crossplatform::Buffer *buffer,int/* start_index*/)
{
}

void RenderPlatform::ActivateRenderTargets(crossplatform::DeviceContext& deviceContext,int num,crossplatform::Texture** targs,crossplatform::Texture* depth)
{
    if (num >= mMaxColorAttatch)
    {
        SIMUL_CERR << "Too many targets \n";
        return;
    }
}
#include <cstdint>
void RenderPlatform::DeactivateRenderTargets(crossplatform::DeviceContext& deviceContext)
{
    deviceContext.GetFrameBufferStack().pop();

    // Default FBO:
    if (deviceContext.GetFrameBufferStack().empty())
    {
        auto defT = deviceContext.defaultTargetsAndViewport;
		const uintptr_t ll= uintptr_t(defT.m_rt[0]);
		GLuint id = GLuint(ll);
        glBindFramebuffer(GL_FRAMEBUFFER, id);
        SetViewports(deviceContext, 1, &defT.viewport);
    }
    // Plugin FBO:
    else
    {
        auto topRt = deviceContext.GetFrameBufferStack().top();
		uintptr_t ll=uintptr_t(topRt->m_rt[0]);
		GLuint id = GLuint(ll);
        glBindFramebuffer(GL_FRAMEBUFFER, id);
        SetViewports(deviceContext, 1, &topRt->viewport);
    }
}

void RenderPlatform::SetViewports(crossplatform::DeviceContext& deviceContext,int num ,const crossplatform::Viewport* vps)
{
    if (num >= mMaxViewports)
    {
        SIMUL_CERR << "Too many viewports \n";
        return;
    }
    for (int i = 0; i < num; i++)
    {
        glViewportIndexedf(i, (GLfloat)vps[i].x, (GLfloat)vps[i].y, (GLfloat)vps[i].w, (GLfloat)vps[i].h);
        glScissorIndexed(i,     (GLint)vps[i].x, (GLint)vps[i].y,   (GLsizei)vps[i].w, (GLsizei)vps[i].h);
    }
}

void RenderPlatform::SetIndexBuffer(crossplatform::DeviceContext &,crossplatform::Buffer *buffer)
{
}

void RenderPlatform::SetTopology(crossplatform::DeviceContext &,crossplatform::Topology t)
{
    mCurTopology = toGLTopology(t);
}

void RenderPlatform::EnsureEffectIsBuilt(const char *,const std::vector<crossplatform::EffectDefineOptions> &)
{
}

void RenderPlatform::StoreRenderState(crossplatform::DeviceContext &)
{
}

void RenderPlatform::RestoreRenderState(crossplatform::DeviceContext &)
{
}

void RenderPlatform::PopRenderTargets(crossplatform::DeviceContext &)
{
}

GLenum RenderPlatform::toGLTopology(crossplatform::Topology t)
{
    switch (t)
    {
    case crossplatform::POINTLIST:
        return GL_POINTS;
    case crossplatform::LINELIST:
        return GL_LINES;
    case crossplatform::LINESTRIP:
        return GL_LINE_STRIP;
    case crossplatform::TRIANGLELIST:
        return GL_TRIANGLES;
    case crossplatform::TRIANGLESTRIP:
        return GL_TRIANGLE_STRIP;
    case crossplatform::LINELIST_ADJ:
        return GL_LINES_ADJACENCY;
    case crossplatform::LINESTRIP_ADJ:
        return GL_LINE_STRIP_ADJACENCY;
    case crossplatform::TRIANGLELIST_ADJ:
        return GL_TRIANGLES_ADJACENCY;
    case crossplatform::TRIANGLESTRIP_ADJ:
        return GL_TRIANGLE_STRIP_ADJACENCY;
    default:
        break;
    };
    return GL_LINE_LOOP;
}

void RenderPlatform::Draw(crossplatform::DeviceContext &deviceContext,int num_verts,int start_vert)
{
    BeginEvent(deviceContext, ((opengl::EffectPass*)deviceContext.contextState.currentEffectPass)->PassName.c_str());
    ApplyCurrentPass(deviceContext);
    glDrawArrays(mCurTopology, start_vert, num_verts);
    EndEvent(deviceContext);
}

void RenderPlatform::DrawIndexed(crossplatform::DeviceContext &deviceContext,int num_indices,int start_index,int base_vertex)
{
}

void RenderPlatform::DrawLines(crossplatform::DeviceContext &,crossplatform::PosColourVertex *lines,int vertex_count,bool strip,bool test_depth,bool view_centred)
{
}

void RenderPlatform::Draw2dLines(crossplatform::DeviceContext &deviceContext,crossplatform::PosColourVertex *lines,int vertex_count,bool strip)
{
}

void RenderPlatform::DrawCircle		(crossplatform::DeviceContext &,const float *,float ,const float *,bool)
{
}

void RenderPlatform::GenerateMips(crossplatform::DeviceContext& deviceContext, crossplatform::Texture* t, bool wrap, int array_idx)
{
    t->GenerateMips(deviceContext);
}

crossplatform::Shader* RenderPlatform::CreateShader()
{
	Shader* S = new Shader();
	return S;
}
