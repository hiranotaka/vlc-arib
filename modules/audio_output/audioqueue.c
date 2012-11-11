/*****************************************************************************
 * audioqueue.c : AudioQueue audio output plugin for vlc
 *****************************************************************************
 * Copyright (C) 2010 VideoLAN and AUTHORS
 *
 * Authors: Romain Goyet <romain.goyet@likid.org>
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
#include <unistd.h>                                      /* write(), close() */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_aout.h>

#include <AudioToolBox/AudioToolBox.h>

#define NUMBER_OF_BUFFERS 3
#define FRAME_SIZE 2048

/*****************************************************************************
 * aout_sys_t: AudioQueue audio output method descriptor
 *****************************************************************************
 * This structure is part of the audio output thread descriptor.
 * It describes the specific properties of an audio device.
 *****************************************************************************/
struct aout_sys_t
{
    aout_packet_t packet;
    AudioQueueRef audioQueue;
    float soft_gain;
    bool soft_mute;
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Open               ( vlc_object_t * );
static void Close              ( vlc_object_t * );
static void Play               ( audio_output_t *, block_t *, mtime_t * );
static void AudioQueueCallback (void *, AudioQueueRef, AudioQueueBufferRef);

#include "volume.h"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin ()
    set_shortname( "AudioQueue" )
    set_description( N_("AudioQueue (iOS / Mac OS) audio output") )
    set_capability( "audio output", 40 )
    set_category( CAT_AUDIO )
    set_subcategory( SUBCAT_AUDIO_AOUT )
    add_shortcut( "audioqueue" )
    set_callbacks( Open, Close )
vlc_module_end ()

/*****************************************************************************
 * Open: open the audio device
 *****************************************************************************/

static int Start( audio_output_t *aout, audio_sample_format_t *restrict fmt )
{
    audio_output_t *p_aout = (audio_output_t *)p_this;
    aout_sys_t *p_sys = aout->sys;

    OSStatus status = 0;

    // Setup the audio device.
    AudioStreamBasicDescription deviceFormat;
    deviceFormat.mSampleRate = 44100;
    deviceFormat.mFormatID = kAudioFormatLinearPCM;
    deviceFormat.mFormatFlags = kLinearPCMFormatFlagIsSignedInteger; // Signed integer, little endian
    deviceFormat.mBytesPerPacket = 4;
    deviceFormat.mFramesPerPacket = 1;
    deviceFormat.mBytesPerFrame = 4;
    deviceFormat.mChannelsPerFrame = 2;
    deviceFormat.mBitsPerChannel = 16;
    deviceFormat.mReserved = 0;

    // Create a new output AudioQueue for the device.
    status = AudioQueueNewOutput(&deviceFormat,         // Format
                                 AudioQueueCallback,    // Callback
                                 p_aout,                // User data, passed to the callback
                                 CFRunLoopGetMain(),    // RunLoop
                                 kCFRunLoopDefaultMode, // RunLoop mode
                                 0,                     // Flags ; must be zero (per documentation)...
                                 &(p_sys->audioQueue)); // Output

    // This will be used for boosting the audio without the need of a mixer (floating-point conversion is expensive on ARM)
    // AudioQueueSetParameter(p_sys->audioQueue, kAudioQueueParam_Volume, 12.0); // Defaults to 1.0

    msg_Dbg(p_aout, "New AudioQueue output created (status = %i)", status);

    // Allocate buffers for the AudioQueue, and pre-fill them.
    for (int i = 0; i < NUMBER_OF_BUFFERS; ++i) {
        AudioQueueBufferRef buffer = NULL;
        status = AudioQueueAllocateBuffer(p_sys->audioQueue, FRAME_SIZE * 4, &buffer);
        AudioQueueCallback(NULL, p_sys->audioQueue, buffer);
    }

    fmt->i_format = VLC_CODEC_S16L;
    fmt->i_physical_channels = AOUT_CHANS_STEREO;
    fmt->i_rate = 44100;
    aout_PacketInit(p_aout, &p_sys->packet, FRAME_SIZE, fmt);
    p_aout->play = aout_PacketPlay;
    p_aout->pause = aout_PacketPause;
    p_aout->flush = aout_PacketFlush;
    aout_SoftVolumeStart(p_aout);

    msg_Dbg(p_aout, "Starting AudioQueue (status = %i)", status);
    status = AudioQueueStart(p_sys->audioQueue, NULL);

    return VLC_SUCCESS;
}

/*****************************************************************************
  * aout_FifoPop : get the next buffer out of the FIFO
  *****************************************************************************/
static block_t *aout_FifoPop2( aout_fifo_t * p_fifo )
 {
     block_t *p_buffer = p_fifo->p_first;
     if( p_buffer != NULL )
     {
         p_fifo->p_first = p_buffer->p_next;
         if( p_fifo->p_first == NULL )
             p_fifo->pp_last = &p_fifo->p_first;
     }
     return p_buffer;
 }


/*****************************************************************************
 * Close: close the audio device
 *****************************************************************************/
static void Stop ( audio_output_t *p_aout )
{
    struct aout_sys_t * p_sys = p_aout->sys;

    msg_Dbg(p_aout, "Stopping AudioQueue");
    AudioQueueStop(p_sys->audioQueue, false);
    msg_Dbg(p_aout, "Disposing of AudioQueue");
    AudioQueueDispose(p_sys->audioQueue, false);
    aout_PacketDestroy(p_aout);
}

void AudioQueueCallback(void * inUserData, AudioQueueRef inAQ, AudioQueueBufferRef inBuffer) {
    audio_output_t * p_aout = (audio_output_t *)inUserData;
    block_t *   p_buffer = NULL;

    if (p_aout) {
        struct aout_sys_t * p_sys = p_aout->sys;
        aout_packet_t * packet = &p_sys->packet;

        if (packet)
        {
            vlc_mutex_lock( &packet->lock );
            p_buffer = aout_FifoPop2( &packet->fifo );
            vlc_mutex_unlock( &packet->lock );
        }
    }

    if ( p_buffer != NULL ) {
        memcpy( inBuffer->mAudioData, p_buffer->p_buffer, p_buffer->i_buffer );
        inBuffer->mAudioDataByteSize = p_buffer->i_buffer;
        block_Release( p_buffer );
    } else {
        memset( inBuffer->mAudioData, 0, inBuffer->mAudioDataBytesCapacity );
        inBuffer->mAudioDataByteSize = inBuffer->mAudioDataBytesCapacity;
    }
    AudioQueueEnqueueBuffer(inAQ, inBuffer, 0, NULL);
}

static int Open(vlc_object_t *obj)
{
    audio_output_t *aout = (audio_output_t *)obj;
    aout_sys_t *sys = malloc(sizeof (*sys));

    if (unlikely(sys == NULL))
        return VLC_ENOMEM;
    aout->sys = sys;
    aout->start = Start;
    aout->stop = Stop;
    aout_SoftVolumeInit(aout);
    return VLC_SUCCESS;
}

static void Close(vlc_object_t *obj)
{
    audio_output_t *aout = (audio_output_t *)obj;
    aout_sys_t *sys = aout->sys;

    free(sys);
}
