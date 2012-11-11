/*****************************************************************************
 * misc.h: code not specific to vlc
 *****************************************************************************
 * Copyright (C) 2003-2012 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Jon Lech Johansen <jon-vl@nanocrew.net>
 *          Felix Paul Kühne <fkuehne at videolan dot org>
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
#import "CompatibilityFixes.h"

/*****************************************************************************
 * NSSound (VLCAdditions)
 *
 * added code to change the system volume, needed for the apple remote code
 * this is simplified code, which won't let you set the exact volume
 * (that's what the audio output is for after all), but just the system volume
 * in steps of 1/16 (matching the default AR or volume key implementation).
 *****************************************************************************/

@interface NSSound (VLCAdditions)
+ (float)systemVolumeForChannel:(int)channel;
+ (bool)setSystemVolume:(float)volume forChannel:(int)channel;
+ (void)increaseSystemVolume;
+ (void)decreaseSystemVolume;
@end

/*****************************************************************************
 * NSAnimation (VLCAddition)
 *****************************************************************************/

@interface NSAnimation (VLCAdditions)
@property (readwrite) void * userInfo;

@end

/*****************************************************************************
 * NSScreen (VLCAdditions)
 *
 *  Missing extension to NSScreen
 *****************************************************************************/

@interface NSScreen (VLCAdditions)

@property (readonly) BOOL mainScreen;

+ (NSScreen *)screenWithDisplayID: (CGDirectDisplayID)displayID;
- (BOOL)isScreen: (NSScreen*)screen;
- (CGDirectDisplayID)displayID;
- (void)blackoutOtherScreens;
+ (void)unblackoutScreens;
@end


/*****************************************************************************
 * VLBrushedMetalImageView
 *****************************************************************************/

@interface VLBrushedMetalImageView : NSImageView

@end


/*****************************************************************************
 * MPSlider
 *****************************************************************************/

@interface MPSlider : NSSlider

@end

/*****************************************************************************
 * ProgressView
 *****************************************************************************/

@interface VLCProgressView : NSView

- (void)scrollWheel:(NSEvent *)o_event;

@end


/*****************************************************************************
 * TimeLineSlider
 *****************************************************************************/

@interface TimeLineSlider : NSSlider
{
    NSImage *o_knob_img;
    NSRect img_rect;
    BOOL b_dark;
}
@property (readonly) CGFloat knobPosition;

- (void)drawRect:(NSRect)rect;
- (void)drawKnobInRect:(NSRect)knobRect;

@end

/*****************************************************************************
 * VLCVolumeSliderCommon
 *****************************************************************************/

@interface VLCVolumeSliderCommon : NSSlider

- (void)scrollWheel:(NSEvent *)o_event;

@end

/*****************************************************************************
 * ITSlider
 *****************************************************************************/

@interface ITSlider : VLCVolumeSliderCommon
{
    NSImage *img;
    NSRect image_rect;
}

- (void)drawRect:(NSRect)rect;
- (void)drawKnobInRect:(NSRect)knobRect;

@end

/*****************************************************************************
 * VLCTimeField interface
 *****************************************************************************
 * we need the implementation to catch our click-event in the controller window
 *****************************************************************************/

@interface VLCTimeField : NSTextField
{
    NSShadow * o_string_shadow;
    NSDictionary * o_string_attributes_dict;
    NSTextAlignment textAlignment;
}
@property (readonly) BOOL timeRemaining;
@end

/*****************************************************************************
 * VLCMainWindowSplitView interface
 *****************************************************************************/
@interface VLCMainWindowSplitView : NSSplitView

@end

/*****************************************************************************
 * VLCThreePartImageView interface
 *****************************************************************************/
@interface VLCThreePartImageView : NSView
{
    NSImage * o_left_img;
    NSImage * o_middle_img;
    NSImage * o_right_img;
}

- (void)setImagesLeft:(NSImage *)left middle: (NSImage *)middle right:(NSImage *)right;
@end

/*****************************************************************************
 * VLCThreePartDropView interface
 *****************************************************************************/
@interface VLCThreePartDropView : VLCThreePartImageView

@end
