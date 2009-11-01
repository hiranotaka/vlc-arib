/*****************************************************************************
 * VLCTime.m: VLCKit.framework VLCTime implementation
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

#import <VLCTime.h>

@implementation VLCTime
/* Factories */
+ (VLCTime *)nullTime
{
    static VLCTime * nullTime = nil;
    if (!nullTime)
        nullTime = [[VLCTime timeWithNumber:nil] retain];
    return nullTime;
}

+ (VLCTime *)timeWithNumber:(NSNumber *)aNumber
{
    return [[[VLCTime alloc] initWithNumber:aNumber] autorelease];
}

+ (VLCTime *)timeWithInt:(int)aInt
{
    return [[[VLCTime alloc] initWithInt:aInt] autorelease];
}

/* Initializers */
- (id)initWithNumber:(NSNumber *)aNumber
{
    if (self = [super init])
    {
        if (aNumber)
            value = [[aNumber copy] retain];
        else
            value = nil;
    }
    return self;
}

- (id)initWithInt:(int)aInt
{
    if (self = [super init])
    {
        if (aInt)
            value = [[NSNumber numberWithInt: aInt] retain];
        else
            value = nil;
    }
    return self;
}

- (void)dealloc
{
    [value release];
    [super dealloc];
}

- (id)copyWithZone:(NSZone *)zone
{
    return [[VLCTime alloc] initWithNumber:value];
}

/* NSObject Overrides */
- (NSString *)description
{
    return self.stringValue;
}

/* Operations */
- (NSNumber *)numberValue
{
    return value ? [[value copy] autorelease] : nil;
}

- (NSString *)stringValue
{
    if (value)
    {
        long long duration = [value longLongValue] / 1000000;
        long long positiveDuration = llabs(duration);
        if( positiveDuration > 3600 )
            return [NSString stringWithFormat:@"%s%01d:%02d:%02d",
                        duration < 0 ? "-" : "",
                (long) (positiveDuration / 3600),
                (long)((positiveDuration / 60) % 60),
                (long) (positiveDuration % 60)];
        else
            return [NSString stringWithFormat:@"%s%02d:%02d",
                            duration < 0 ? "-" : "",
                    (long)((positiveDuration / 60) % 60),
                    (long) (positiveDuration % 60)];
    }
    else
    {
        // Return a string that represents an undefined time.
        return @"--:--";
    }
}

- (int)intValue
{
    if( value )
        return [value intValue];
    return 0;
}

- (NSComparisonResult)compare:(VLCTime *)aTime
{
    if (!aTime && !value)
        return NSOrderedSame;
    else if (!aTime)
        return NSOrderedDescending;
    else
        return [value compare:aTime.numberValue];
}
@end
