/*****************************************************************************
 * vlc_aout.h : audio output interface
 *****************************************************************************
 * Copyright (C) 2002-2011 VLC authors and VideoLAN
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
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

#ifndef VLC_AOUT_H
#define VLC_AOUT_H 1

/**
 * \file
 * This file defines functions, structures and macros for audio output object
 */

/* Buffers which arrive in advance of more than AOUT_MAX_ADVANCE_TIME
 * will be considered as bogus and be trashed */
#define AOUT_MAX_ADVANCE_TIME           (AOUT_MAX_PREPARE_TIME + CLOCK_FREQ)

/* Buffers which arrive in advance of more than AOUT_MAX_PREPARE_TIME
 * will cause the calling thread to sleep */
#define AOUT_MAX_PREPARE_TIME           (2 * CLOCK_FREQ)

/* Buffers which arrive after pts - AOUT_MIN_PREPARE_TIME will be trashed
 * to avoid too heavy resampling */
#define AOUT_MIN_PREPARE_TIME           AOUT_MAX_PTS_ADVANCE

/* Tolerance values from EBU Recommendation 37 */
/** Maximum advance of actual audio playback time to coded PTS,
 * above which downsampling will be performed */
#define AOUT_MAX_PTS_ADVANCE            (CLOCK_FREQ / 25)

/** Maximum delay of actual audio playback time from coded PTS,
 * above which upsampling will be performed */
#define AOUT_MAX_PTS_DELAY              (3 * CLOCK_FREQ / 50)

/* Max acceptable resampling (in %) */
#define AOUT_MAX_RESAMPLING             10

#include "vlc_es.h"

#define AOUT_FMTS_IDENTICAL( p_first, p_second ) (                          \
    ((p_first)->i_format == (p_second)->i_format)                           \
      && AOUT_FMTS_SIMILAR(p_first, p_second) )

/* Check if i_rate == i_rate and i_channels == i_channels */
#define AOUT_FMTS_SIMILAR( p_first, p_second ) (                            \
    ((p_first)->i_rate == (p_second)->i_rate)                               \
      && ((p_first)->i_physical_channels == (p_second)->i_physical_channels)\
      && ((p_first)->i_original_channels == (p_second)->i_original_channels) )

#define AOUT_FMT_LINEAR( p_format ) \
    (aout_BitsPerSample((p_format)->i_format) != 0)

#define VLC_CODEC_SPDIFL VLC_FOURCC('s','p','d','i')
#define VLC_CODEC_SPDIFB VLC_FOURCC('s','p','d','b')

#define AOUT_FMT_SPDIF( p_format ) \
    ( ((p_format)->i_format == VLC_CODEC_SPDIFL)       \
       || ((p_format)->i_format == VLC_CODEC_SPDIFB)   \
       || ((p_format)->i_format == VLC_CODEC_A52)       \
       || ((p_format)->i_format == VLC_CODEC_DTS) )

/* This is heavily borrowed from libmad, by Robert Leslie <rob@mars.org> */
/*
 * Fixed-point format: 0xABBBBBBB
 * A == whole part      (sign + 3 bits)
 * B == fractional part (28 bits)
 *
 * Values are signed two's complement, so the effective range is:
 * 0x80000000 to 0x7fffffff
 *       -8.0 to +7.9999999962747097015380859375
 *
 * The smallest representable value is:
 * 0x00000001 == 0.0000000037252902984619140625 (i.e. about 3.725e-9)
 *
 * 28 bits of fractional accuracy represent about
 * 8.6 digits of decimal accuracy.
 *
 * Fixed-point numbers can be added or subtracted as normal
 * integers, but multiplication requires shifting the 64-bit result
 * from 56 fractional bits back to 28 (and rounding.)
 */
typedef int32_t vlc_fixed_t;
#define FIXED32_FRACBITS 28
#define FIXED32_MIN ((vlc_fixed_t) -0x80000000L)
#define FIXED32_MAX ((vlc_fixed_t) +0x7fffffffL)
#define FIXED32_ONE ((vlc_fixed_t) 0x10000000)

/* Values used for the audio-device and audio-channels object variables */
#define AOUT_VAR_MONO               1
#define AOUT_VAR_STEREO             2
#define AOUT_VAR_2F2R               4
#define AOUT_VAR_3F2R               5
#define AOUT_VAR_5_1                6
#define AOUT_VAR_6_1                7
#define AOUT_VAR_7_1                8
#define AOUT_VAR_SPDIF              10

#define AOUT_VAR_CHAN_UNSET         0 /* must be zero */
#define AOUT_VAR_CHAN_STEREO        1
#define AOUT_VAR_CHAN_RSTEREO       2
#define AOUT_VAR_CHAN_LEFT          3
#define AOUT_VAR_CHAN_RIGHT         4
#define AOUT_VAR_CHAN_DOLBYS        5

/*****************************************************************************
 * Main audio output structures
 *****************************************************************************/

/* Size of a frame for S/PDIF output. */
#define AOUT_SPDIF_SIZE 6144

/* Number of samples in an A/52 frame. */
#define A52_FRAME_NB 1536

/* FIXME to remove once aout.h is cleaned a bit more */
#include <vlc_block.h>

/** Audio output object */
struct audio_output
{
    VLC_COMMON_MEMBERS

    struct aout_sys_t *sys; /**< Output plugin private data */
    int (*start) (audio_output_t *, audio_sample_format_t *);
    void (*stop) (audio_output_t *);
    void (*play)(audio_output_t *, block_t *, mtime_t *); /**< Play callback
        - queue a block for playback */
    void (*pause)( audio_output_t *, bool, mtime_t ); /**< Pause/resume
        callback (optional, may be NULL) */
    void (*flush)( audio_output_t *, bool ); /**< Flush/drain callback
        (optional, may be NULL) */
    int (*volume_set)(audio_output_t *, float); /**< Volume setter (or NULL) */
    int (*mute_set)(audio_output_t *, bool); /**< Mute setter (or NULL) */

    struct {
        void (*volume_report)(audio_output_t *, float);
        void (*mute_report)(audio_output_t *, bool);
        void (*policy_report)(audio_output_t *, bool);
        int (*gain_request)(audio_output_t *, float);
    } event;
};

/**
 * It describes the audio channel order VLC expect.
 */
static const uint32_t pi_vlc_chan_order_wg4[] =
{
    AOUT_CHAN_LEFT, AOUT_CHAN_RIGHT,
    AOUT_CHAN_MIDDLELEFT, AOUT_CHAN_MIDDLERIGHT,
    AOUT_CHAN_REARLEFT, AOUT_CHAN_REARRIGHT, AOUT_CHAN_REARCENTER,
    AOUT_CHAN_CENTER, AOUT_CHAN_LFE, 0
};

/*****************************************************************************
 * Prototypes
 *****************************************************************************/

/**
 * This function computes the reordering needed to go from pi_chan_order_in to
 * pi_chan_order_out.
 * If pi_chan_order_in or pi_chan_order_out is NULL, it will assume that vlc
 * internal (WG4) order is requested.
 */
VLC_API int aout_CheckChannelReorder( const uint32_t *pi_chan_order_in, const uint32_t *pi_chan_order_out, uint32_t i_channel_mask, int i_channels, int *pi_chan_table );
VLC_API void aout_ChannelReorder( void *, size_t, unsigned, const int *, unsigned );

/**
 * This fonction will compute the extraction parameter into pi_selection to go
 * from i_channels with their type given by pi_order_src[] into the order
 * describe by pi_order_dst.
 * It will also set :
 * - *pi_channels as the number of channels that will be extracted which is
 * lower (in case of non understood channels type) or equal to i_channels.
 * - the layout of the channels (*pi_layout).
 *
 * It will return true if channel extraction is really needed, in which case
 * aout_ChannelExtract must be used
 *
 * XXX It must be used when the source may have channel type not understood
 * by VLC. In this case the channel type pi_order_src[] must be set to 0.
 * XXX It must also be used if multiple channels have the same type.
 */
VLC_API bool aout_CheckChannelExtraction( int *pi_selection, uint32_t *pi_layout, int *pi_channels, const uint32_t pi_order_dst[AOUT_CHAN_MAX], const uint32_t *pi_order_src, int i_channels );

/**
 * Do the actual channels extraction using the parameters created by
 * aout_CheckChannelExtraction.
 *
 * XXX this function does not work in place (p_dst and p_src must not overlap).
 * XXX Only 8, 16, 24, 32, 64 bits per sample are supported.
 */
VLC_API void aout_ChannelExtract( void *p_dst, int i_dst_channels, const void *p_src, int i_src_channels, int i_sample_count, const int *pi_selection, int i_bits_per_sample );

/* */
static inline unsigned aout_FormatNbChannels(const audio_sample_format_t *fmt)
{
    return popcount(fmt->i_physical_channels);
}

VLC_API unsigned int aout_BitsPerSample( vlc_fourcc_t i_format ) VLC_USED;
VLC_API void aout_FormatPrepare( audio_sample_format_t * p_format );
VLC_API void aout_FormatPrint(vlc_object_t *, const char *,
                              const audio_sample_format_t *);
#define aout_FormatPrint(o, t, f) aout_FormatPrint(VLC_OBJECT(o), t, f)
VLC_API const char * aout_FormatPrintChannels( const audio_sample_format_t * ) VLC_USED;

VLC_API float aout_VolumeGet (audio_output_t *);
VLC_API int aout_VolumeSet (audio_output_t *, float);
VLC_API int aout_MuteGet (audio_output_t *);
VLC_API int aout_MuteSet (audio_output_t *, bool);

/**
 * Report change of configured audio volume to the core and UI.
 */
static inline void aout_VolumeReport(audio_output_t *aout, float volume)
{
    aout->event.volume_report(aout, volume);
}

/**
 * Report change of muted flag to the core and UI.
 */
static inline void aout_MuteReport(audio_output_t *aout, bool mute)
{
    aout->event.mute_report(aout, mute);
}

/**
 * Report audio policy status.
 * \parm cork true to request a cork, false to undo any pending cork.
 */
static inline void aout_PolicyReport(audio_output_t *aout, bool cork)
{
    aout->event.policy_report(aout, cork);
}

/**
 * Request a change of software audio amplification.
 * \param gain linear amplitude gain (must be positive)
 * \warning Values in excess 1.0 may cause overflow and distorsion.
 */
static inline int aout_GainRequest(audio_output_t *aout, float gain)
{
    return aout->event.gain_request(aout, gain);
}

VLC_API int aout_ChannelsRestart( vlc_object_t *, const char *, vlc_value_t, vlc_value_t, void * );

/* */
VLC_API vout_thread_t * aout_filter_RequestVout( filter_t *, vout_thread_t *p_vout, video_format_t *p_fmt ) VLC_USED;

/** Audio output buffer FIFO */
struct aout_fifo_t
{
    block_t  *p_first;
    block_t **pp_last;
    date_t    end_date;
};

/* Legacy packet-oriented audio output helpers */
typedef struct
{
    vlc_mutex_t lock;
    audio_sample_format_t format;
    aout_fifo_t partial; /**< Audio blocks before packetization */
    aout_fifo_t fifo; /**< Packetized audio blocks */
    mtime_t pause_date; /**< Date when paused or VLC_TS_INVALID */
    mtime_t time_report; /**< Desynchronization estimate or VLC_TS_INVALID */
    unsigned samples; /**< Samples per packet */
    bool starving; /**< Whether currently starving (to limit error messages) */
} aout_packet_t;

VLC_DEPRECATED void aout_PacketInit(audio_output_t *, aout_packet_t *, unsigned, const audio_sample_format_t *);
VLC_DEPRECATED void aout_PacketDestroy(audio_output_t *);

VLC_DEPRECATED void aout_PacketPlay(audio_output_t *, block_t *, mtime_t *);
VLC_DEPRECATED void aout_PacketPause(audio_output_t *, bool, mtime_t);
VLC_DEPRECATED void aout_PacketFlush(audio_output_t *, bool);

VLC_DEPRECATED block_t *aout_PacketNext(audio_output_t *, mtime_t) VLC_USED;


#endif /* VLC_AOUT_H */
