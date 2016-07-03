/*****************************************************************************
 * ts_streams_private.h: Transport Stream input module for VLC.
 *****************************************************************************
 * Copyright (C) 2004-2016 VLC authors and VideoLAN
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
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *****************************************************************************/
#ifndef VLC_TS_STREAMS_PRIVATE_H
#define VLC_TS_STREAMS_PRIVATE_H

typedef struct dvbpsi_s dvbpsi_t;
typedef struct ts_sections_processor_t ts_sections_processor_t;

#include "mpeg4_iod.h"

#include <vlc_common.h>
#include <vlc_es.h>

#ifdef HAVE_ARIB
# include "../arib/multi2.h"
#endif

struct ts_pat_t
{
    int             i_version;
    int             i_ts_id;
    dvbpsi_t       *handle;
    DECL_ARRAY(ts_pid_t *) programs;

};

struct ts_pmt_t
{
    dvbpsi_t       *handle;
    int             i_version;
    int             i_number;
    int             i_pid_pcr;
    bool            b_selected;
    /* IOD stuff (mpeg4) */
    od_descriptor_t *iod;
    od_descriptors_t od;

    DECL_ARRAY(ts_pid_t *) e_streams;

    /* Used for ref tracking PSIP pid chain */
    ts_pid_t        *p_atsc_si_basepid;
    /* Used for ref tracking SI pid chain, starting with SDT */
    ts_pid_t        *p_si_sdt_pid;

    struct
    {
        mtime_t i_current;
        mtime_t i_first; // seen <> != -1
        /* broken PCR handling */
        mtime_t i_first_dts;
        mtime_t i_pcroffset;
        bool    b_disable; /* ignore PCR field, use dts */
        bool    b_fix_done;
    } pcr;

    struct
    {
        time_t i_event_start;
        time_t i_event_length;
    } eit;

    mtime_t i_last_dts;

#ifdef HAVE_ARIB
    ts_pid_t        *p_ecm;
#endif
};

struct ts_pes_es_t
{
    ts_pmt_t *p_program;
    es_format_t  fmt;
    es_out_id_t *id;
    uint16_t i_sl_es_id;
    ts_pes_es_t *p_extraes; /* Some private streams encapsulate several ES (eg. DVB subtitles) */
    ts_pes_es_t *p_next; /* Next es on same pid from different pmt (shared pid) */
    /* J2K stuff */
    uint8_t  b_interlaced;
};

typedef enum
{
    TS_TRANSPORT_PES,
    TS_TRANSPORT_SECTIONS,
    TS_TRANSPORT_IGNORE
} ts_transport_type_t;

struct ts_pes_t
{
    ts_pes_es_t *p_es;

    uint8_t i_stream_type;

    ts_transport_type_t transport;
    int         i_data_size;
    int         i_data_gathered;
    block_t     *p_data;
    block_t     **pp_last;
    bool        b_always_receive;
    ts_sections_processor_t *p_sections_proc;

    block_t *   p_prepcr_outqueue;

    /* SL AU */
    struct
    {
        block_t     *p_data;
        block_t     **pp_last;
    } sl;
};

typedef struct ts_si_context_t ts_si_context_t;

struct ts_si_t
{
    dvbpsi_t *handle;
    int       i_version;
    /* Track successfully set pid */
    ts_pid_t *eitpid;
    ts_pid_t *tdtpid;
};

typedef struct ts_psip_context_t ts_psip_context_t;

struct ts_psip_t
{
    dvbpsi_t       *handle;
    int             i_version;
    ts_pes_es_t    *p_eas_es;
    ts_psip_context_t *p_ctx;
    /* Used to track list of active pid for eit/ett, to call PIDRelease on them.
       VCT table could have been used, but PIDSetup can fail, and we can't alter
       the VCT table accordingly without going ahead of more troubles */
    DECL_ARRAY(ts_pid_t *) eit;

};

#ifdef HAVE_ARIB
struct ts_cat_t
{
    dvbpsi_t       *handle;
    int             i_version;
    ts_pid_t       *p_emm;
};

struct ts_emm_t
{
    dvbpsi_t       *handle;
    int             i_version;
};

struct ts_ecm_t
{
    dvbpsi_t       *handle;
    int             i_version;
    MULTI2         *arib_descrambler;
};
#endif

#endif
