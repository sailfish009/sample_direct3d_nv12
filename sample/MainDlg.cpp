//========================================================================
// simple nv12 render using direct3d
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
#include "MainDlg.h"


DIRECT3D                             *CMainDlg::m_render = nullptr;
HWND                                      CMainDlg::view_hwnd = nullptr;
std::unique_ptr<UINT8[]> CMainDlg::indata = nullptr;

FILE *fp = nullptr;
std::atomic<UINT8> video_off = 0;

void CMainDlg::init()
{
  BOOL ready = TRUE;

  if (fp == nullptr)
  {
    indata = std::make_unique<UINT8[]>(4096 * 2048);
    if (fopen_s(&fp, "../../nv12.yuv", "rb+") != 0)
    {
      printf("open file failed\n");
      ready = FALSE;
    }
    else
      printf("file opened\n");
  }

  if (ready)
  {
    view_hwnd = GetDlgItem(IDC_VIEW);
    run_video();
  }
}

BOOL CMainDlg::run_video()
{
  // Direct3D init
  if (m_render != nullptr)  delete m_render;
  m_render = new DIRECT3D(view_hwnd);

  video_off = 0;
  video_worker = std::thread([=] {video_thread(); });
  video_worker.detach();
  return FALSE;
}

BOOL CMainDlg::stop_video()
{
  video_off = 1;
  Sleep(100);

  if (m_render != nullptr)
  {
    delete m_render;
    m_render = nullptr;
  }
  return TRUE;
}

void CMainDlg::video_thread()
{
  for (;;)
  {
    if (video_off) return;
    if (fread(indata.get(), 1, 4096 * VIDEO_MAIN_HEIGHT, fp) != 4096 * VIDEO_MAIN_HEIGHT)
    {
      fseek(fp, 0, SEEK_SET);
    }
    m_render->render(std::move(indata), VIDEO_MAIN_WIDTH, VIDEO_MAIN_HEIGHT);
    std::this_thread::sleep_for(std::chrono::milliseconds(33));
  }
}

