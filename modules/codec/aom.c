/*****************************************************************************
 * aom.c: libaom decoder (AV1) module
 *****************************************************************************
 * Copyright (C) 2016 VLC authors and VideoLAN
 *
 * Authors: Tristan Matthews <tmatth@videolan.org>
 * Based on vpx.c by: Rafaël Carré <funman@videolan.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_codec.h>

#include <aom/aom_decoder.h>
#include <aom/aomdx.h>

#ifdef ENABLE_SOUT
# include <aom/aomcx.h>
# include <aom/aom_image.h>
#endif

/****************************************************************************
 * Local prototypes
 ****************************************************************************/
static int OpenDecoder(vlc_object_t *);
static void CloseDecoder(vlc_object_t *);
#ifdef ENABLE_SOUT
static int OpenEncoder(vlc_object_t *);
static void CloseEncoder(vlc_object_t *);
static block_t *Encode(encoder_t *p_enc, picture_t *p_pict);
#endif

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

vlc_module_begin ()
    set_shortname("aom")
    set_description(N_("AOM video decoder"))
    set_capability("video decoder", 100)
    set_callbacks(OpenDecoder, CloseDecoder)
    set_category(CAT_INPUT)
    set_subcategory(SUBCAT_INPUT_VCODEC)
#ifdef ENABLE_SOUT
    add_submodule()
    set_shortname("aom")
    set_capability("encoder", 60)
    set_description(N_("AOM video encoder"))
    set_callbacks(OpenEncoder, CloseEncoder)
#endif
vlc_module_end ()

static void aom_err_msg(vlc_object_t *this, aom_codec_ctx_t *ctx,
                        const char *msg)
{
    const char *error  = aom_codec_error(ctx);
    const char *detail = aom_codec_error_detail(ctx);
    if (!detail)
        detail = "no specific information";
    msg_Err(this, msg, error, detail);
}

#define AOM_ERR(this, ctx, msg) aom_err_msg(VLC_OBJECT(this), ctx, msg ": %s (%s)")

/*****************************************************************************
 * decoder_sys_t: libaom decoder descriptor
 *****************************************************************************/
struct decoder_sys_t
{
    aom_codec_ctx_t ctx;
};

static const struct
{
    vlc_fourcc_t     i_chroma;
    enum aom_img_fmt i_chroma_id;
    uint8_t          i_bitdepth;
    uint8_t          i_needs_hack;

} chroma_table[] =
{
    { VLC_CODEC_I420, AOM_IMG_FMT_I420, 8, 0 },
    { VLC_CODEC_I422, AOM_IMG_FMT_I422, 8, 0 },
    { VLC_CODEC_I444, AOM_IMG_FMT_I444, 8, 0 },
    { VLC_CODEC_I440, AOM_IMG_FMT_I440, 8, 0 },

    { VLC_CODEC_YV12, AOM_IMG_FMT_YV12, 8, 0 },
    { VLC_CODEC_YUVA, AOM_IMG_FMT_444A, 8, 0 },
    { VLC_CODEC_YUYV, AOM_IMG_FMT_YUY2, 8, 0 },
    { VLC_CODEC_UYVY, AOM_IMG_FMT_UYVY, 8, 0 },
    { VLC_CODEC_YVYU, AOM_IMG_FMT_YVYU, 8, 0 },

    { VLC_CODEC_RGB15, AOM_IMG_FMT_RGB555, 8, 0 },
    { VLC_CODEC_RGB16, AOM_IMG_FMT_RGB565, 8, 0 },
    { VLC_CODEC_RGB24, AOM_IMG_FMT_RGB24, 8, 0 },
    { VLC_CODEC_RGB32, AOM_IMG_FMT_RGB32, 8, 0 },

    { VLC_CODEC_ARGB, AOM_IMG_FMT_ARGB, 8, 0 },
    { VLC_CODEC_BGRA, AOM_IMG_FMT_ARGB_LE, 8, 0 },

    { VLC_CODEC_GBR_PLANAR, AOM_IMG_FMT_I444, 8, 1 },
    { VLC_CODEC_GBR_PLANAR_10L, AOM_IMG_FMT_I44416, 10, 1 },

    { VLC_CODEC_I420_10L, AOM_IMG_FMT_I42016, 10, 0 },
    { VLC_CODEC_I422_10L, AOM_IMG_FMT_I42216, 10, 0 },
    { VLC_CODEC_I444_10L, AOM_IMG_FMT_I44416, 10, 0 },

    { VLC_CODEC_I420_12L, AOM_IMG_FMT_I42016, 12, 0 },
    { VLC_CODEC_I422_12L, AOM_IMG_FMT_I42216, 12, 0 },
    { VLC_CODEC_I444_12L, AOM_IMG_FMT_I44416, 12, 0 },

    { VLC_CODEC_I444_16L, AOM_IMG_FMT_I44416, 16, 0 },
};

static vlc_fourcc_t FindVlcChroma( struct aom_image *img )
{
    uint8_t hack = (img->fmt & AOM_IMG_FMT_I444) && (img->cs == AOM_CS_SRGB);

    for( unsigned int i = 0; i < ARRAY_SIZE(chroma_table); i++ )
        if( chroma_table[i].i_chroma_id == img->fmt &&
            chroma_table[i].i_bitdepth == img->bit_depth &&
            chroma_table[i].i_needs_hack == hack )
            return chroma_table[i].i_chroma;

    return 0;
}

/****************************************************************************
 * Decode: the whole thing
 ****************************************************************************/
static int Decode(decoder_t *dec, block_t *block)
{
    aom_codec_ctx_t *ctx = &dec->p_sys->ctx;

    if (!block) /* No Drain */
        return VLCDEC_SUCCESS;

    if (block->i_flags & (BLOCK_FLAG_CORRUPTED)) {
        block_Release(block);
        return VLCDEC_SUCCESS;
    }

    /* Associate packet PTS with decoded frame */
    mtime_t *pkt_pts = malloc(sizeof(*pkt_pts));
    if (!pkt_pts) {
        block_Release(block);
        return VLCDEC_SUCCESS;
    }

    *pkt_pts = block->i_pts;

    aom_codec_err_t err;
    err = aom_codec_decode(ctx, block->p_buffer, block->i_buffer, pkt_pts, 0);

    block_Release(block);

    if (err != AOM_CODEC_OK) {
        free(pkt_pts);
        AOM_ERR(dec, ctx, "Failed to decode frame");
        return VLCDEC_SUCCESS;
    }

    const void *iter = NULL;
    struct aom_image *img = aom_codec_get_frame(ctx, &iter);
    if (!img) {
        free(pkt_pts);
        return VLCDEC_SUCCESS;
    }

    /* fetches back the PTS */
    pkt_pts = img->user_priv;
    mtime_t pts = *pkt_pts;
    free(pkt_pts);

    dec->fmt_out.i_codec = FindVlcChroma(img);
    if (dec->fmt_out.i_codec == 0) {
        msg_Err(dec, "Unsupported output colorspace %d", img->fmt);
        return VLCDEC_SUCCESS;
    }

    video_format_t *v = &dec->fmt_out.video;

    if (img->d_w != v->i_visible_width || img->d_h != v->i_visible_height) {
        v->i_visible_width = img->d_w;
        v->i_visible_height = img->d_h;
    }

    if( !dec->fmt_out.video.i_sar_num || !dec->fmt_out.video.i_sar_den )
    {
        dec->fmt_out.video.i_sar_num = 1;
        dec->fmt_out.video.i_sar_den = 1;
    }

    v->b_color_range_full = img->range == AOM_CR_FULL_RANGE;

    switch( img->cs )
    {
        case AOM_CS_SRGB:
        case AOM_CS_BT_709:
            v->space = COLOR_SPACE_BT709;
            break;
        case AOM_CS_BT_601:
        case AOM_CS_SMPTE_170:
        case AOM_CS_SMPTE_240:
            v->space = COLOR_SPACE_BT601;
            break;
        case AOM_CS_BT_2020_CL:
        case AOM_CS_BT_2020_NCL:
            v->space = COLOR_SPACE_BT2020;
            break;
        default:
            break;
    }

    if (decoder_UpdateVideoFormat(dec))
        return VLCDEC_SUCCESS;
    picture_t *pic = decoder_NewPicture(dec);
    if (!pic)
        return VLCDEC_SUCCESS;

    for (int plane = 0; plane < pic->i_planes; plane++ ) {
        uint8_t *src = img->planes[plane];
        uint8_t *dst = pic->p[plane].p_pixels;
        int src_stride = img->stride[plane];
        int dst_stride = pic->p[plane].i_pitch;

        int size = __MIN( src_stride, dst_stride );
        for( int line = 0; line < pic->p[plane].i_visible_lines; line++ ) {
            memcpy( dst, src, size );
            src += src_stride;
            dst += dst_stride;
        }
    }

    pic->b_progressive = true; /* codec does not support interlacing */
    pic->date = pts;

    decoder_QueueVideo(dec, pic);
    return VLCDEC_SUCCESS;
}

/*****************************************************************************
 * OpenDecoder: probe the decoder
 *****************************************************************************/
static int OpenDecoder(vlc_object_t *p_this)
{
    decoder_t *dec = (decoder_t *)p_this;
    const aom_codec_iface_t *iface;
    int av_version;

    if (dec->fmt_in.i_codec != VLC_CODEC_AV1)
        return VLC_EGENERIC;

    iface = &aom_codec_av1_dx_algo;
    av_version = 1;

    decoder_sys_t *sys = malloc(sizeof(*sys));
    if (!sys)
        return VLC_ENOMEM;
    dec->p_sys = sys;

    struct aom_codec_dec_cfg deccfg = {
        .threads = __MIN(vlc_GetCPUCount(), 16),
        .allow_lowbitdepth = 1
    };

    msg_Dbg(p_this, "AV%d: using libaom version %s (build options %s)",
        av_version, aom_codec_version_str(), aom_codec_build_config());

    if (aom_codec_dec_init(&sys->ctx, iface, &deccfg, 0) != AOM_CODEC_OK) {
        AOM_ERR(p_this, &sys->ctx, "Failed to initialize decoder");
        free(sys);
        return VLC_EGENERIC;;
    }

    dec->pf_decode = Decode;

    dec->fmt_out.video.i_width = dec->fmt_in.video.i_width;
    dec->fmt_out.video.i_height = dec->fmt_in.video.i_height;
    dec->fmt_out.i_codec = VLC_CODEC_I420;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * CloseDecoder: decoder destruction
 *****************************************************************************/
static void CloseDecoder(vlc_object_t *p_this)
{
    decoder_t *dec = (decoder_t *)p_this;
    decoder_sys_t *sys = dec->p_sys;

    /* Free our PTS */
    const void *iter = NULL;
    for (;;) {
        struct aom_image *img = aom_codec_get_frame(&sys->ctx, &iter);
        if (!img)
            break;
        free(img->user_priv);
    }

    aom_codec_destroy(&sys->ctx);

    free(sys);
}

#ifdef ENABLE_SOUT

/*****************************************************************************
 * encoder_sys_t: libaom encoder descriptor
 *****************************************************************************/
struct encoder_sys_t
{
    struct aom_codec_ctx ctx;
};

/*****************************************************************************
 * OpenEncoder: probe the encoder
 *****************************************************************************/
static int OpenEncoder(vlc_object_t *p_this)
{
    encoder_t *p_enc = (encoder_t *)p_this;
    encoder_sys_t *p_sys;

    if (p_enc->fmt_out.i_codec != VLC_CODEC_AV1)
        return VLC_EGENERIC;

    /* Allocate the memory needed to store the encoder's structure */
    p_sys = malloc(sizeof(*p_sys));
    if (p_sys == NULL)
        return VLC_ENOMEM;

    p_enc->p_sys = p_sys;

    const struct aom_codec_iface *iface = &aom_codec_av1_cx_algo;

    struct aom_codec_enc_cfg enccfg = {};
    aom_codec_enc_config_default(iface, &enccfg, 0);
    enccfg.g_threads = __MIN(vlc_GetCPUCount(), 4);
    enccfg.g_w = p_enc->fmt_in.video.i_visible_width;
    enccfg.g_h = p_enc->fmt_in.video.i_visible_height;

    int enc_flags;
    switch (p_enc->fmt_in.i_codec) {
        case VLC_CODEC_I420_10L:
            enc_flags = AOM_CODEC_USE_HIGHBITDEPTH;
            /* Profile 1: 10-bit and 12-bit color only, with 4:2:0 sampling. */
            enccfg.g_profile = 2;
            enccfg.g_bit_depth = 10;
            break;
        case VLC_CODEC_I420:
            enc_flags = 0;
            /* Profile 0: 8-bit 4:2:0 only. */
            enccfg.g_profile = 0;
            enccfg.g_bit_depth = 8;
            break;
        default:
            msg_Err(p_this, "Unsupported input format %s",
                    vlc_fourcc_GetDescription(VIDEO_ES, p_enc->fmt_in.i_codec));
            free(p_sys);
            return VLC_EGENERIC;
    }

    msg_Dbg(p_this, "AV1: using libaom version %s (build options %s)",
        aom_codec_version_str(), aom_codec_build_config());

    struct aom_codec_ctx *ctx = &p_sys->ctx;
    if (aom_codec_enc_init(ctx, iface, &enccfg, enc_flags) != AOM_CODEC_OK)
    {
        AOM_ERR(p_this, ctx, "Failed to initialize encoder");
        free(p_sys);
        return VLC_EGENERIC;
    }

    p_enc->pf_encode_video = Encode;

    return VLC_SUCCESS;
}

/****************************************************************************
 * Encode: the whole thing
 ****************************************************************************/
static block_t *Encode(encoder_t *p_enc, picture_t *p_pict)
{
    encoder_sys_t *p_sys = p_enc->p_sys;
    struct aom_codec_ctx *ctx = &p_sys->ctx;

    if (!p_pict) return NULL;

    aom_image_t img = {};
    unsigned i_w = p_enc->fmt_in.video.i_visible_width;
    unsigned i_h = p_enc->fmt_in.video.i_visible_height;
    const aom_img_fmt_t img_fmt = p_enc->fmt_in.i_codec == VLC_CODEC_I420_10L ?
        AOM_IMG_FMT_I42016 : AOM_IMG_FMT_I420;

    /* Create and initialize the aom_image */
    if (!aom_img_alloc(&img, img_fmt, i_w, i_h, 16))
    {
        AOM_ERR(p_enc, ctx, "Failed to allocate image");
        return NULL;
    }

    for (int plane = 0; plane < p_pict->i_planes; plane++) {
        uint8_t *src = p_pict->p[plane].p_pixels;
        uint8_t *dst = img.planes[plane];
        int src_stride = p_pict->p[plane].i_pitch;
        int dst_stride = img.stride[plane];

        int size = __MIN(src_stride, dst_stride);
        for (int line = 0; line < p_pict->p[plane].i_visible_lines; line++)
        {
            /* FIXME: do this in-place */
            memcpy(dst, src, size);
            src += src_stride;
            dst += dst_stride;
        }
    }

    aom_codec_err_t res = aom_codec_encode(ctx, &img, p_pict->date, 1, 0,
                                           AOM_DL_GOOD_QUALITY);
    if (res != AOM_CODEC_OK) {
        AOM_ERR(p_enc, ctx, "Failed to encode frame");
        aom_img_free(&img);
        return NULL;
    }

    const aom_codec_cx_pkt_t *pkt = NULL;
    aom_codec_iter_t iter = NULL;
    block_t *p_out = NULL;
    while ((pkt = aom_codec_get_cx_data(ctx, &iter)) != NULL)
    {
        if (pkt->kind == AOM_CODEC_CX_FRAME_PKT)
        {
            int keyframe = pkt->data.frame.flags & AOM_FRAME_IS_KEY;
            block_t *p_block = block_Alloc(pkt->data.frame.sz);
            if (unlikely(p_block == NULL)) {
                block_ChainRelease(p_out);
                p_out = NULL;
                break;
            }

            /* FIXME: do this in-place */
            memcpy(p_block->p_buffer, pkt->data.frame.buf, pkt->data.frame.sz);
            p_block->i_dts = p_block->i_pts = pkt->data.frame.pts;
            if (keyframe)
                p_block->i_flags |= BLOCK_FLAG_TYPE_I;
            block_ChainAppend(&p_out, p_block);
        }
    }
    aom_img_free(&img);
    return p_out;
}

/*****************************************************************************
 * CloseEncoder: encoder destruction
 *****************************************************************************/
static void CloseEncoder(vlc_object_t *p_this)
{
    encoder_t *p_enc = (encoder_t *)p_this;
    encoder_sys_t *p_sys = p_enc->p_sys;
    if (aom_codec_destroy(&p_sys->ctx))
        AOM_ERR(p_this, &p_sys->ctx, "Failed to destroy codec");
    free(p_sys);
}

#endif  /* ENABLE_SOUT */
