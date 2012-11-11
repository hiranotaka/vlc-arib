/*****************************************************************************
 * util.cpp : matroska demuxer
 *****************************************************************************
 * Copyright (C) 2003-2004 the VideoLAN team
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Steve Lhomme <steve.lhomme@free.fr>
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
#include "util.hpp"
#include "demux.hpp"

#include <stdint.h>
/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

#ifdef HAVE_ZLIB_H
block_t *block_zlib_decompress( vlc_object_t *p_this, block_t *p_in_block ) {
    int result, dstsize, n;
    unsigned char *dst;
    block_t *p_block;
    z_stream d_stream;

    d_stream.zalloc = (alloc_func)0;
    d_stream.zfree = (free_func)0;
    d_stream.opaque = (voidpf)0;
    result = inflateInit(&d_stream);
    if( result != Z_OK )
    {
        msg_Dbg( p_this, "inflateInit() failed. Result: %d", result );
        return NULL;
    }

    d_stream.next_in = (Bytef *)p_in_block->p_buffer;
    d_stream.avail_in = p_in_block->i_buffer;
    n = 0;
    p_block = block_New( p_this, 0 );
    dst = NULL;
    do
    {
        n++;
        p_block = block_Realloc( p_block, 0, n * 1000 );
        dst = (unsigned char *)p_block->p_buffer;
        d_stream.next_out = (Bytef *)&dst[(n - 1) * 1000];
        d_stream.avail_out = 1000;
        result = inflate(&d_stream, Z_NO_FLUSH);
        if( ( result != Z_OK ) && ( result != Z_STREAM_END ) )
        {
            msg_Dbg( p_this, "Zlib decompression failed. Result: %d", result );
            return NULL;
        }
    }
    while( ( d_stream.avail_out == 0 ) && ( d_stream.avail_in != 0 ) &&
           ( result != Z_STREAM_END ) );

    dstsize = d_stream.total_out;
    inflateEnd( &d_stream );

    p_block = block_Realloc( p_block, 0, dstsize );
    p_block->i_buffer = dstsize;
    block_Release( p_in_block );

    return p_block;
}
#endif

/* Utility function for BlockDecode */
block_t *MemToBlock( uint8_t *p_mem, size_t i_mem, size_t offset)
{
    if( unlikely( i_mem > SIZE_MAX - offset ) )
        return NULL;

    block_t *p_block = block_New( p_demux, i_mem + offset );
    if( likely(p_block != NULL) )
    {
        memcpy( p_block->p_buffer + offset, p_mem, i_mem );
    }
    return p_block;
}


void handle_real_audio(demux_t * p_demux, mkv_track_t * p_tk, block_t * p_blk, mtime_t i_pts)
{
    uint8_t * p_frame = p_blk->p_buffer;
    Cook_PrivateTrackData * p_sys = (Cook_PrivateTrackData *) p_tk->p_sys;
    size_t size = p_blk->i_buffer;

    if( p_tk->i_last_dts == VLC_TS_INVALID )
    {
        for( size_t i = 0; i < p_sys->i_subpackets; i++)
            if( p_sys->p_subpackets[i] )
            {
                block_Release(p_sys->p_subpackets[i]);
                p_sys->p_subpackets[i] = NULL;
            }
        p_sys->i_subpacket = 0;
    }

    if( p_tk->fmt.i_codec == VLC_CODEC_COOK ||
        p_tk->fmt.i_codec == VLC_CODEC_ATRAC3 )
    {
        const uint32_t i_num = p_sys->i_frame_size / p_sys->i_subpacket_size;
        const int y = p_sys->i_subpacket / ( p_sys->i_frame_size / p_sys->i_subpacket_size );

        for( int i = 0; i < i_num; i++ )
        {
            int i_index = p_sys->i_sub_packet_h * i +
                          ((p_sys->i_sub_packet_h + 1) / 2) * (y&1) + (y>>1);
            if( i_index >= p_sys->i_subpackets )
                return;

            block_t *p_block = block_New( p_demux, p_sys->i_subpacket_size );
            if( !p_block )
                return;

            if( size < p_sys->i_subpacket_size )
                return;

            memcpy( p_block->p_buffer, p_frame, p_sys->i_subpacket_size );
            p_block->i_dts = VLC_TS_INVALID;
            p_block->i_pts = VLC_TS_INVALID;
            if( !p_sys->i_subpacket )
            {
                p_tk->i_last_dts = 
                p_block->i_pts = i_pts + VLC_TS_0;
            }

            p_frame += p_sys->i_subpacket_size;
            size -=  p_sys->i_subpacket_size;

            p_sys->i_subpacket++;
            p_sys->p_subpackets[i_index] = p_block;
        }
    }
    else
    {
        /*TODO*/
    }
    if( p_sys->i_subpacket == p_sys->i_subpackets )
    {
        for( size_t i = 0; i < p_sys->i_subpackets; i++)
        {
            es_out_Send( p_demux->out, p_tk->p_es,  p_sys->p_subpackets[i]);
            p_sys->p_subpackets[i] = NULL;
        }
        p_sys->i_subpacket = 0;
    }
}

int32_t Cook_PrivateTrackData::Init()
{
    i_subpackets = (size_t) i_sub_packet_h * (size_t) i_frame_size / (size_t) i_subpacket_size;
    p_subpackets = (block_t**) calloc(i_subpackets, sizeof(block_t*));

    if( unlikely( !p_subpackets ) )
    {
        i_subpackets = 0;
        return 1;
    }

    return 0;
}

Cook_PrivateTrackData::~Cook_PrivateTrackData()
{
    for( size_t i = 0; i < i_subpackets; i++ )
        if( p_subpackets[i] )
            block_Release( p_subpackets[i] );

    free( p_subpackets );    
}
