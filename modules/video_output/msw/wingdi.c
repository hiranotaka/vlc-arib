/*****************************************************************************
 * wingdi.c : Win32 / WinCE GDI video output plugin for vlc
 *****************************************************************************
 * Copyright (C) 2002-2009 the VideoLAN team
 * $Id$
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
 *          Samuel Hocevar <sam@zoy.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_interface.h>
#include <vlc_playlist.h>
#include <vlc_vout.h>

#include <windows.h>
#include <tchar.h>
#include <commctrl.h>

#include "vout.h"

#define MAX_DIRECTBUFFERS 10

#ifndef WS_NONAVDONEBUTTON
#define WS_NONAVDONEBUTTON 0
#endif
/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  OpenVideo  ( vlc_object_t * );
static void CloseVideo ( vlc_object_t * );

static int  Init      ( vout_thread_t * );
static void End       ( vout_thread_t * );
static int  Manage    ( vout_thread_t * );
static void Render    ( vout_thread_t *, picture_t * );
#ifdef MODULE_NAME_IS_wingapi
static void FirstDisplayGAPI( vout_thread_t *, picture_t * );
static void DisplayGAPI( vout_thread_t *, picture_t * );
static int GAPILockSurface( vout_thread_t *, picture_t * );
static int GAPIUnlockSurface( vout_thread_t *, picture_t * );
#else
static void FirstDisplayGDI( vout_thread_t *, picture_t * );
static void DisplayGDI( vout_thread_t *, picture_t * );
#endif
static void SetPalette( vout_thread_t *, uint16_t *, uint16_t *, uint16_t * );

static void InitBuffers        ( vout_thread_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin ()
    set_category( CAT_VIDEO )
    set_subcategory( SUBCAT_VIDEO_VOUT )
#ifdef MODULE_NAME_IS_wingapi
    set_shortname( "Windows GAPI" )
    set_description( N_("Windows GAPI video output") )
    set_capability( "video output", 20 )
#else
    set_shortname( "Windows GDI" )
    set_description( N_("Windows GDI video output") )
    set_capability( "video output", 10 )
#endif
    set_callbacks( OpenVideo, CloseVideo )
vlc_module_end ()

/*****************************************************************************
 * OpenVideo: activate GDI video thread output method
 *****************************************************************************/
static int OpenVideo ( vlc_object_t *p_this )
{
    vout_thread_t * p_vout = (vout_thread_t *)p_this;

    p_vout->p_sys = (vout_sys_t *)calloc( 1, sizeof(vout_sys_t) );
    if( !p_vout->p_sys ) return VLC_ENOMEM;

#ifdef MODULE_NAME_IS_wingapi
    /* Load GAPI */
    p_vout->p_sys->gapi_dll = LoadLibrary( _T("GX.DLL") );
    if( p_vout->p_sys->gapi_dll == NULL )
    {
        msg_Warn( p_vout, "failed loading gx.dll" );
        free( p_vout->p_sys );
        return VLC_EGENERIC;
    }

    GXOpenDisplay = (void *)GetProcAddress( p_vout->p_sys->gapi_dll,
        _T("?GXOpenDisplay@@YAHPAUHWND__@@K@Z") );
    GXCloseDisplay = (void *)GetProcAddress( p_vout->p_sys->gapi_dll,
        _T("?GXCloseDisplay@@YAHXZ") );
    GXBeginDraw = (void *)GetProcAddress( p_vout->p_sys->gapi_dll,
        _T("?GXBeginDraw@@YAPAXXZ") );
    GXEndDraw = (void *)GetProcAddress( p_vout->p_sys->gapi_dll,
        _T("?GXEndDraw@@YAHXZ") );
    GXGetDisplayProperties = (void *)GetProcAddress( p_vout->p_sys->gapi_dll,
        _T("?GXGetDisplayProperties@@YA?AUGXDisplayProperties@@XZ") );
    GXSuspend = (void *)GetProcAddress( p_vout->p_sys->gapi_dll,
        _T("?GXSuspend@@YAHXZ") );
    GXResume = GetProcAddress( p_vout->p_sys->gapi_dll,
        _T("?GXResume@@YAHXZ") );

    if( !GXOpenDisplay || !GXCloseDisplay || !GXBeginDraw || !GXEndDraw ||
        !GXGetDisplayProperties || !GXSuspend || !GXResume )
    {
        msg_Err( p_vout, "failed GetProcAddress on gapi.dll" );
        free( p_vout->p_sys );
        return VLC_EGENERIC;
    }

    msg_Dbg( p_vout, "GAPI DLL loaded" );

    p_vout->p_sys->render_width = p_vout->render.i_width;
    p_vout->p_sys->render_height = p_vout->render.i_height;
#endif

    p_vout->pf_init = Init;
    p_vout->pf_end = End;
    p_vout->pf_manage = Manage;
    p_vout->pf_render = Render;
    p_vout->pf_control = Control;
#ifdef MODULE_NAME_IS_wingapi
    p_vout->pf_display = FirstDisplayGAPI;
#else
    p_vout->pf_display = FirstDisplayGDI;
#endif

    if( CommonInit( p_vout ) )
        goto error;

    return VLC_SUCCESS;

error:
    CloseVideo( VLC_OBJECT(p_vout) );
    return VLC_EGENERIC;
}

/*****************************************************************************
 * CloseVideo: deactivate the GDI video output
 *****************************************************************************/
static void CloseVideo ( vlc_object_t *p_this )
{
    vout_thread_t * p_vout = (vout_thread_t *)p_this;

    CommonClean( p_vout );

#ifdef MODULE_NAME_IS_wingapi
    FreeLibrary( p_vout->p_sys->gapi_dll );
#endif

    free( p_vout->p_sys );
}

/*****************************************************************************
 * Init: initialize video thread output method
 *****************************************************************************/
static int Init( vout_thread_t *p_vout )
{
    picture_t *p_pic;

    /* Initialize offscreen buffer */
    InitBuffers( p_vout );

    p_vout->p_sys->rect_display.left = 0;
    p_vout->p_sys->rect_display.top = 0;
    p_vout->p_sys->rect_display.right  = GetSystemMetrics(SM_CXSCREEN);
    p_vout->p_sys->rect_display.bottom = GetSystemMetrics(SM_CYSCREEN);

    I_OUTPUTPICTURES = 0;

    /* Initialize the output structure */
    switch( p_vout->p_sys->i_depth )
    {
    case 8:
        p_vout->output.i_chroma = VLC_CODEC_RGB8;
        p_vout->output.pf_setpalette = SetPalette;
        break;
    case 15:
        p_vout->output.i_chroma = VLC_CODEC_RGB15;
        p_vout->output.i_rmask  = 0x7c00;
        p_vout->output.i_gmask  = 0x03e0;
        p_vout->output.i_bmask  = 0x001f;
        break;
    case 16:
        p_vout->output.i_chroma = VLC_CODEC_RGB16;
        p_vout->output.i_rmask  = 0xf800;
        p_vout->output.i_gmask  = 0x07e0;
        p_vout->output.i_bmask  = 0x001f;
        break;
    case 24:
        p_vout->output.i_chroma = VLC_CODEC_RGB24;
        p_vout->output.i_rmask  = 0x00ff0000;
        p_vout->output.i_gmask  = 0x0000ff00;
        p_vout->output.i_bmask  = 0x000000ff;
        break;
    case 32:
        p_vout->output.i_chroma = VLC_CODEC_RGB32;
        p_vout->output.i_rmask  = 0x00ff0000;
        p_vout->output.i_gmask  = 0x0000ff00;
        p_vout->output.i_bmask  = 0x000000ff;
        break;
    default:
        msg_Err( p_vout, "screen depth %i not supported",
                 p_vout->p_sys->i_depth );
        return VLC_EGENERIC;
        break;
    }

    p_pic = &p_vout->p_picture[0];

#ifdef MODULE_NAME_IS_wingapi
    p_vout->output.i_width  = 0;
    p_vout->output.i_height = 0;
    p_pic->pf_lock  = GAPILockSurface;
    p_pic->pf_unlock = GAPIUnlockSurface;
    Manage( p_vout );
    GAPILockSurface( p_vout, p_pic );
    p_vout->i_changes = 0;
    p_vout->output.i_width  = p_vout->p_sys->render_width;
    p_vout->output.i_height = p_vout->p_sys->render_height;

#else
    p_vout->output.i_width  = p_vout->render.i_width;
    p_vout->output.i_height = p_vout->render.i_height;

    p_vout->fmt_out = p_vout->fmt_in;
    p_vout->fmt_out.i_chroma = p_vout->output.i_chroma;
#endif

    p_vout->output.i_aspect = p_vout->render.i_aspect;

    p_pic->p->p_pixels = p_vout->p_sys->p_pic_buffer;
    p_pic->p->i_lines = p_vout->output.i_height;
    p_pic->p->i_visible_lines = p_vout->output.i_height;
    p_pic->p->i_pitch = p_vout->p_sys->i_pic_pitch;
    p_pic->p->i_pixel_pitch = p_vout->p_sys->i_pic_pixel_pitch;
    p_pic->p->i_visible_pitch = p_vout->output.i_width *
        p_pic->p->i_pixel_pitch;
    p_pic->i_planes = 1;
    p_pic->i_status = DESTROYED_PICTURE;
    p_pic->i_type   = DIRECT_PICTURE;

    PP_OUTPUTPICTURE[ I_OUTPUTPICTURES++ ] = p_pic;

    /* Change the window title bar text */
#ifdef MODULE_NAME_IS_wingapi
    EventThreadUpdateTitle( p_vout->p_sys->p_event, VOUT_TITLE " (WinGAPI output)" );
#else
    EventThreadUpdateTitle( p_vout->p_sys->p_event, VOUT_TITLE " (WinGDI output)" );
#endif
    UpdateRects( p_vout, true );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * End: terminate video thread output method
 *****************************************************************************/
static void End( vout_thread_t *p_vout )
{
#ifdef MODULE_NAME_IS_wingapi
    GXCloseDisplay();
#else
    DeleteDC( p_vout->p_sys->off_dc );
    DeleteObject( p_vout->p_sys->off_bitmap );
#endif
}

/*****************************************************************************
 * Manage: handle events
 *****************************************************************************
 * This function should be called regularly by video output thread. It manages
 * console events. It returns a non null value on error.
 *****************************************************************************/
static int Manage( vout_thread_t *p_vout )
{
    /*
     * Position Change
     */
    if( p_vout->p_sys->i_changes & DX_POSITION_CHANGE )
    {
        p_vout->p_sys->i_changes &= ~DX_POSITION_CHANGE;
    }

    CommonManage( p_vout );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Render: render previously calculated output
 *****************************************************************************/
static void Render( vout_thread_t *p_vout, picture_t *p_pic )
{
    /* No need to do anything, the fake direct buffers stay as they are */
    (void)p_vout;
    (void)p_pic;
}

/*****************************************************************************
 * Display: displays previously rendered output
 *****************************************************************************/
#define rect_src p_vout->p_sys->rect_src
#define rect_src_clipped p_vout->p_sys->rect_src_clipped
#define rect_dest p_vout->p_sys->rect_dest
#define rect_dest_clipped p_vout->p_sys->rect_dest_clipped

#ifndef MODULE_NAME_IS_wingapi
static void DisplayGDI( vout_thread_t *p_vout, picture_t *p_pic )
{
    VLC_UNUSED( p_pic );

    vout_sys_t *p_sys = p_vout->p_sys;
    RECT rect_dst = rect_dest_clipped;
    HDC hdc = GetDC( p_sys->hvideownd );

    OffsetRect( &rect_dst, -rect_dest.left, -rect_dest.top );
    SelectObject( p_sys->off_dc, p_sys->off_bitmap );

    if( rect_dest_clipped.right - rect_dest_clipped.left !=
        rect_src_clipped.right - rect_src_clipped.left ||
        rect_dest_clipped.bottom - rect_dest_clipped.top !=
        rect_src_clipped.bottom - rect_src_clipped.top )
    {
        StretchBlt( hdc, rect_dst.left, rect_dst.top,
                    rect_dst.right, rect_dst.bottom,
                    p_sys->off_dc, rect_src_clipped.left, rect_src_clipped.top,
                    rect_src_clipped.right, rect_src_clipped.bottom, SRCCOPY );
    }
    else
    {
        BitBlt( hdc, rect_dst.left, rect_dst.top,
                rect_dst.right, rect_dst.bottom,
                p_sys->off_dc, rect_src_clipped.left,
                rect_src_clipped.top, SRCCOPY );
    }

    ReleaseDC( p_sys->hvideownd, hdc );
}

static void FirstDisplayGDI( vout_thread_t *p_vout, picture_t *p_pic )
{
    /*
    ** Video window is initially hidden, show it now since we got a
    ** picture to show.
    */
    SetWindowPos( p_vout->p_sys->hvideownd, 0, 0, 0, 0, 0,
        SWP_ASYNCWINDOWPOS|
        SWP_FRAMECHANGED|
        SWP_SHOWWINDOW|
        SWP_NOMOVE|
        SWP_NOSIZE|
        SWP_NOZORDER );

    /* get initial picture presented */
    DisplayGDI(p_vout, p_pic);

    /* use and restores proper display function for further pictures */
    p_vout->pf_display = DisplayGDI;
}

#else

static int GAPILockSurface( vout_thread_t *p_vout, picture_t *p_pic )
{
    vout_sys_t *p_sys = p_vout->p_sys;
    int i_x, i_y, i_width, i_height;
    RECT video_rect;
    POINT point;

    GetClientRect( p_sys->hwnd, &video_rect);
    vout_PlacePicture( p_vout, video_rect.right - video_rect.left,
                       video_rect.bottom - video_rect.top,
                       &i_x, &i_y, &i_width, &i_height );
    point.x = point.y = 0;
    ClientToScreen( p_sys->hwnd, &point );
    i_x += point.x + video_rect.left;
    i_y += point.y + video_rect.top;

    if( i_width != p_vout->output.i_width ||
        i_height != p_vout->output.i_height )
    {
        GXDisplayProperties gxdisplayprop = GXGetDisplayProperties();

        p_sys->render_width = i_width;
        p_sys->render_height = i_height;
        p_vout->i_changes |= VOUT_SIZE_CHANGE;

        msg_Dbg( p_vout, "vout size change (%ix%i -> %ix%i)",
                 i_width, i_height, p_vout->output.i_width,
                 p_vout->output.i_height );

        p_vout->p_sys->i_pic_pixel_pitch = gxdisplayprop.cbxPitch;
        p_vout->p_sys->i_pic_pitch = gxdisplayprop.cbyPitch;
        return VLC_EGENERIC;
    }
    else
    {
        GXDisplayProperties gxdisplayprop;
        RECT display_rect, dest_rect;
        uint8_t *p_dest, *p_src = p_pic->p->p_pixels;

        video_rect.left = i_x; video_rect.top = i_y;
        video_rect.right = i_x + i_width;
        video_rect.bottom = i_y + i_height;

        gxdisplayprop = GXGetDisplayProperties();
        display_rect.left = 0; display_rect.top = 0;
        display_rect.right = gxdisplayprop.cxWidth;
        display_rect.bottom = gxdisplayprop.cyHeight;

        if( !IntersectRect( &dest_rect, &video_rect, &display_rect ) )
        {
            return VLC_EGENERIC;
        }

#ifndef NDEBUG
        msg_Dbg( p_vout, "video (%d,%d,%d,%d) display (%d,%d,%d,%d) "
                 "dest (%d,%d,%d,%d)",
                 video_rect.left, video_rect.right,
                 video_rect.top, video_rect.bottom,
                 display_rect.left, display_rect.right,
                 display_rect.top, display_rect.bottom,
                 dest_rect.left, dest_rect.right,
                 dest_rect.top, dest_rect.bottom );
#endif

        if( !(p_dest = GXBeginDraw()) )
        {
#ifndef NDEBUG
            msg_Err( p_vout, "GXBeginDraw error %d ", GetLastError() );
#endif
            return VLC_EGENERIC;
        }

        p_src += (dest_rect.left - video_rect.left) * gxdisplayprop.cbxPitch +
            (dest_rect.top - video_rect.top) * p_pic->p->i_pitch;
        p_dest += dest_rect.left * gxdisplayprop.cbxPitch +
            dest_rect.top * gxdisplayprop.cbyPitch;
        i_width = dest_rect.right - dest_rect.left;
        i_height = dest_rect.bottom - dest_rect.top;

        p_pic->p->p_pixels = p_dest;
    }

    return VLC_SUCCESS;
}

static int GAPIUnlockSurface( vout_thread_t *p_vout, picture_t *p_pic )
{
    GXEndDraw();
    return VLC_SUCCESS;
}

static void DisplayGAPI( vout_thread_t *p_vout, picture_t *p_pic )
{
}

static void FirstDisplayGAPI( vout_thread_t *p_vout, picture_t *p_pic )
{
    /* get initial picture presented through D3D */
    DisplayGAPI(p_vout, p_pic);

    /*
    ** Video window is initially hidden, show it now since we got a
    ** picture to show.
    */
    SetWindowPos( p_vout->p_sys->hvideownd, 0, 0, 0, 0, 0,
        SWP_ASYNCWINDOWPOS|
        SWP_FRAMECHANGED|
        SWP_SHOWWINDOW|
        SWP_NOMOVE|
        SWP_NOSIZE|
        SWP_NOZORDER );

    /* use and restores proper display function for further pictures */
    p_vout->pf_display = DisplayGAPI;
}

#endif

#undef rect_src
#undef rect_src_clipped
#undef rect_dest
#undef rect_dest_clipped
/*****************************************************************************
 * SetPalette: sets an 8 bpp palette
 *****************************************************************************/
static void SetPalette( vout_thread_t *p_vout,
                        uint16_t *red, uint16_t *green, uint16_t *blue )
{
    VLC_UNUSED( red ); VLC_UNUSED( green );VLC_UNUSED( blue );
    msg_Err( p_vout, "FIXME: SetPalette unimplemented" );
}

/*****************************************************************************
 * InitBuffers: initialize an offscreen bitmap for direct buffer operations.
 *****************************************************************************/
static void InitBuffers( vout_thread_t *p_vout )
{
    BITMAPINFOHEADER *p_header = &p_vout->p_sys->bitmapinfo.bmiHeader;
    BITMAPINFO *p_info = &p_vout->p_sys->bitmapinfo;
    HDC window_dc = GetDC( p_vout->p_sys->hvideownd );

    /* Get screen properties */
#ifdef MODULE_NAME_IS_wingapi
    GXDisplayProperties gx_displayprop = GXGetDisplayProperties();
    p_vout->p_sys->i_depth = gx_displayprop.cBPP;
#else
    p_vout->p_sys->i_depth = GetDeviceCaps( window_dc, PLANES ) *
        GetDeviceCaps( window_dc, BITSPIXEL );
#endif
    msg_Dbg( p_vout, "GDI depth is %i", p_vout->p_sys->i_depth );

#ifdef MODULE_NAME_IS_wingapi
    GXOpenDisplay( p_vout->p_sys->hvideownd, GX_FULLSCREEN );

#else

    /* Initialize offscreen bitmap */
    memset( p_info, 0, sizeof( BITMAPINFO ) + 3 * sizeof( RGBQUAD ) );

    p_header->biSize = sizeof( BITMAPINFOHEADER );
    p_header->biSizeImage = 0;
    p_header->biPlanes = 1;
    switch( p_vout->p_sys->i_depth )
    {
    case 8:
        p_header->biBitCount = 8;
        p_header->biCompression = BI_RGB;
        /* FIXME: we need a palette here */
        break;
    case 15:
        p_header->biBitCount = 15;
        p_header->biCompression = BI_BITFIELDS;//BI_RGB;
        ((DWORD*)p_info->bmiColors)[0] = 0x00007c00;
        ((DWORD*)p_info->bmiColors)[1] = 0x000003e0;
        ((DWORD*)p_info->bmiColors)[2] = 0x0000001f;
        break;
    case 16:
        p_header->biBitCount = 16;
        p_header->biCompression = BI_BITFIELDS;//BI_RGB;
        ((DWORD*)p_info->bmiColors)[0] = 0x0000f800;
        ((DWORD*)p_info->bmiColors)[1] = 0x000007e0;
        ((DWORD*)p_info->bmiColors)[2] = 0x0000001f;
        break;
    case 24:
        p_header->biBitCount = 24;
        p_header->biCompression = BI_RGB;
        ((DWORD*)p_info->bmiColors)[0] = 0x00ff0000;
        ((DWORD*)p_info->bmiColors)[1] = 0x0000ff00;
        ((DWORD*)p_info->bmiColors)[2] = 0x000000ff;
        break;
    case 32:
        p_header->biBitCount = 32;
        p_header->biCompression = BI_RGB;
        ((DWORD*)p_info->bmiColors)[0] = 0x00ff0000;
        ((DWORD*)p_info->bmiColors)[1] = 0x0000ff00;
        ((DWORD*)p_info->bmiColors)[2] = 0x000000ff;
        break;
    default:
        msg_Err( p_vout, "screen depth %i not supported",
                 p_vout->p_sys->i_depth );
        return;
        break;
    }
    p_header->biWidth = p_vout->render.i_width;
    p_header->biHeight = -p_vout->render.i_height;
    p_header->biClrImportant = 0;
    p_header->biClrUsed = 0;
    p_header->biXPelsPerMeter = 0;
    p_header->biYPelsPerMeter = 0;

    p_vout->p_sys->i_pic_pixel_pitch = p_header->biBitCount / 8;
    p_vout->p_sys->i_pic_pitch = p_header->biBitCount * p_header->biWidth / 8;

    p_vout->p_sys->off_bitmap =
        CreateDIBSection( window_dc, (BITMAPINFO *)p_header, DIB_RGB_COLORS,
                          &p_vout->p_sys->p_pic_buffer, NULL, 0 );

    p_vout->p_sys->off_dc = CreateCompatibleDC( window_dc );

    SelectObject( p_vout->p_sys->off_dc, p_vout->p_sys->off_bitmap );
    ReleaseDC( p_vout->p_sys->hvideownd, window_dc );
#endif
}

