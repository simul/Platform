#pragma once
#include <d3d12.h>
#include <d3dx12.h>
typedef ID3D12GraphicsCommandList ID3D12GraphicsCommandListType;
typedef ID3D12Device5 ID3D12DeviceType;
#include "dxcapi.h"

#pragma comment(lib,"d3d12.lib")
#pragma comment(lib,"D3dcompiler.lib")
#pragma comment(lib,"DXGI.lib")
#define PLATFORM_NAME "DirectX 12"
#define SFX_CONFIG_FILENAME "DirectX12/Sfx/DirectX12.json"

//https://github.com/microsoft/DirectX-Headers/blob/main/include/directx/d3dx12_pipeline_state_stream.h
//https://www.techthoughts.info/windows-version-numbers/
//https://github.com/microsoft/DMF/blob/master/Dmf/Framework/DmfIncludes_KERNEL_MODE.h
//https://github.com/MiroKaku/Musa.Veil/blob/main/Veil.h
// https://doxygen.reactos.org/d9/d16/sdkddkver_8h_source.html
//NTDDI_WINTHRESHOLD    0x0A000000    10.0.10240.0 / 1507 / Threshold 1
//NTDDI_WIN10           0x0A000000    10.0.10240.0 / 1507 / Threshold 1
//NTDDI_WIN10_TH2       0x0A000001    10.0.10586.0 / 1511 / Threshold 2
//NTDDI_WIN10_RS1       0x0A000002    10.0.14393.0 / 1607 / Redstone 1
//NTDDI_WIN10_RS2       0x0A000003    10.0.15063.0 / 1703 / Redstone 2
//NTDDI_WIN10_RS3       0x0A000004    10.0.16299.0 / 1709 / Redstone 3
//NTDDI_WIN10_RS4       0x0A000005    10.0.17134.0 / 1803 / Redstone 4
//NTDDI_WIN10_RS5       0x0A000006    10.0.17763.0 / 1809 / Redstone 5
//NTDDI_WIN10_19H1      0x0A000007    10.0.18362.0 / 1903 / Titanium - 19H1
//NTDDI_WIN10_VB        0x0A000008    10.0.19041.0 / 2004 / Vibranium
//NTDDI_WIN10_MN        0x0A000009    10.0.19042.0 / 20H2 / Manganese
//NTDDI_WIN10_FE        0x0A00000A    10.0.19043.0 / 21H1 / Ferrum (Iron)
//NTDDI_WIN10_CO        0x0A00000B    10.0.22000.0 / 21H2 / Cobalt
//NTDDI_WIN10_NI        0x0A00000C    10.0.22621.0 / 22H2 / Nickel
//NTDDI_WIN10_CU        0x0A00000D    10.0.22621.0 / 22H2 / Copper - Very unsure about these use with caution.
//NTDDI_WIN11_ZN        0x0A00000E    10.0.26100.0 / 23H2 / Zinc - Very unsure about these use with caution.
//NTDDI_WIN11_GA        0x0A00000F    10.0.26100.0 / 23H2 / Gallium - Very unsure about these use with caution.
//NTDDI_WIN11_GE        0x0A000010    10.0.26100.0 / 24H2 / Germanium - Very unsure about these use with caution.

#if defined(D3D12_SDK_VERSION) && (D3D12_SDK_VERSION >= 608)
	#define PLATFORM_CD3DX12_PIPELINE_STATE_STREAM 4
#elif defined(D3D12_SDK_VERSION) && (D3D12_SDK_VERSION >= 606)
	#define PLATFORM_CD3DX12_PIPELINE_STATE_STREAM 3
#elif defined(NTDDI_WIN10_VB) && (NTDDI_VERSION >= NTDDI_WIN10_VB)
	#define PLATFORM_CD3DX12_PIPELINE_STATE_STREAM 2
#elif defined(NTDDI_WIN10_RS3) && (NTDDI_VERSION >= NTDDI_WIN10_RS3)
	#define PLATFORM_CD3DX12_PIPELINE_STATE_STREAM 1
#elif defined(NTDDI_WIN10_RS2) && (NTDDI_VERSION >= NTDDI_WIN10_RS2)
	#define PLATFORM_CD3DX12_PIPELINE_STATE_STREAM 0
#else
	#undef PLATFORM_CD3DX12_PIPELINE_STATE_STREAM
#endif
