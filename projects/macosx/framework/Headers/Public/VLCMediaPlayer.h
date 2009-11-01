/*****************************************************************************
 * VLCMediaPlayer.h: VLCKit.framework VLCMediaPlayer header
 *****************************************************************************
 * Copyright (C) 2007 Pierre d'Herbemont
 * Copyright (C) 2007 the VideoLAN team
 * $Id$
 *
 * Authors: Pierre d'Herbemont <pdherbemont # videolan.org>
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

#import <Cocoa/Cocoa.h>
#import "VLCMedia.h"
#import "VLCVideoView.h"
#import "VLCVideoLayer.h"
#import "VLCTime.h"
#import "VLCAudio.h"

/* Notification Messages */
extern NSString * VLCMediaPlayerTimeChanged;
extern NSString * VLCMediaPlayerStateChanged;

/**
 * VLCMediaPlayerState describes the state of the media player.
 */
typedef enum VLCMediaPlayerState
{
    VLCMediaPlayerStateStopped,        //< Player has stopped
    VLCMediaPlayerStateOpening,        //< Stream is opening
    VLCMediaPlayerStateBuffering,      //< Stream is buffering
    VLCMediaPlayerStateEnded,          //< Stream has ended
    VLCMediaPlayerStateError,          //< Player has generated an error
    VLCMediaPlayerStatePlaying,        //< Stream is playing
    VLCMediaPlayerStatePaused          //< Stream is paused
} VLCMediaPlayerState;

/**
 * Returns the name of the player state as a string.
 * \param state The player state.
 * \return A string containing the name of state. If state is not a valid state, returns nil.
 */
extern NSString * VLCMediaPlayerStateToString(VLCMediaPlayerState state);

/**
 * Formal protocol declaration for playback delegates.  Allows playback messages
 * to be trapped by delegated objects.
 */
@protocol VLCMediaPlayerDelegate
/**
 * Sent by the default notification center whenever the player's time has changed.
 * \details Discussion The value of aNotification is always an VLCMediaPlayerTimeChanged notification. You can retrieve 
 * the VLCMediaPlayer object in question by sending object to aNotification.
 */
- (void)mediaPlayerTimeChanged:(NSNotification *)aNotification;

/**
 * Sent by the default notification center whenever the player's state has changed.
 * \details Discussion The value of aNotification is always an VLCMediaPlayerStateChanged notification. You can retrieve 
 * the VLCMediaPlayer object in question by sending object to aNotification.
 */
- (void)mediaPlayerStateChanged:(NSNotification *)aNotification;
@end

// TODO: Should we use medialist_player or our own flavor of media player?
@interface VLCMediaPlayer : NSObject 
{
    id delegate;                        //< Object delegate
    void * instance;                    //  Internal
    VLCMedia * media;                   //< Current media being played
    VLCTime * cachedTime;               //< Cached time of the media being played
    VLCMediaPlayerState cachedState;    //< Cached state of the media being played
    float position;                     //< The position of the media being played
}

/* Initializers */
- (id)initWithVideoView:(VLCVideoView *)aVideoView;
- (id)initWithVideoLayer:(VLCVideoLayer *)aVideoLayer;

/* Properties */
- (void)setDelegate:(id)value;
- (id)delegate;

/* Video View Options */
// TODO: Should be it's own object?

- (void)setVideoView:(VLCVideoView *)aVideoView;
- (void)setVideoLayer:(VLCVideoLayer *)aVideoLayer;

@property (retain) id drawable; /* The videoView or videoLayer */

- (void)setVideoAspectRatio:(char *)value;
- (char *)videoAspectRatio;
- (void)setVideoSubTitles:(int)value;
- (int)videoSubTitles;

- (void)setVideoCropGeometry:(char *)value;
- (char *)videoCropGeometry;

- (void)setVideoTeleText:(int)value;
- (int)videoTeleText;

/**
 * Take a snapshot of the current video.
 *
 * If width AND height is 0, original size is used.
 * If width OR height is 0, original aspect-ratio is preserved.
 *
 * \param path the path where to save the screenshot to
 * \param width the snapshot's width
 * \param height the snapshot's height
 */
- (void)saveVideoSnapshotAt: (NSString *)path withWidth:(NSUInteger)width andHeight:(NSUInteger)height;

/**
 * Enable or disable deinterlace filter
 *
 * \param name of deinterlace filter to use (availability depends on underlying VLC version)
 * \param enable boolean to enable or disable deinterlace filter
 */
- (void)setDeinterlaceFilter: (NSString *)name enabled: (BOOL)enabled;

@property float rate;

@property (readonly) VLCAudio * audio;

/* Video Information */
- (NSSize)videoSize;
- (BOOL)hasVideoOut;
- (float)framesPerSecond;

/**
 * Sets the current position (or time) of the feed.
 * \param value New time to set the current position to.  If time is [VLCTime nullTime], 0 is assumed.
 */
- (void)setTime:(VLCTime *)value;

/** 
 * Returns the current position (or time) of the feed.
 * \return VLCTIme object with current time.
 */
- (VLCTime *)time;

- (void)setChapter:(int)value;
- (int)chapter;
- (int)countOfChapters;

/* Audio Options */
- (void)setAudioTrack:(int)value;
- (int)audioTrack;
- (int)countOfAudioTracks;

- (void)setAudioChannel:(int)value;
- (int)audioChannel;

/* Media Options */
- (void)setMedia:(VLCMedia *)value;
- (VLCMedia *)media;

/* Playback Operations */
/**
 * Plays a media resource using the currently selected media controller (or 
 * default controller.  If feed was paused then the feed resumes at the position 
 * it was paused in.
 * \return A Boolean determining whether the stream was played or not.
 */
- (BOOL)play;

/**
 * Toggle's the pause state of the feed.
 */
- (void)pause;

/**
 * Stop the playing.
 */
- (void)stop;

/**
 * Fast forwards through the feed at the standard 1x rate.
 */
- (void)fastForward;

/**
 * Fast forwards through the feed at the rate specified.
 * \param rate Rate at which the feed should be fast forwarded.
 */
- (void)fastForwardAtRate:(float)rate;

/**
 * Rewinds through the feed at the standard 1x rate.
 */
- (void)rewind;

/**
 * Rewinds through the feed at the rate specified.
 * \param rate Rate at which the feed should be fast rewound.
 */
- (void)rewindAtRate:(float)rate;

/**
 * Jumps shortly backward in current stream if seeking is supported.
 * \param interval to skip, in sec.
 */
- (void)jumpBackward:(NSInteger)interval;

/**
 * Jumps shortly forward in current stream if seeking is supported.
 * \param interval to skip, in sec.
 */
- (void)jumpForward:(NSInteger)interval;

/**
 * Jumps shortly backward in current stream if seeking is supported.
 */
- (void)extraShortJumpBackward;

/**
 * Jumps shortly forward in current stream if seeking is supported.
 */
- (void)extraShortJumpForward;

/**
 * Jumps shortly backward in current stream if seeking is supported.
 */
- (void)shortJumpBackward;

/**
 * Jumps shortly forward in current stream if seeking is supported.
 */
- (void)shortJumpForward;

/**
 * Jumps shortly backward in current stream if seeking is supported.
 */
- (void)mediumJumpBackward;

/**
 * Jumps shortly forward in current stream if seeking is supported.
 */
- (void)mediumJumpForward;

/**
 * Jumps shortly backward in current stream if seeking is supported.
 */
- (void)longJumpBackward;

/**
 * Jumps shortly forward in current stream if seeking is supported.
 */
- (void)longJumpForward;

/* Playback Information */
/**
 * Playback state flag identifying that the stream is currently playing.
 * \return TRUE if the feed is playing, FALSE if otherwise.
 */
- (BOOL)isPlaying;

/**
 * Playback state flag identifying wheather the stream will play.
 * \return TRUE if the feed is ready for playback, FALSE if otherwise.
 */
- (BOOL)willPlay;

/**
 * Playback's current state.
 * \see VLCMediaState
 */
- (VLCMediaPlayerState)state;

/**
 * Returns the receiver's position in the reading. 
 * \return A number between 0 and 1. indicating the position
 */
- (float)position;
- (void)setPosition:(float)newPosition;

- (BOOL)isSeekable;

- (BOOL)canPause;

@end
