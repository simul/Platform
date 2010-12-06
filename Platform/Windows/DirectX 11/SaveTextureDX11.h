#define _CRT_SECURE_NO_WARNINGS
// Direct3D includes
#include <d3d11.h>

extern void Screenshot(IDirect3DDevice9* pd3dDevice,const char *txt);
extern void SaveTexture(LPDIRECT3DTEXTURE9 ldr_buffer_texture,const char *txt,bool as_dds);