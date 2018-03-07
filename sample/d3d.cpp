//========================================================================
// simple nv12 render using direct3d
//------------------------------------------------------------------------
// DXVA2 renderer
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
// void DIRECT3D::render(const std::unique_ptr<UINT8[]>&data , UINT32 w, UINT32 h);
//------------------------------------------------------------------------
// Copyright (c) 2017-2018 Ji Wong Park <sailfish009@gmail.com>
//
// This software is provided 'as-is', without any express or implied
// warranty. In no event will the authors be held liable for any damages
// arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would
//    be appreciated but is not required.
//
// 2. Altered source versions must be plainly marked as such, and must not
//    be misrepresented as being the original software.
//
// 3. This notice may not be removed or altered from any source
//    distribution.
//
//========================================================================

#include "stdafx.h"
#include "d3d.h"
#include <stdio.h>

RECT g_SrcRect = VIDEO_MAIN_RECT;
RECT g_DstRect = VIDEO_MAIN_RECT;

const D3DCOLOR RGB_WHITE = D3DCOLOR_XRGB(0xEB, 0xEB, 0xEB);

DWORD
RGBtoYUV(const D3DCOLOR rgb)
{
  const INT A = HIBYTE(HIWORD(rgb));
  const INT R = LOBYTE(HIWORD(rgb)) - 16;
  const INT G = HIBYTE(LOWORD(rgb)) - 16;
  const INT B = LOBYTE(LOWORD(rgb)) - 16;

  //
  // studio RGB [16...235] to SDTV ITU-R BT.601 YCbCr
  //
  INT Y = (77 * R + 150 * G + 29 * B + 128) / 256 + 16;
  INT U = (-44 * R - 87 * G + 131 * B + 128) / 256 + 128;
  INT V = (131 * R - 110 * G - 21 * B + 128) / 256 + 128;

  return D3DCOLOR_AYUV(A, Y, U, V);
}

DXVA2_AYUVSample16 GetBackgroundColor()
{
  const D3DCOLOR yuv = RGBtoYUV(RGB_WHITE);

  const BYTE Y = LOBYTE(HIWORD(yuv));
  const BYTE U = HIBYTE(LOWORD(yuv));
  const BYTE V = LOBYTE(LOWORD(yuv));

  DXVA2_AYUVSample16 color;

  color.Cr = V * 0x100;
  color.Cb = U * 0x100;
  color.Y = Y * 0x100;
  color.Alpha = 0xFFFF;

  return color;
}

RECT
ScaleRectangle(const RECT& input, const RECT& src, const RECT& dst)
{
  RECT rect;

  UINT src_dx = src.right - src.left;
  UINT src_dy = src.bottom - src.top;

  UINT dst_dx = dst.right - dst.left;
  UINT dst_dy = dst.bottom - dst.top;

  //
  // Scale input rectangle within src rectangle to dst rectangle.
  //
  rect.left = input.left   * dst_dx / src_dx;
  rect.right = input.right  * dst_dx / src_dx;
  rect.top = input.top    * dst_dy / src_dy;
  rect.bottom = input.bottom * dst_dy / src_dy;

  return rect;
}


DIRECT3D::DIRECT3D(HWND hwnd):
  g_pD3D9(nullptr),
  g_pD3DD9(nullptr),
  g_pD3DRT(nullptr),
  m_width(VIDEO_MAIN_WIDTH),
  m_height(VIDEO_MAIN_HEIGHT)
{
  m_hwnd = hwnd;
  Direct3DCreate9Ex(D3D_SDK_VERSION, &g_pD3D9);

  ZeroMemory(&g_D3DPP, sizeof(g_D3DPP));

  g_D3DPP.BackBufferWidth = 0;
  g_D3DPP.BackBufferHeight = 0;
  g_D3DPP.BackBufferFormat = VIDEO_RENDER_TARGET_FORMAT;
  g_D3DPP.BackBufferCount = BACK_BUFFER_COUNT;
  g_D3DPP.SwapEffect = D3DSWAPEFFECT_DISCARD;
  g_D3DPP.hDeviceWindow = m_hwnd;
  g_D3DPP.Windowed = TRUE;                               // windowed
  g_D3DPP.Flags = D3DPRESENTFLAG_VIDEO;
  g_D3DPP.FullScreen_RefreshRateInHz = D3DPRESENT_RATE_DEFAULT;
  g_D3DPP.PresentationInterval = D3DPRESENT_INTERVAL_ONE;

  HRESULT hr = g_pD3D9->CreateDeviceEx(
    D3DADAPTER_DEFAULT, 
    D3DDEVTYPE_HAL, 
    m_hwnd,
    D3DCREATE_FPU_PRESERVE | D3DCREATE_HARDWARE_VERTEXPROCESSING|D3DCREATE_MULTITHREADED,
    &g_D3DPP, 
    NULL, 
    &g_pD3DD9);

  InitializeDXVA2();

  GetClientRect(m_hwnd, &client);

  target = client;
#if 0
  target.left = client.left;
  target.top = client.top;
  target.right = client.right / 2;
  target.bottom = client.bottom / 2;
#endif

#if 1
  RECT destRect;
  destRect.left = client.left;
  destRect.top = client.top;
  destRect.right = client.right / 2;
  destRect.bottom = client.bottom / 2;
#endif


  //
  // Initialize VPBlt parameters.
  //
  //blt.TargetFrame = start_100ns;
  blt.TargetRect = target;

  // DXVA2_VideoProcess_Constriction
  blt.ConstrictionSize.cx = target.right - target.left;
  blt.ConstrictionSize.cy = target.bottom - target.top;

  blt.BackgroundColor = GetBackgroundColor();

  // DXVA2_VideoProcess_YUV2RGBExtended
  blt.DestFormat.VideoChromaSubsampling = DXVA2_VideoChromaSubsampling_Unknown;
  blt.DestFormat.NominalRange = EX_COLOR_INFO[g_ExColorInfo][1];
  blt.DestFormat.VideoTransferMatrix = DXVA2_VideoTransferMatrix_Unknown;
  blt.DestFormat.VideoLighting = DXVA2_VideoLighting_dim;
  blt.DestFormat.VideoPrimaries = DXVA2_VideoPrimaries_BT709;
  blt.DestFormat.VideoTransferFunction = DXVA2_VideoTransFunc_709;

  blt.DestFormat.SampleFormat = DXVA2_SampleProgressiveFrame;

  //
  // Initialize main stream video sample.
  //
  // samples[0].Start = start_100ns;
  // samples[0].End = end_100ns;

  // DXVA2_VideoProcess_YUV2RGBExtended
  samples[0].SampleFormat.VideoChromaSubsampling = DXVA2_VideoChromaSubsampling_MPEG2;
  samples[0].SampleFormat.NominalRange = DXVA2_NominalRange_16_235;
  samples[0].SampleFormat.VideoTransferMatrix = EX_COLOR_INFO[g_ExColorInfo][0];
  samples[0].SampleFormat.VideoLighting = DXVA2_VideoLighting_dim;
  samples[0].SampleFormat.VideoPrimaries = DXVA2_VideoPrimaries_BT709;
  samples[0].SampleFormat.VideoTransferFunction = DXVA2_VideoTransFunc_709;

  samples[0].SampleFormat.SampleFormat = DXVA2_SampleProgressiveFrame;

  samples[0].SrcSurface = g_pMainStream;

  // DXVA2_VideoProcess_SubRects
  samples[0].SrcRect = g_SrcRect;

  // DXVA2_VideoProcess_StretchX, Y
  samples[0].DstRect = destRect; // ScaleRectangle(g_DstRect, VIDEO_MAIN_RECT, client);

  // DXVA2_VideoProcess_PlanarAlpha
  samples[0].PlanarAlpha = DXVA2FloatToFixed(float(0xFF) / 0xFF);


}

DIRECT3D::~DIRECT3D()
{
  DestroyDXVA2();
}


void DIRECT3D::render(const std::unique_ptr<UINT8[]>&data , UINT32 w, UINT32 h)
{
  UINT32 pitch                = 4096;
  D3DLOCKED_RECT lr;
  HRESULT hr                   = g_pMainStream->LockRect(&lr, NULL, D3DLOCK_NOSYSLOCK);
  UINT32 render_pitch = lr.Pitch;
  UINT8   * buf = (UINT8 *)lr.pBits;
  UINT8	 *dst_y              = buf;
  UINT8	 *dst_uv            = dst_y + (VIDEO_MAIN_HEIGHT * render_pitch);
  UINT8	 *src_y              = data.get();
  UINT8	 *src_uv           = src_y + pitch/2;
  g_pMainStream->UnlockRect();

  for (UINT32 i = 0; i < h; i++) 
  { 
    memcpy(dst_y, src_y, w);  dst_y += render_pitch;  src_y += 4096;
    if (i & 0x1) { memcpy(dst_uv, src_uv, w);  dst_uv += render_pitch;  src_uv += (pitch << 1); }
  }

  hr = g_pDXVAVPD->VideoProcessBlt(g_pD3DRT,
    &blt,
    samples,
    SUB_STREAM_COUNT + 1,
    NULL);

  if (FAILED(hr))
  {
    printf("VideoProcessBlt failed with error 0x%x.\n", hr);
  }

  g_pD3DD9->Present(NULL, NULL, NULL, NULL);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////

BOOL DIRECT3D::InitializeDXVA2()
{
  HRESULT hr;

  //
  // Retrieve a back buffer as the video render target.
  //
  hr = g_pD3DD9->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &g_pD3DRT);

  if (FAILED(hr))
  {
    printf("GetBackBuffer failed with error 0x%x.\n", hr);
    return FALSE;
  }

  //
  // Create DXVA2 Video Processor Service.
  //
  hr = DXVA2CreateVideoService(g_pD3DD9,
    IID_IDirectXVideoProcessorService,
    (VOID**)&g_pDXVAVPS);

  if (FAILED(hr))
  {
    printf("DXVA2CreateVideoService failed with error 0x%x.\n", hr);
    return FALSE;
  }

  //
  // Initialize the video descriptor.
  //
  g_VideoDesc.SampleWidth = VIDEO_MAIN_WIDTH;
  g_VideoDesc.SampleHeight = VIDEO_MAIN_HEIGHT;
  g_VideoDesc.SampleFormat.VideoChromaSubsampling = DXVA2_VideoChromaSubsampling_MPEG2;
  g_VideoDesc.SampleFormat.NominalRange = DXVA2_NominalRange_16_235;
  g_VideoDesc.SampleFormat.VideoTransferMatrix = EX_COLOR_INFO[g_ExColorInfo][0];
  g_VideoDesc.SampleFormat.VideoLighting = DXVA2_VideoLighting_dim;
  g_VideoDesc.SampleFormat.VideoPrimaries = DXVA2_VideoPrimaries_BT709;
  g_VideoDesc.SampleFormat.VideoTransferFunction = DXVA2_VideoTransFunc_709;
  g_VideoDesc.SampleFormat.SampleFormat = DXVA2_SampleProgressiveFrame;
  g_VideoDesc.Format = VIDEO_MAIN_FORMAT;
  g_VideoDesc.InputSampleFreq.Numerator = VIDEO_FPS;
  g_VideoDesc.InputSampleFreq.Denominator = 1;
  g_VideoDesc.OutputFrameFreq.Numerator = VIDEO_FPS;
  g_VideoDesc.OutputFrameFreq.Denominator = 1;

  //
  // Query the video processor GUID.
  //
  UINT count;
  GUID* guids = NULL;

  hr = g_pDXVAVPS->GetVideoProcessorDeviceGuids(&g_VideoDesc, &count, &guids);

  if (FAILED(hr))
  {
    printf("GetVideoProcessorDeviceGuids failed with error 0x%x.\n", hr);
    return FALSE;
  }

  //
  // Create a DXVA2 device.
  //
  for (UINT i = 0; i < count; i++)
  {
    if (CreateDXVA2VPDevice(guids[i]))
    {
      break;
    }
  }

  CoTaskMemFree(guids);

  if (!g_pDXVAVPD)
  {
    printf("Failed to create a DXVA2 device.\n");
    return FALSE;
  }

  return TRUE;
}


VOID DIRECT3D::DestroyDXVA2()
{

  if (g_pMainStream)
  {
    g_pMainStream->Release();
    g_pMainStream = NULL;
  }

  if (g_pDXVAVPD)
  {
    g_pDXVAVPD->Release();
    g_pDXVAVPD = NULL;
  }

  if (g_pDXVAVPS)
  {
    g_pDXVAVPS->Release();
    g_pDXVAVPS = NULL;
  }

  if (g_pD3DRT)
  {
    g_pD3DRT->Release();
    g_pD3DRT = NULL;
  }
}

BOOL DIRECT3D::CreateDXVA2VPDevice(REFGUID guid)
{
  HRESULT hr;

  //
  // Query the supported render target format.
  //
  UINT i, count;
  D3DFORMAT* formats = NULL;

  hr = g_pDXVAVPS->GetVideoProcessorRenderTargets(guid,
    &g_VideoDesc,
    &count,
    &formats);

  if (FAILED(hr))
  {
    printf("GetVideoProcessorRenderTargets failed with error 0x%x.\n", hr);
    return FALSE;
  }

  for (i = 0; i < count; i++)
  {
    if (formats[i] == VIDEO_RENDER_TARGET_FORMAT)
    {
      break;
    }
  }

  CoTaskMemFree(formats);

  if (i >= count)
  {
    printf("GetVideoProcessorRenderTargets doesn't support that format.\n");
    return FALSE;
  }

  //
  // Query video processor capabilities.
  //
  hr = g_pDXVAVPS->GetVideoProcessorCaps(guid,
    &g_VideoDesc,
    VIDEO_RENDER_TARGET_FORMAT,
    &g_VPCaps);

  if (FAILED(hr))
  {
    printf("GetVideoProcessorCaps failed with error 0x%x.\n", hr);
    return FALSE;
  }

  //
  // This is a progressive device and we cannot provide any reference sample.
  //
  if (g_VPCaps.NumForwardRefSamples > 0 || g_VPCaps.NumBackwardRefSamples > 0)
  {
    printf("NumForwardRefSamples or NumBackwardRefSamples is greater than 0.\n");
    return FALSE;
  }

  //
  // Check to see if the device supports all the VP operations we want.
  //
  if ((g_VPCaps.VideoProcessorOperations & VIDEO_REQUIED_OP) != VIDEO_REQUIED_OP)
  {
    printf("The DXVA2 device doesn't support the VP operations.\n");
    return FALSE;
  }

  //
  // Create a main stream surface.
  //
  hr = g_pDXVAVPS->CreateSurface(VIDEO_MAIN_WIDTH,
    VIDEO_MAIN_HEIGHT,
    0,
    VIDEO_MAIN_FORMAT,
    g_VPCaps.InputPool,
    0,
    DXVA2_VideoProcessorRenderTarget, //DXVA2_VideoSoftwareRenderTarget,
    &g_pMainStream,
    NULL);

  if (FAILED(hr))
  {
    printf("CreateSurface(MainStream) failed with error 0x%x.\n", hr);
    return FALSE;
  }

  //
  // Finally create a video processor device.
  //
  hr = g_pDXVAVPS->CreateVideoProcessor(guid,
    &g_VideoDesc,
    VIDEO_RENDER_TARGET_FORMAT,
    SUB_STREAM_COUNT,
    &g_pDXVAVPD);

  if (FAILED(hr))
  {
    printf("CreateVideoProcessor failed with error 0x%x.\n", hr);
    return FALSE;
  }

  g_GuidVP = guid;

  return TRUE;
}



