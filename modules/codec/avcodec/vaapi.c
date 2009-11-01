/*****************************************************************************
 * vaapi.c: VAAPI helpers for the ffmpeg decoder
 *****************************************************************************
 * Copyright (C) 2009 Laurent Aimar
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir_AT_ videolan _DOT_ org>
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_vout.h>
#include <assert.h>

#ifdef HAVE_LIBAVCODEC_AVCODEC_H
#   include <libavcodec/avcodec.h>
#   ifdef HAVE_AVCODEC_VAAPI
#       include <libavcodec/vaapi.h>
#   endif
#elif defined(HAVE_FFMPEG_AVCODEC_H)
#   include <ffmpeg/avcodec.h>
#else
#   include <avcodec.h>
#endif

#include "avcodec.h"
#include "vaapi.h"

#ifdef HAVE_AVCODEC_VAAPI

#include <X11/Xlib.h>
#include <va/va_x11.h>


typedef struct
{
    VASurfaceID  i_id;
    int          i_refcount;
    unsigned int i_order;

} vlc_va_surface_t;

struct vlc_va_t
{
    /* */
    Display      *p_display_x11;
    VADisplay     p_display;

    VAConfigID    i_config_id;
    VAContextID   i_context_id;

    struct vaapi_context hw_ctx;

    /* */
    int i_version_major;
    int i_version_minor;

    /* */
    int          i_surface_count;
    unsigned int i_surface_order;
    int          i_surface_width;
    int          i_surface_height;
    vlc_fourcc_t i_surface_chroma;

    vlc_va_surface_t *p_surface;

    VAImage      image;

};

static int VaOpen( vlc_va_t *p_va, int i_codec_id );
static void VaClose( vlc_va_t *p_va );

static int VaCreateSurfaces( vlc_va_t *p_va, void **pp_hw_ctx, vlc_fourcc_t *pi_chroma,
                             int i_width, int i_height );
static void VaDestroySurfaces( vlc_va_t *p_va );

vlc_va_t *VaNew( int i_codec_id )
{
    vlc_va_t *p_va = calloc( 1, sizeof(*p_va) );
    if( !p_va )
        return NULL;

    if( VaOpen( p_va, i_codec_id ) )
    {
        free( p_va );
        return NULL;
    }
    return p_va;
}
void VaDelete( vlc_va_t *p_va )
{
    VaClose( p_va );
    free( p_va );
}
int VaSetup( vlc_va_t *p_va, void **pp_hw_ctx, vlc_fourcc_t *pi_chroma,
             int i_width, int i_height )
{
    if( p_va->i_surface_width == i_width &&
        p_va->i_surface_height == i_height )
        return VLC_SUCCESS;

    *pp_hw_ctx = NULL;
    *pi_chroma = 0;
    if( p_va->i_surface_width || p_va->i_surface_height )
        VaDestroySurfaces( p_va );

    if( i_width > 0 && i_height > 0 )
        return VaCreateSurfaces( p_va, pp_hw_ctx, pi_chroma, i_width, i_height );

    return VLC_EGENERIC;
}
void VaVersion( vlc_va_t *p_va, char *psz_version, size_t i_version )
{
    snprintf( psz_version, i_version, "%d.%d", p_va->i_version_major, p_va->i_version_minor );
}

static int VaOpen( vlc_va_t *p_va, int i_codec_id )
{
    VAProfile i_profile;
    int i_surface_count;

    /* */
    switch( i_codec_id )
    {
    case CODEC_ID_MPEG1VIDEO:
    case CODEC_ID_MPEG2VIDEO:
        i_profile = VAProfileMPEG2Main;
        i_surface_count = 2+1;
        break;
    case CODEC_ID_MPEG4:
        i_profile = VAProfileMPEG4AdvancedSimple;
        i_surface_count = 2+1;
        break;
    case CODEC_ID_WMV3:
        i_profile = VAProfileVC1Main;
        i_surface_count = 2+1;
        break;
    case CODEC_ID_VC1:
        i_profile = VAProfileVC1Advanced;
        i_surface_count = 2+1;
        break;
    case CODEC_ID_H264:
        i_profile = VAProfileH264High;
        i_surface_count = 16+1;
        break;
    default:
        return VLC_EGENERIC;
    }

    /* */
    memset( p_va, 0, sizeof(*p_va) );

    /* Create a VA display */
    p_va->p_display_x11 = XOpenDisplay(NULL);
    if( !p_va->p_display_x11 )
        goto error;

    p_va->p_display = vaGetDisplay( p_va->p_display_x11 );
    if( !p_va->p_display )
        goto error;

    if( vaInitialize( p_va->p_display, &p_va->i_version_major, &p_va->i_version_minor ) )
        goto error;

    /* Create a VA configuration */
    VAConfigAttrib attrib;
    memset( &attrib, 0, sizeof(attrib) );
    attrib.type = VAConfigAttribRTFormat;
    if( vaGetConfigAttributes( p_va->p_display,
                               i_profile, VAEntrypointVLD, &attrib, 1 ) )
        goto error;

    /* Not sure what to do if not, I don't have a way to test */
    if( (attrib.value & VA_RT_FORMAT_YUV420) == 0 )
        goto error;
    if( vaCreateConfig( p_va->p_display,
                        i_profile, VAEntrypointVLD, &attrib, 1, &p_va->i_config_id ) )
    {
        p_va->i_config_id = 0;
        goto error;
    }

    p_va->i_surface_count = i_surface_count;

    return VLC_SUCCESS;

error:
    return VLC_EGENERIC;
}
static void VaClose( vlc_va_t *p_va )
{
    if( p_va->i_surface_width || p_va->i_surface_height )
        VaDestroySurfaces( p_va );

    if( p_va->i_config_id )
        vaDestroyConfig( p_va->p_display, p_va->i_config_id );
    if( p_va->p_display )
        vaTerminate( p_va->p_display );
    if( p_va->p_display_x11 )
        XCloseDisplay( p_va->p_display_x11 );
}

static int VaCreateSurfaces( vlc_va_t *p_va, void **pp_hw_ctx, vlc_fourcc_t *pi_chroma,
                             int i_width, int i_height )
{
    assert( i_width > 0 && i_height > 0 );

    /* */
    p_va->p_surface = calloc( p_va->i_surface_count, sizeof(*p_va->p_surface) );
    if( !p_va->p_surface )
        return VLC_EGENERIC;

    /* Create surfaces */
    VASurfaceID pi_surface_id[p_va->i_surface_count];
    if( vaCreateSurfaces( p_va->p_display, i_width, i_height, VA_RT_FORMAT_YUV420,
                          p_va->i_surface_count, pi_surface_id ) )
    {
        for( int i = 0; i < p_va->i_surface_count; i++ )
            p_va->p_surface[i].i_id = VA_INVALID_SURFACE;
        goto error;
    }

    for( int i = 0; i < p_va->i_surface_count; i++ )
    {
        vlc_va_surface_t *p_surface = &p_va->p_surface[i];

        p_surface->i_id = pi_surface_id[i];
        p_surface->i_refcount = 0;
        p_surface->i_order = 0;
    }

    /* Create a context */
    if( vaCreateContext( p_va->p_display, p_va->i_config_id,
                         i_width, i_height, VA_PROGRESSIVE,
                         pi_surface_id, p_va->i_surface_count, &p_va->i_context_id ) )
    {
        p_va->i_context_id = 0;
        goto error;
    }

    /* Find a supported image chroma */
    int i_fmt_count = vaMaxNumImageFormats( p_va->p_display );
    VAImageFormat *p_fmt = calloc( i_fmt_count, sizeof(*p_fmt) );
    if( !p_fmt )
        goto error;

    if( vaQueryImageFormats( p_va->p_display, p_fmt, &i_fmt_count ) )
    {
        free( p_fmt );
        goto error;
    }

    vlc_fourcc_t  i_chroma = 0;
    VAImageFormat fmt;
    for( int i = 0; i < i_fmt_count; i++ )
    {
        if( p_fmt[i].fourcc == VA_FOURCC( 'Y', 'V', '1', '2' ) ||
            p_fmt[i].fourcc == VA_FOURCC( 'I', '4', '2', '0' ) )
        {
            i_chroma = VLC_FOURCC( 'I', '4', '2', '0' );
            fmt = p_fmt[i];
        }
        /* TODO: It seems that these may also be available (but not
         * with my setup):
         * VA_FOURCC( 'N', 'V', '1', '2')
         * VA_FOURCC( 'U', 'Y', 'V', 'Y')
         * VA_FOURCC( 'Y', 'U', 'Y', 'V')
         */
    }
    free( p_fmt );
    if( !i_chroma )
        goto error;
    *pi_chroma = i_chroma;

    /* Create an image for surface extraction */
    if( vaCreateImage(  p_va->p_display, &fmt, i_width, i_height, &p_va->image ) )
    {
        p_va->image.image_id = 0;
        goto error;
    }

    /* Setup the ffmpeg hardware context */
    *pp_hw_ctx = &p_va->hw_ctx;

    memset( &p_va->hw_ctx, 0, sizeof(p_va->hw_ctx) );
    p_va->hw_ctx.display    = p_va->p_display;
    p_va->hw_ctx.config_id  = p_va->i_config_id;
    p_va->hw_ctx.context_id = p_va->i_context_id;

    /* */
    p_va->i_surface_chroma = i_chroma;
    p_va->i_surface_width = i_width;
    p_va->i_surface_height = i_height;
    return VLC_SUCCESS;

error:
    VaDestroySurfaces( p_va );
    return VLC_EGENERIC;
}
static void VaDestroySurfaces( vlc_va_t *p_va )
{
    if( p_va->image.image_id )
        vaDestroyImage( p_va->p_display, p_va->image.image_id );

    if( p_va->i_context_id )
        vaDestroyContext( p_va->p_display, p_va->i_context_id );

    for( int i = 0; i < p_va->i_surface_count && p_va->p_surface; i++ )
    {
        vlc_va_surface_t *p_surface = &p_va->p_surface[i];

        if( p_surface->i_id != VA_INVALID_SURFACE )
            vaDestroySurfaces( p_va->p_display, &p_surface->i_id, 1 );
    }
    free( p_va->p_surface );

    /* */
    p_va->image.image_id = 0;
    p_va->i_context_id = 0;
    p_va->p_surface = NULL;
    p_va->i_surface_width = 0;
    p_va->i_surface_height = 0;
}

int VaExtract( vlc_va_t *p_va, picture_t *p_picture, AVFrame *p_ff )
{
    VASurfaceID i_surface_id = (VASurfaceID)(uintptr_t)p_ff->data[3];

#if VA_CHECK_VERSION(0,31,0)
    if( vaSyncSurface( p_va->p_display, i_surface_id ) )
#else
    if( vaSyncSurface( p_va->p_display, p_va->i_context_id, i_surface_id ) )
#endif
        return VLC_EGENERIC;

    /* XXX vaDeriveImage may be better but it is not supported by
     * my setup.
     */

    if( vaGetImage( p_va->p_display, i_surface_id,
                    0, 0, p_va->i_surface_width, p_va->i_surface_height,
                    p_va->image.image_id) )
        return VLC_EGENERIC;

    void *p_base;
    if( vaMapBuffer( p_va->p_display, p_va->image.buf, &p_base ) )
        return VLC_EGENERIC;

    for( int i_plane = 0; i_plane < p_picture->i_planes; i_plane++ )
    {
        const int i_src_plane = ((p_va->image.format.fourcc == VA_FOURCC('Y','V','1','2' )) && i_plane != 0) ?  (3 - i_plane) : i_plane;
        const uint8_t *p_src = (uint8_t*)p_base + p_va->image.offsets[i_src_plane];
        const int i_src_stride = p_va->image.pitches[i_src_plane];

        uint8_t *p_dst = p_picture->p[i_plane].p_pixels;
        const int i_dst_stride = p_picture->p[i_plane].i_pitch;

        if( i_src_stride != i_dst_stride )
        {
            for( int i = 0; i < p_picture->p[i_plane].i_visible_lines; i++ )
            {
                vlc_memcpy( p_dst, p_src, __MIN( i_src_stride, i_dst_stride ) );
                p_src += i_src_stride;
                p_dst += i_dst_stride;
            }
        }
        else
        {
            vlc_memcpy( p_dst, p_src, p_picture->p[i_plane].i_visible_lines * i_src_stride );
        }
    }

    if( vaUnmapBuffer( p_va->p_display, p_va->image.buf ) )
        return VLC_EGENERIC;

    return VLC_SUCCESS;
}
int VaGrabSurface( vlc_va_t *p_va, AVFrame *p_ff )
{
    int i_old;
    int i;

    /* Grab an unused surface, in case none are, try the oldest
     * XXX using the oldest is a workaround in case a problem happens with ffmpeg */
    for( i = 0, i_old = 0; i < p_va->i_surface_count; i++ )
    {
        vlc_va_surface_t *p_surface = &p_va->p_surface[i];

        if( !p_surface->i_refcount )
            break;

        if( p_surface->i_order < p_va->p_surface[i_old].i_order )
            i_old = i;
    }
    if( i >= p_va->i_surface_count )
        i = i_old;

    vlc_va_surface_t *p_surface = &p_va->p_surface[i];

    p_surface->i_refcount = 1;
    p_surface->i_order = p_va->i_surface_order++;

    /* */
    for( int i = 0; i < 4; i++ )
    {
        p_ff->data[i] = NULL;
        p_ff->linesize[i] = 0;

        if( i == 0 || i == 3 )
            p_ff->data[i] = (void*)(uintptr_t)p_surface->i_id;/* Yummie */
    }
    return VLC_SUCCESS;
}
void VaUngrabSurface( vlc_va_t *p_va, AVFrame *p_ff )
{
    VASurfaceID i_surface_id = (VASurfaceID)(uintptr_t)p_ff->data[3];

    for( int i = 0; i < p_va->i_surface_count; i++ )
    {
        vlc_va_surface_t *p_surface = &p_va->p_surface[i];

        if( p_surface->i_id == i_surface_id )
            p_surface->i_refcount--;
    }
}

#else

vlc_va_t *VaNew( int i_codec_id )
{
    VLC_UNUSED(i_codec_id);
    return NULL;
}
void VaDelete( vlc_va_t *p_va )
{
    VLC_UNUSED(p_va);
    assert( 0 );
}

void VaVersion( vlc_va_t *p_va, char *psz_version, size_t i_version )
{
    VLC_UNUSED(p_va); VLC_UNUSED(psz_version); VLC_UNUSED(i_version);
    assert(0);
}

int VaSetup( vlc_va_t *p_va, void **pp_hw_ctx, vlc_fourcc_t *pi_chroma,
             int i_width, int i_height )
{
    VLC_UNUSED(p_va); VLC_UNUSED(pp_hw_ctx); VLC_UNUSED(pi_chroma);
    VLC_UNUSED(i_width); VLC_UNUSED(i_height);
    assert(0);
    return -1;
}

int VaExtract( vlc_va_t *p_va, picture_t *p_picture, AVFrame *p_ff )
{
    VLC_UNUSED(p_va); VLC_UNUSED(p_picture); VLC_UNUSED(p_ff);
    assert(0);
    return -1;
}

int VaGrabSurface( vlc_va_t *p_va, AVFrame *p_ff )
{
    VLC_UNUSED(p_va); VLC_UNUSED(p_ff);
    assert(0);
    return -1;
}

void VaUngrabSurface( vlc_va_t *p_va, AVFrame *p_ff )
{
    VLC_UNUSED(p_va); VLC_UNUSED(p_ff);
    assert(0);
}

#endif
