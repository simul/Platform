#define NOMINMAX
#include "Utilities.h"
#include "Macros.h"
#include <algorithm>
using namespace simul;
using namespace dx9;


int ByteSizeOfFormatElement( D3DFORMAT format )
{
	switch (format)
	{
	//case D3DFMT_DXT1:
	//	return 4;
	case D3DFMT_R3G3B2:
	case D3DFMT_A8:
	case D3DFMT_P8:
	case D3DFMT_L8:
	case D3DFMT_A4L4:
	case D3DFMT_DXT2:
	case D3DFMT_DXT3:
	case D3DFMT_DXT4:
	case D3DFMT_DXT5:
		return 1;
	case D3DFMT_X4R4G4B4:
	case D3DFMT_A4R4G4B4:
	case D3DFMT_A1R5G5B5:
	case D3DFMT_X1R5G5B5:
	case D3DFMT_R5G6B5:
	case D3DFMT_A8R3G3B2:
	case D3DFMT_A8P8:
	case D3DFMT_A8L8:
	case D3DFMT_V8U8:
	case D3DFMT_L6V5U5:
	case D3DFMT_D16_LOCKABLE:
	case D3DFMT_D15S1:
	case D3DFMT_D16:
	case D3DFMT_L16:
	case D3DFMT_INDEX16:
	case D3DFMT_CxV8U8:
	case D3DFMT_G8R8_G8B8:
	case D3DFMT_R8G8_B8G8:
	case D3DFMT_R16F:
		return 2;
	case D3DFMT_R8G8B8:
		return 3;
	case D3DFMT_A2W10V10U10:
	case D3DFMT_A2B10G10R10:
	case D3DFMT_A2R10G10B10:
	case D3DFMT_X8R8G8B8:
	case D3DFMT_A8R8G8B8:
	case D3DFMT_X8L8V8U8:
	case D3DFMT_Q8W8V8U8:
	case D3DFMT_V16U16:
		//case D3DFMT_W11V11U10: // Dropped in DX9.0
	case D3DFMT_UYVY:
	case D3DFMT_YUY2:
	case D3DFMT_G16R16:
	case D3DFMT_D32:
	case D3DFMT_D24S8:
	case D3DFMT_D24X8:
	case D3DFMT_D24X4S4:
	case D3DFMT_D24FS8:
	case D3DFMT_D32F_LOCKABLE:
	case D3DFMT_INDEX32:
	case D3DFMT_MULTI2_ARGB8:
	case D3DFMT_G16R16F:
	case D3DFMT_R32F:
		return 4;
	case D3DFMT_A16B16G16R16:
	case D3DFMT_Q16W16V16U16:
	case D3DFMT_A16B16G16R16F:
	case D3DFMT_G32R32F:
		return 8;
	case D3DFMT_A32B32G32R32F:
		return 16;
	}
	return 0;
}

TextureStruct::TextureStruct()
	:texture(NULL)
	,width(0)
	,length(0)
	,depth(0)
	,format(D3DFMT_UNKNOWN)
{
}


TextureStruct::~TextureStruct()
{
	release();
}

void TextureStruct::release()
{
	SAFE_RELEASE(texture);
	format=D3DFMT_UNKNOWN;
	width=length=depth=0;
}

void TextureStruct::ensureTexture3DSizeAndFormat(IDirect3DDevice9 *pd3dDevice,int w,int l,int d,D3DFORMAT f)
{
	if(width==w&&length==l&&depth==d&&format==f)
		return;
	release();
	format=f;
	width=w;length=l;depth=d;
	LPDIRECT3DVOLUMETEXTURE9 tex3d=NULL;
	V_CHECK(D3DXCreateVolumeTexture(pd3dDevice,w,l,d,1,0,format,D3DPOOL_MANAGED,&tex3d));
	texture=tex3d;
}

void TextureStruct::setTexels(const void *src,int texel_index,int num_texels)
{
	/*
		ID3D9Texture3D* ppd(NULL);
		if(texture->QueryInterface( __uuidof(ID3D9Texture3D),(void**)&ppd)!=S_OK)
			ok=false;
		else
		{
			ppd->GetDesc(&textureDesc);
			if(textureDesc.Width!=w||textureDesc.Height!=l||textureDesc.Depth!=d||textureDesc.Format!=f)
				ok=false;
		}
		SAFE_RELEASE(ppd);
	*/
	D3DLOCKED_BOX lockedBox={0};
	D3DVOLUME_DESC desc3d;
	LPDIRECT3DVOLUMETEXTURE9 tex3d=NULL;
	{
		tex3d=(LPDIRECT3DVOLUMETEXTURE9)texture;
		tex3d->GetLevelDesc(0,&desc3d);
		if(!tex3d)
			return;
		V_CHECK(tex3d->LockBox(0,&lockedBox,NULL,NULL));
	}

	int byteSize=ByteSizeOfFormatElement(format);
	const unsigned char *source=(const unsigned char*)src;
	unsigned char *target=(unsigned char*)lockedBox.pBits;
	int expected_pitch=byteSize*width;
	if(lockedBox.RowPitch==expected_pitch)
	{
		target+=texel_index*byteSize;
		memcpy(target,source,num_texels*byteSize);
	}
	else
	{
		int block	=lockedBox.RowPitch/byteSize;
		int row		=texel_index/width;
		int last_row=(texel_index+num_texels)/width;
		int col		=texel_index-row*width;
		target		+=row*block*byteSize;
		source		+=col*byteSize;
		int columns=std::min(num_texels,width-col);
		memcpy(target,source,columns*byteSize);
		source		+=columns*byteSize;
		target		+=block*byteSize;
		for(int r=row+1;r<last_row;r++)
		{
			memcpy(target,source,width*byteSize);
			target		+=block*byteSize;
			source		+=width*byteSize;
		}
		int end_columns=texel_index+num_texels-last_row*width;
		if(end_columns>0)
			memcpy(target,source,end_columns*byteSize);
	}

	{
		V_CHECK(tex3d->UnlockBox(0));
		tex3d=(LPDIRECT3DVOLUMETEXTURE9)texture;
		tex3d->GetLevelDesc(0,&desc3d);
	}
}