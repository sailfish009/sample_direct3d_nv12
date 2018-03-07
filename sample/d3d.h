//========================================================================
// simple nv12 render using direct3d
//------------------------------------------------------------------------
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//
// Source : DXVA2_VideoProc Sample
// 
// https://msdn.microsoft.com/en-us/library/windows/desktop/bb970335(v=vs.85).aspx
// 
// Modified by Ji Wong Park <sailfish009@gmail.com>
//
//========================================================================

#pragma once
#include <d3d9.h>
#include <initguid.h>
#include <d3d9.h>
#include <dxva2api.h>
#include <memory>

#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "dxva2.lib")

const UINT VIDEO_MAIN_WIDTH = 1920;     // 960; // 640;
const UINT VIDEO_MAIN_HEIGHT = 1080;   // 540; // 480;
const RECT VIDEO_MAIN_RECT = { 0, 0, VIDEO_MAIN_WIDTH, VIDEO_MAIN_HEIGHT };
const D3DFORMAT VIDEO_MAIN_FORMAT = (D3DFORMAT)MAKEFOURCC('N', 'V', '1', '2');
const D3DFORMAT VIDEO_RENDER_TARGET_FORMAT = D3DFMT_X8R8G8B8;
const UINT VIDEO_FPS = 30; // 60;
const UINT BACK_BUFFER_COUNT = 1;
const UINT SUB_STREAM_COUNT = 0;     // 1;
const UINT VIDEO_REQUIED_OP = DXVA2_VideoProcess_YUV2RGB;

const UINT EX_COLOR_INFO[][2] =
{
  // SDTV ITU-R BT.601 YCbCr to driver's optimal RGB range
  { DXVA2_VideoTransferMatrix_BT601, DXVA2_NominalRange_Unknown },
  // SDTV ITU-R BT.601 YCbCr to studio RGB [16...235]
  { DXVA2_VideoTransferMatrix_BT601, DXVA2_NominalRange_16_235 },
  // SDTV ITU-R BT.601 YCbCr to computer RGB [0...255]
  { DXVA2_VideoTransferMatrix_BT601, DXVA2_NominalRange_0_255 },
  // HDTV ITU-R BT.709 YCbCr to driver's optimal RGB range
  { DXVA2_VideoTransferMatrix_BT709, DXVA2_NominalRange_Unknown },
  // HDTV ITU-R BT.709 YCbCr to studio RGB [16...235]
  { DXVA2_VideoTransferMatrix_BT709, DXVA2_NominalRange_16_235 },
  // HDTV ITU-R BT.709 YCbCr to computer RGB [0...255]
  { DXVA2_VideoTransferMatrix_BT709, DXVA2_NominalRange_0_255 }
};


template <class T>
inline void SafeRelease(T*& pT)
{
  if (pT != nullptr)
  {
    pT->Release();
    pT = nullptr;
  }
}

class DIRECT3D
{
public:
  DIRECT3D(HWND hWnd);
  ~DIRECT3D();
  void                                                    render(const std::unique_ptr<UINT8[]>&data, UINT32 w, UINT32 h);

private:
  HWND						                                    m_hwnd;
  HWND						                                    m_parent_hwnd;
  UINT32					                                    m_width;
  UINT32					                                    m_height;
  RECT                                                  client;
  RECT                                                  target;

private:
  IDirect3D9Ex                                  *g_pD3D9;
  IDirect3DDevice9Ex                     *g_pD3DD9;
  IDirect3DSurface9                       *g_pD3DRT;
  D3DPRESENT_PARAMETERS g_D3DPP = { 0 };
  DXVA2_VideoProcessBltParams blt = { 0 };
  DXVA2_VideoSample samples[2] = { 0 };

  IDirectXVideoProcessorService* g_pDXVAVPS = NULL;
  IDirectXVideoProcessor*        g_pDXVAVPD = NULL;
  IDirect3DSurface9* g_pMainStream = NULL;
  GUID                     g_GuidVP = { 0 };
  DXVA2_VideoDesc          g_VideoDesc = { 0 };
  DXVA2_VideoProcessorCaps g_VPCaps = { 0 };

private:
  INT g_ExColorInfo = 0;

  BOOL InitializeDXVA2();
  VOID  DestroyDXVA2();
  BOOL CreateDXVA2VPDevice(REFGUID guid);

};

