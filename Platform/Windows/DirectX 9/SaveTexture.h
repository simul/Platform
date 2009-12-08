#define _CRT_SECURE_NO_WARNINGS
// Direct3D includes
#include <d3d9.h>
#include <tchar.h>

extern void Screenshot(IDirect3DDevice9* pd3dDevice,const _TCHAR *txt);
extern void SaveTexture(LPDIRECT3DTEXTURE9 ldr_buffer_texture,const _TCHAR *txt);