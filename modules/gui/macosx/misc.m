/*****************************************************************************
 * misc.m: code not specific to vlc
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

#import "misc.h"
#import "intf.h"                                          /* VLCApplication */
#import "MainWindow.h"
#import "ControlsBar.h"
#import "controls.h"
#import "CoreInteraction.h"
#import <CoreAudio/CoreAudio.h>
#import <vlc_keys.h>


/*****************************************************************************
 * NSSound (VLCAdditions)
 *
 * added code to change the system volume, needed for the apple remote code
 * this is simplified code, which won't let you set the exact volume
 * (that's what the audio output is for after all), but just the system volume
 * in steps of 1/16 (matching the default AR or volume key implementation).
 *****************************************************************************/

@implementation NSSound (VLCAdditions)

+ (float)systemVolumeForChannel:(int)channel
{
    AudioDeviceID i_device;
    float f_volume;
    OSStatus err;
    UInt32 i_size;

    i_size = sizeof( i_device );
    AudioObjectPropertyAddress deviceAddress = { kAudioHardwarePropertyDefaultOutputDevice, kAudioDevicePropertyScopeOutput, kAudioObjectPropertyElementMaster };
    err = AudioObjectGetPropertyData( kAudioObjectSystemObject, &deviceAddress, 0, NULL, &i_size, &i_device );
    if (err != noErr) {
        msg_Warn( VLCIntf, "couldn't get main audio output device" );
        return .0;
    }

    AudioObjectPropertyAddress propertyAddress = { kAudioDevicePropertyVolumeScalar, kAudioDevicePropertyScopeOutput, channel };
    i_size = sizeof( f_volume );
    err = AudioObjectGetPropertyData(i_device, &propertyAddress, 0, NULL, &i_size, &f_volume);
    if (err != noErr) {
        msg_Warn( VLCIntf, "couldn't get volume value" );
        return .0;
    }

    return f_volume;
}

+ (bool)setSystemVolume:(float)f_volume forChannel:(int)i_channel
{
    /* the following code will fail on S/PDIF devices. there is an easy work-around, but we'd like to match the OS behavior */

    AudioDeviceID i_device;
    OSStatus err;
    UInt32 i_size;
    Boolean b_writeable;

    i_size = sizeof( i_device );
    AudioObjectPropertyAddress deviceAddress = { kAudioHardwarePropertyDefaultOutputDevice, kAudioDevicePropertyScopeOutput, kAudioObjectPropertyElementMaster };
    err = AudioObjectGetPropertyData( kAudioObjectSystemObject, &deviceAddress, 0, NULL, &i_size, &i_device );
    if (err != noErr) {
        msg_Warn( VLCIntf, "couldn't get main audio output device" );
        return NO;
    }

    AudioObjectPropertyAddress propertyAddress = { kAudioDevicePropertyVolumeScalar, kAudioDevicePropertyScopeOutput, i_channel };
    i_size = sizeof( f_volume );
    err = AudioObjectIsPropertySettable( i_device, &propertyAddress, &b_writeable );
    if (err != noErr || !b_writeable ) {
        msg_Warn( VLCIntf, "we can't set the main audio devices' volume" );
        return NO;
    }
    err = AudioObjectSetPropertyData(i_device, &propertyAddress, 0, NULL, i_size, &f_volume);

    return YES;
}

+ (void)increaseSystemVolume
{
    float f_volume = [NSSound systemVolumeForChannel:1]; // we trust that mono is always available and that all channels got the same volume
    f_volume += .0625; // 1/16 to match the OS
    bool b_returned = YES;

    /* since core audio doesn't provide a reasonable way to see how many channels we got, let's see how long we can do this */
    for (NSUInteger x = 1; b_returned ; x++)
        b_returned = [NSSound setSystemVolume: f_volume forChannel:x];
}

+ (void)decreaseSystemVolume
{
    float f_volume = [NSSound systemVolumeForChannel:1]; // we trust that mono is always available and that all channels got the same volume
    f_volume -= .0625; // 1/16 to match the OS
    bool b_returned = YES;

    /* since core audio doesn't provide a reasonable way to see how many channels we got, let's see how long we can do this */
    for (NSUInteger x = 1; b_returned ; x++)
        b_returned = [NSSound setSystemVolume: f_volume forChannel:x];
}

@end

/*****************************************************************************
 * NSAnimation (VLCAdditions)
 *
 *  Missing extension to NSAnimation
 *****************************************************************************/

@implementation NSAnimation (VLCAdditions)
/* fake class attributes  */
static NSMapTable *VLCAdditions_userInfo = NULL;

+ (void)load
{
    /* init our fake object attribute */
    VLCAdditions_userInfo = NSCreateMapTable(NSNonRetainedObjectMapKeyCallBacks, NSObjectMapValueCallBacks, 16);
}

- (void)dealloc
{
    NSMapRemove(VLCAdditions_userInfo, self);
    [super dealloc];
}

- (void)setUserInfo: (void *)userInfo
{
    NSMapInsert(VLCAdditions_userInfo, self, (void*)userInfo);
}

- (void *)userInfo
{
    return NSMapGet(VLCAdditions_userInfo, self);
}
@end

/*****************************************************************************
 * NSScreen (VLCAdditions)
 *
 *  Missing extension to NSScreen
 *****************************************************************************/

@implementation NSScreen (VLCAdditions)

static NSMutableArray *blackoutWindows = NULL;

+ (void)load
{
    /* init our fake object attribute */
    blackoutWindows = [[NSMutableArray alloc] initWithCapacity:1];
}

+ (NSScreen *)screenWithDisplayID: (CGDirectDisplayID)displayID
{
    NSUInteger count = [[NSScreen screens] count];

    for ( NSUInteger i = 0; i < count; i++ ) {
        NSScreen *screen = [[NSScreen screens] objectAtIndex: i];
        if ([screen displayID] == displayID)
            return screen;
    }
    return nil;
}

- (BOOL)mainScreen
{
    return ([self displayID] == [[[NSScreen screens] objectAtIndex:0] displayID]);
}

- (BOOL)isScreen: (NSScreen*)screen
{
    return ([self displayID] == [screen displayID]);
}

- (CGDirectDisplayID)displayID
{
    return (CGDirectDisplayID)[[[self deviceDescription] objectForKey: @"NSScreenNumber"] intValue];
}

- (void)blackoutOtherScreens
{
    /* Free our previous blackout window (follow blackoutWindow alloc strategy) */
    [blackoutWindows makeObjectsPerformSelector:@selector(close)];
    [blackoutWindows removeAllObjects];

    NSUInteger screenCount = [[NSScreen screens] count];
    for (NSUInteger i = 0; i < screenCount; i++) {
        NSScreen *screen = [[NSScreen screens] objectAtIndex: i];
        VLCWindow *blackoutWindow;
        NSRect screen_rect;

        if ([self isScreen: screen])
            continue;

        screen_rect = [screen frame];
        screen_rect.origin.x = screen_rect.origin.y = 0;

        /* blackoutWindow alloc strategy
            - The NSMutableArray blackoutWindows has the blackoutWindow references
            - blackoutOtherDisplays is responsible for alloc/releasing its Windows
        */
        blackoutWindow = [[VLCWindow alloc] initWithContentRect: screen_rect styleMask: NSBorderlessWindowMask
                backing: NSBackingStoreBuffered defer: NO screen: screen];
        [blackoutWindow setBackgroundColor:[NSColor blackColor]];
        [blackoutWindow setLevel: NSFloatingWindowLevel]; /* Disappear when Expose is triggered */

        [blackoutWindow displayIfNeeded];
        [blackoutWindow orderFront: self animate: YES];

        [blackoutWindows addObject: blackoutWindow];
        [blackoutWindow release];

        if ( [screen mainScreen] )
            [NSApp setPresentationOptions:(NSApplicationPresentationAutoHideDock | NSApplicationPresentationAutoHideMenuBar)];
    }
}

+ (void)unblackoutScreens
{
    NSUInteger blackoutWindowCount = [blackoutWindows count];

    for (NSUInteger i = 0; i < blackoutWindowCount; i++) {
        VLCWindow *blackoutWindow = [blackoutWindows objectAtIndex: i];
        [blackoutWindow closeAndAnimate: YES];
    }

    [NSApp setPresentationOptions:(NSApplicationPresentationDefault)];
}

@end

/*****************************************************************************
 * VLBrushedMetalImageView
 *****************************************************************************/

@implementation VLBrushedMetalImageView

- (BOOL)mouseDownCanMoveWindow
{
    return YES;
}

- (void)dealloc
{
    [self unregisterDraggedTypes];
    [super dealloc];
}

- (void)awakeFromNib
{
    [self registerForDraggedTypes:[NSArray arrayWithObject: NSFilenamesPboardType]];
    [self setImageScaling: NSScaleToFit];
    [self setImageFrameStyle: NSImageFrameNone];
    [self setImageAlignment: NSImageAlignCenter];
}

- (NSDragOperation)draggingEntered:(id <NSDraggingInfo>)sender
{
    if ((NSDragOperationGeneric & [sender draggingSourceOperationMask]) == NSDragOperationGeneric)
        return NSDragOperationGeneric;

    return NSDragOperationNone;
}

- (BOOL)prepareForDragOperation:(id <NSDraggingInfo>)sender
{
    return YES;
}

- (BOOL)performDragOperation:(id <NSDraggingInfo>)sender
{
    BOOL b_returned;
    b_returned = [[VLCCoreInteraction sharedInstance] performDragOperation: sender];

    [self setNeedsDisplay:YES];
    return b_returned;
}

- (void)concludeDragOperation:(id <NSDraggingInfo>)sender
{
    [self setNeedsDisplay:YES];
}

@end


/*****************************************************************************
 * MPSlider
 *****************************************************************************/
@implementation MPSlider

void _drawKnobInRect(NSRect knobRect)
{
    // Center knob in given rect
    knobRect.origin.x += (int)((float)(knobRect.size.width - 7)/2.0);
    knobRect.origin.y += (int)((float)(knobRect.size.height - 7)/2.0);

    // Draw diamond
    NSRectFillUsingOperation(NSMakeRect(knobRect.origin.x + 3, knobRect.origin.y + 6, 1, 1), NSCompositeSourceOver);
    NSRectFillUsingOperation(NSMakeRect(knobRect.origin.x + 2, knobRect.origin.y + 5, 3, 1), NSCompositeSourceOver);
    NSRectFillUsingOperation(NSMakeRect(knobRect.origin.x + 1, knobRect.origin.y + 4, 5, 1), NSCompositeSourceOver);
    NSRectFillUsingOperation(NSMakeRect(knobRect.origin.x + 0, knobRect.origin.y + 3, 7, 1), NSCompositeSourceOver);
    NSRectFillUsingOperation(NSMakeRect(knobRect.origin.x + 1, knobRect.origin.y + 2, 5, 1), NSCompositeSourceOver);
    NSRectFillUsingOperation(NSMakeRect(knobRect.origin.x + 2, knobRect.origin.y + 1, 3, 1), NSCompositeSourceOver);
    NSRectFillUsingOperation(NSMakeRect(knobRect.origin.x + 3, knobRect.origin.y + 0, 1, 1), NSCompositeSourceOver);
}

void _drawFrameInRect(NSRect frameRect)
{
    // Draw frame
    NSRectFillUsingOperation(NSMakeRect(frameRect.origin.x, frameRect.origin.y, frameRect.size.width, 1), NSCompositeSourceOver);
    NSRectFillUsingOperation(NSMakeRect(frameRect.origin.x, frameRect.origin.y + frameRect.size.height-1, frameRect.size.width, 1), NSCompositeSourceOver);
    NSRectFillUsingOperation(NSMakeRect(frameRect.origin.x, frameRect.origin.y, 1, frameRect.size.height), NSCompositeSourceOver);
    NSRectFillUsingOperation(NSMakeRect(frameRect.origin.x+frameRect.size.width-1, frameRect.origin.y, 1, frameRect.size.height), NSCompositeSourceOver);
}

- (void)drawRect:(NSRect)rect
{
    // Draw default to make sure the slider behaves correctly
    [[NSGraphicsContext currentContext] saveGraphicsState];
    NSRectClip(NSZeroRect);
    [super drawRect:rect];
    [[NSGraphicsContext currentContext] restoreGraphicsState];

    // Full size
    rect = [self bounds];
    int diff = (int)(([[self cell] knobThickness] - 7.0)/2.0) - 1;
    rect.origin.x += diff-1;
    rect.origin.y += diff;
    rect.size.width -= 2*diff-2;
    rect.size.height -= 2*diff;

    // Draw dark
    NSRect knobRect = [[self cell] knobRectFlipped:NO];
    [[[NSColor blackColor] colorWithAlphaComponent:0.6] set];
    _drawFrameInRect(rect);
    _drawKnobInRect(knobRect);

    // Draw shadow
    [[[NSColor blackColor] colorWithAlphaComponent:0.1] set];
    rect.origin.x++;
    rect.origin.y++;
    knobRect.origin.x++;
    knobRect.origin.y++;
    _drawFrameInRect(rect);
    _drawKnobInRect(knobRect);
}

@end

/*****************************************************************************
 * ProgressView
 *****************************************************************************/

@implementation VLCProgressView : NSView

- (void)scrollWheel:(NSEvent *)o_event
{
    intf_thread_t * p_intf = VLCIntf;
    CGFloat f_deltaY = [o_event deltaY];
    CGFloat f_deltaX = [o_event deltaX];

    if (!OSX_SNOW_LEOPARD && [o_event isDirectionInvertedFromDevice])
        f_deltaX = -f_deltaX; // optimisation, actually double invertion of f_deltaY here
    else
        f_deltaY = -f_deltaY;

    // positive for left / down, negative otherwise
    CGFloat f_delta = f_deltaX + f_deltaY;
    CGFloat f_abs;
    int i_vlckey;

    if (f_delta > 0.0f) {
        i_vlckey = ACTIONID_JUMP_BACKWARD_EXTRASHORT;
        f_abs = f_delta;
    }
    else {
        i_vlckey = ACTIONID_JUMP_FORWARD_EXTRASHORT;
        f_abs = -f_delta;
    }

    for (NSUInteger i = 0; i < (int)(f_abs/4.+1.) && f_abs > 0.05 ; i++)
        var_SetInteger( p_intf->p_libvlc, "key-action", i_vlckey );
}

- (BOOL)acceptsFirstResponder
{
    return YES;
}

@end

/*****************************************************************************
 * TimeLineSlider
 *****************************************************************************/

@implementation TimeLineSlider

- (void)awakeFromNib
{
    if (config_GetInt( VLCIntf, "macosx-interfacestyle" )) {
        o_knob_img = [NSImage imageNamed:@"progression-knob_dark"];
        b_dark = YES;
    } else {
        o_knob_img = [NSImage imageNamed:@"progression-knob"];
        b_dark = NO;
    }
    img_rect.size = [o_knob_img size];
    img_rect.origin.x = img_rect.origin.y = 0;
}

- (void)dealloc
{
    [o_knob_img release];
    [super dealloc];
}

- (CGFloat)knobPosition
{
    NSRect knobRect = [[self cell] knobRectFlipped:NO];
    knobRect.origin.x += knobRect.size.width / 2;
    return knobRect.origin.x;
}

- (void)drawKnobInRect:(NSRect)knobRect
{
    knobRect.origin.x += (knobRect.size.width - img_rect.size.width) / 2;
    knobRect.size.width = img_rect.size.width;
    knobRect.size.height = img_rect.size.height;
    [o_knob_img drawInRect:knobRect fromRect:img_rect operation:NSCompositeSourceOver fraction:1];
}

- (void)drawRect:(NSRect)rect
{
    [[[[VLCMain sharedInstance] mainWindow] controlsBar] drawFancyGradientEffectForTimeSlider];
    msleep( 10000 ); //wait for the gradient to draw completely

    /* Draw default to make sure the slider behaves correctly */
    [[NSGraphicsContext currentContext] saveGraphicsState];
    NSRectClip(NSZeroRect);
    [super drawRect:rect];
    [[NSGraphicsContext currentContext] restoreGraphicsState];

    NSRect knobRect = [[self cell] knobRectFlipped:NO];
    if (b_dark)
        knobRect.origin.y+=2;
    else
        knobRect.origin.y+=1;
    [self drawKnobInRect: knobRect];
}

@end

/*****************************************************************************
 * VLCVolumeSliderCommon
 *****************************************************************************/

@implementation VLCVolumeSliderCommon : NSSlider

- (void)scrollWheel:(NSEvent *)o_event
{
    intf_thread_t * p_intf = VLCIntf;
    CGFloat f_deltaY = [o_event deltaY];
    CGFloat f_deltaX = [o_event deltaX];

    if (!OSX_SNOW_LEOPARD && [o_event isDirectionInvertedFromDevice])
        f_deltaX = -f_deltaX; // optimisation, actually double invertion of f_deltaY here
    else
        f_deltaY = -f_deltaY;

    // positive for left / down, negative otherwise
    CGFloat f_delta = f_deltaX + f_deltaY;
    CGFloat f_abs;
    int i_vlckey;

    if (f_delta > 0.0f) {
        i_vlckey = ACTIONID_VOL_DOWN;
        f_abs = f_delta;
    }
    else {
        i_vlckey = ACTIONID_VOL_UP;
        f_abs = -f_delta;
    }

    for (NSUInteger i = 0; i < (int)(f_abs/4.+1.) && f_abs > 0.05 ; i++)
        var_SetInteger(p_intf->p_libvlc, "key-action", i_vlckey);
}

@end

/*****************************************************************************
 * ITSlider
 *****************************************************************************/

@implementation ITSlider

- (void)awakeFromNib
{
    BOOL b_dark = config_GetInt( VLCIntf, "macosx-interfacestyle" );
    if (b_dark)
        img = [NSImage imageNamed:@"volume-slider-knob_dark"];
    else
        img = [NSImage imageNamed:@"volume-slider-knob"];

    image_rect.size = [img size];
    image_rect.origin.x = 0;

    if (b_dark)
        image_rect.origin.y = -1;
    else
        image_rect.origin.y = 0;
}

- (void)drawKnobInRect:(NSRect)knobRect
{
    knobRect.origin.x += (knobRect.size.width - image_rect.size.width) / 2;
    knobRect.size.width = image_rect.size.width;
    knobRect.size.height = image_rect.size.height;
    [img drawInRect:knobRect fromRect:image_rect operation:NSCompositeSourceOver fraction:1];
}

- (void)drawRect:(NSRect)rect
{
    /* Draw default to make sure the slider behaves correctly */
    [[NSGraphicsContext currentContext] saveGraphicsState];
    NSRectClip(NSZeroRect);
    [super drawRect:rect];
    [[NSGraphicsContext currentContext] restoreGraphicsState];

    NSRect knobRect = [[self cell] knobRectFlipped:NO];
    knobRect.origin.y+=2;
    [self drawKnobInRect: knobRect];
}

@end

/*****************************************************************************
 * VLCTimeField implementation
 *****************************************************************************
 * we need this to catch our click-event in the controller window
 *****************************************************************************/

@implementation VLCTimeField
+ (void)initialize{
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    NSDictionary *appDefaults = [NSDictionary dictionaryWithObject:@"NO" forKey:@"DisplayTimeAsTimeRemaining"];

    [defaults registerDefaults:appDefaults];
}

- (void)awakeFromNib
{
    NSColor *o_string_color;
    if (!config_GetInt( VLCIntf, "macosx-interfacestyle"))
        o_string_color = [NSColor colorWithCalibratedRed:0.229 green:0.229 blue:0.229 alpha:100.0];
    else
        o_string_color = [NSColor colorWithCalibratedRed:0.64 green:0.64 blue:0.64 alpha:100.0];

    textAlignment = NSCenterTextAlignment;
    o_string_attributes_dict = [[NSDictionary dictionaryWithObjectsAndKeys: o_string_color, NSForegroundColorAttributeName, [NSFont titleBarFontOfSize:10.0], NSFontAttributeName, nil] retain];
}

- (void)setAlignment:(NSTextAlignment)alignment
{
    textAlignment = alignment;
    [self setStringValue:[self stringValue]];
}

- (void)dealloc
{
    [o_string_shadow release];
    [o_string_attributes_dict release];
    [super dealloc];
}

- (void)setStringValue:(NSString *)string
{
    if (!o_string_shadow) {
        o_string_shadow = [[NSShadow alloc] init];
        [o_string_shadow setShadowColor: [NSColor colorWithCalibratedWhite:1.0 alpha:0.5]];
        [o_string_shadow setShadowOffset:NSMakeSize(0.0, -1.5)];
        [o_string_shadow setShadowBlurRadius:0.0];
    }

    NSMutableAttributedString *o_attributed_string = [[NSMutableAttributedString alloc] initWithString:string attributes: o_string_attributes_dict];
    NSUInteger i_stringLength = [string length];

    [o_attributed_string addAttribute: NSShadowAttributeName value: o_string_shadow range: NSMakeRange(0, i_stringLength)];
    [o_attributed_string setAlignment: textAlignment range: NSMakeRange(0, i_stringLength)];
    [self setAttributedStringValue: o_attributed_string];
    [o_attributed_string release];
}

- (void)mouseDown: (NSEvent *)ourEvent
{
    if ( [ourEvent clickCount] > 1 )
        [[[VLCMain sharedInstance] controls] goToSpecificTime: nil];
    else
    {
        if ([[NSUserDefaults standardUserDefaults] boolForKey:@"DisplayTimeAsTimeRemaining"])
            [[NSUserDefaults standardUserDefaults] setObject:@"NO" forKey:@"DisplayTimeAsTimeRemaining"];
        else
            [[NSUserDefaults standardUserDefaults] setObject:@"YES" forKey:@"DisplayTimeAsTimeRemaining"];
    }
}

- (BOOL)timeRemaining
{
    return [[NSUserDefaults standardUserDefaults] boolForKey:@"DisplayTimeAsTimeRemaining"];
}
@end

/*****************************************************************************
 * VLCMainWindowSplitView implementation
 * comment 1 + 2 taken from NSSplitView.h (10.7 SDK)
 *****************************************************************************/
@implementation VLCMainWindowSplitView : NSSplitView
/* Return the color of the dividers that the split view is drawing between subviews. The default implementation of this method returns [NSColor clearColor] for the thick divider style. It will also return [NSColor clearColor] for the thin divider style when the split view is in a textured window. All other thin dividers are drawn with a color that looks good between two white panes. You can override this method to change the color of dividers.
 */
- (NSColor *)dividerColor
{
    return [NSColor colorWithCalibratedRed:.60 green:.60 blue:.60 alpha:1.];
}

/* Return the thickness of the dividers that the split view is drawing between subviews. The default implementation returns a value that depends on the divider style. You can override this method to change the size of dividers.
 */
- (CGFloat)dividerThickness
{
    return 1.0;
}
@end

/*****************************************************************************
 * VLCThreePartImageView interface
 *****************************************************************************/
@implementation VLCThreePartImageView

- (void)dealloc
{
    [o_left_img release];
    [o_middle_img release];
    [o_right_img release];

    [super dealloc];
}

- (void)setImagesLeft:(NSImage *)left middle: (NSImage *)middle right:(NSImage *)right
{
    if (o_left_img)
        [o_left_img release];
    if (o_middle_img)
        [o_middle_img release];
    if (o_right_img)
        [o_right_img release];

    o_left_img = [left retain];
    o_middle_img = [middle retain];
    o_right_img = [right retain];
}

- (void)drawRect:(NSRect)rect
{
    NSRect bnds = [self bounds];
    NSDrawThreePartImage( bnds, o_left_img, o_middle_img, o_right_img, NO, NSCompositeSourceOver, 1, NO );
}

@end

@implementation VLCThreePartDropView

- (BOOL)mouseDownCanMoveWindow
{
    return YES;
}

- (void)dealloc
{
    [self unregisterDraggedTypes];
    [super dealloc];
}

- (void)awakeFromNib
{
    [self registerForDraggedTypes:[NSArray arrayWithObject: NSFilenamesPboardType]];
}

- (NSDragOperation)draggingEntered:(id <NSDraggingInfo>)sender
{
    if ((NSDragOperationGeneric & [sender draggingSourceOperationMask]) == NSDragOperationGeneric)
        return NSDragOperationGeneric;

    return NSDragOperationNone;
}

- (BOOL)prepareForDragOperation:(id <NSDraggingInfo>)sender
{
    return YES;
}

- (BOOL)performDragOperation:(id <NSDraggingInfo>)sender
{
    BOOL b_returned;
    b_returned = [[VLCCoreInteraction sharedInstance] performDragOperation: sender];

    [self setNeedsDisplay:YES];
    return YES;
}

- (void)concludeDragOperation:(id <NSDraggingInfo>)sender
{
    [self setNeedsDisplay:YES];
}

@end
