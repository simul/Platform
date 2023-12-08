#include "Layout.h"
#ifdef _MSC_VER
#include <Windows.h>
#endif
#include "Platform/Core/RuntimeError.h"
#include "Platform/CrossPlatform/DeviceContext.h"

using namespace platform;
using namespace crossplatform;

 LayoutSemantic TextToSemantic(const char *txt)
{
	std::string str=txt;
	if(str=="POSITION") return LayoutSemantic::POSITION;
	if(str=="NORMAL") return LayoutSemantic::NORMAL;
	if(str=="TANGENT") return LayoutSemantic::TANGENT;
	if(str=="TEXCOORD") return LayoutSemantic::TEXCOORD;
	if(str=="POSITIONT") return LayoutSemantic::POSITIONT;
	if(str=="BINORMAL") return LayoutSemantic::BINORMAL;
	if(str=="COLOUR") return LayoutSemantic::COLOUR;;
	if(str=="BLENDINDICES") return LayoutSemantic::BLENDINDICES;
	if(str=="BLENDWEIGHT") return LayoutSemantic::BLENDWEIGHT;
	if(str=="PSIZE") return LayoutSemantic::PSIZE;
	if(str=="CUSTOM") return LayoutSemantic::CUSTOM;
	if(str=="TESSFACTOR") return LayoutSemantic::TESSFACTOR;
	if(str=="COLOR") return LayoutSemantic::COLOUR;
	return LayoutSemantic::CUSTOM;
};

bool crossplatform::LayoutMatches(const std::vector<LayoutDesc> &desc1,const std::vector<LayoutDesc> &desc2)
{
	if(desc1.size()==0||desc1.size()!=desc2.size())
		return false;
	for(size_t i=0;i<desc1.size();i++)
	{
		const auto &d1=desc1[i];
		const auto &d2=desc2[i];
		if(d1.format!=d2.format)
			return false;
	}
	return true;
}

bool crossplatform::LayoutContains(const std::vector<LayoutDesc> &desc, const char *semanticName)
{
	std::string str(semanticName);
	for (size_t i = 0; i < desc.size(); i++)
	{
		const auto &d1 = desc[i];
		if (d1.semanticName == str)
			return true;
	}
	return false;
}

Layout::Layout()
	:apply_count(0)
	,struct_size(0)
{
}


Layout::~Layout()
{
	SIMUL_ASSERT(apply_count==0);
}

void Layout::SetDesc(const LayoutDesc *d,int num,bool i)
{
	interleaved = i;
	parts.clear();
	struct_size=0;
	for(int i=0;i<num;i++)
	{
		parts.push_back(*d);
		SIMUL_ASSERT(d->alignedByteOffset==struct_size);
		struct_size+=GetByteSize(d->format);
		d++;
	}
	MakeHash();
}

int Layout::GetPitch() const
{
	return interleaved?struct_size:0;
}

static const char *GetSemanticText(LayoutSemantic s)
{
	switch(s)
	{
	case LayoutSemantic::UNKNOWN:				return "UNKNOWN";
	case LayoutSemantic::POSITION:				return "POSITION";
	case LayoutSemantic::NORMAL:				return "NORMAL";
	case LayoutSemantic::TANGENT:				return "TANGENT";
	case LayoutSemantic::TEXCOORD:				return "TEXCOORD";
	case LayoutSemantic::POSITIONT:				return "POSITIONT";
	case LayoutSemantic::BINORMAL:				return "BINORMAL";
	case LayoutSemantic::COLOUR:				return "COLOR";
	case LayoutSemantic::BLENDINDICES:			return "BLENDINDICES";
	case LayoutSemantic::BLENDWEIGHT:			return "BLENDWEIGHT";
	case LayoutSemantic::PSIZE:					return "PSIZE";
	case LayoutSemantic::CUSTOM:				return "CUSTOM";
	case LayoutSemantic::TESSFACTOR:			return "TESSFACTOR";
	default:
	return "";
	};
};
bool Layout::HasSemantic(LayoutSemantic semantic) const
{
	for(const auto &p:parts)
	{
		if(std::string(p.semanticName)==GetSemanticText(semantic))
			return true;
	}
	return false;
}

int Layout::GetStructSize() const
{
	return struct_size;
}

void Layout::Apply(DeviceContext &deviceContext)
{
	deviceContext.contextState.currentLayout=this;
}

void Layout::Unapply(DeviceContext &deviceContext)
{
	deviceContext.contextState.currentLayout=nullptr;
}
		struct LayoutDesc
		{
			const char *semanticName;
			int			semanticIndex;
			PixelFormat	format;
			int			inputSlot;
			int			alignedByteOffset;
			bool		perInstance;		// otherwise it's per vertex.
			int			instanceDataStepRate;
		};

uint64_t GetLayoutPartHash(const SimpleLayoutSpec &s)
{
	uint64_t h=0;
	h|=(uint64_t)s.format;			// 5-6 bits.
	//h|=(uint64_t)(s.inputSlot<<6);
	//h|=(uint64_t)(s.semantic)<<8;	// 4 bits
	return h;
}

uint64_t platform::crossplatform::GetLayoutHash(const platform::crossplatform::LayoutDesc &d)
{
	SimpleLayoutSpec sp={d.format,TextToSemantic(d.semanticName),d.semanticIndex};
	uint64_t h=GetLayoutPartHash(sp);
	return h;
}

uint64_t platform::crossplatform::GetLayoutHash(const std::vector<SimpleLayoutSpec> &l)
{
	uint64_t hash=0;
	std::vector<uint64_t> H(l.size(),0);
	for(size_t i=0;i<l.size();i++)
	{
		const SimpleLayoutSpec &s=l[i];
		uint64_t f=(uint64_t)GetLayoutPartHash(s);
		H[i]=f;
	}
	for(size_t i=0;i<l.size();i++)
	{
		hash=hash<<7;
		hash^=H[i];
	}
	return hash;
}

void Layout::MakeHash() 
{
	hash=0;
	std::vector<uint64_t> H(parts.size(),0);
	for(size_t i=0;i<parts.size();i++)
	{
		uint64_t f=(uint64_t)GetLayoutHash(parts[i]);
		H[i]=f;
	}
	for(size_t i=0;i<parts.size();i++)
	{
		hash=hash<<7;
		hash^=H[i];
	}
}
