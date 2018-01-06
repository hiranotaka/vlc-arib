/*****************************************************************************
 * VLCRendererMenuController.m: Controller class for the renderer menu
 *****************************************************************************
 * Copyright (C) 2016 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Marvin Scholz <epirat07 at gmail dot com>
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

#import "VLCRendererMenuController.h"

#import "VLCRendererItem.h"
#import "VLCMain.h"

#include <vlc_renderer_discovery.h>

@interface VLCRendererMenuController ()
{
    NSMutableDictionary         *_rendererItems;
    NSMutableArray              *_rendererDiscoveries;
    BOOL                         _isDiscoveryEnabled;
    NSMenuItem                  *_selectedItem;

    intf_thread_t               *p_intf;
    vlc_renderer_discovery_t    *p_rd;
}
@end

@implementation VLCRendererMenuController

- (instancetype)init
{
    self = [super init];
    if (self) {
        _rendererItems = [NSMutableDictionary dictionary];
        _rendererDiscoveries = [NSMutableArray array];
        _isDiscoveryEnabled = NO;
        p_intf = getIntf();

        [self loadRendererDiscoveries];
    }
    return self;
}

- (void)awakeFromNib
{
    _selectedItem = _rendererNoneItem;
    [self setupMenu];
}

- (void)dealloc
{
    [self stopRendererDiscoveries];
}

- (void)setupMenu
{
    [_rendererDiscoveryState setTitle:_NS("Renderer discovery off")];
    [_rendererDiscoveryState setEnabled:NO];

    [_rendererDiscoveryToggle setTitle:_NS("Enable renderer discovery")];

    [_rendererNoneItem setTitle:_NS("No renderer")];
    [_rendererNoneItem setState:NSOnState];
}

- (void)loadRendererDiscoveries
{
    playlist_t *playlist = pl_Get(p_intf);

    // Service Discovery subnodes
    char **ppsz_longnames;
    char **ppsz_names;

    if (vlc_rd_get_names(playlist, &ppsz_names, &ppsz_longnames) != VLC_SUCCESS) {
        return;
    }
    char **ppsz_name = ppsz_names;
    char **ppsz_longname = ppsz_longnames;

    for( ; *ppsz_name; ppsz_name++, ppsz_longname++) {
        VLCRendererDiscovery *dc = [[VLCRendererDiscovery alloc] initWithName:*ppsz_name andLongname:*ppsz_longname];
        [dc setDelegate:self];
        [_rendererDiscoveries addObject:dc];
        free(*ppsz_name);
        free(*ppsz_longname);
    }
    free(ppsz_names);
    free(ppsz_longnames);
}

#pragma mark - Renderer item management

- (void)addRendererItem:(VLCRendererItem *)item
{
    // Create a menu item
    NSMenuItem *menuItem = [[NSMenuItem alloc] initWithTitle:item.name
                                                      action:@selector(selectRenderer:)
                                               keyEquivalent:@""];
    [menuItem setTarget:self];
    [menuItem setRepresentedObject:item];
    [_rendererMenu insertItem:menuItem atIndex:[_rendererMenu indexOfItem:_rendererNoneItem] + 1];
}

- (void)removeRendererItem:(VLCRendererItem *)item
{
    NSInteger index = [_rendererMenu indexOfItemWithRepresentedObject:item];
    if (index != NSNotFound) {
        NSMenuItem *menuItem = [_rendererMenu itemAtIndex:index];
        if (menuItem == _selectedItem) {
            [self selectRenderer:_rendererNoneItem];
        }
        [_rendererMenu removeItemAtIndex:index];
    }
}

- (void)startRendererDiscoveries
{
    _isDiscoveryEnabled = YES;
    [_rendererDiscoveryState setTitle:_NS("Renderer discovery on")];
    [_rendererDiscoveryToggle setTitle:_NS("Disable renderer discovery")];
    for (VLCRendererDiscovery *dc in _rendererDiscoveries) {
        [dc startDiscovery];
    }
}

- (void)stopRendererDiscoveries
{
    _isDiscoveryEnabled = NO;
    [_rendererDiscoveryState setTitle:_NS("Renderer discovery off")];
    [_rendererDiscoveryToggle setTitle:_NS("Enable renderer discovery")];
    for (VLCRendererDiscovery *dc in _rendererDiscoveries) {
        [dc stopDiscovery];
    }
}

- (BOOL)validateMenuItem:(NSMenuItem *)menuItem {
    if (menuItem == _rendererNoneItem ||
        [[menuItem representedObject] isKindOfClass:[VLCRendererItem class]]) {
        return _isDiscoveryEnabled;
    }
    return [menuItem isEnabled];
}

- (IBAction)selectRenderer:(NSMenuItem *)sender
{
    [_selectedItem setState:NSOffState];
    [sender setState:NSOnState];
    _selectedItem = sender;

    VLCRendererItem* item = [sender representedObject];
    playlist_t *playlist = pl_Get(p_intf);

    if (!playlist)
        return;

    if (item) {
        [item setRendererForPlaylist:playlist];
    } else {
        [self unsetRendererForPlaylist:playlist];
    }
}

- (void)unsetRendererForPlaylist:(playlist_t*)playlist
{
    playlist_SetRenderer(playlist, NULL);
}

#pragma mark VLCRendererDiscovery delegate methods
- (void)addedRendererItem:(VLCRendererItem *)item from:(VLCRendererDiscovery *)sender
{
    [self addRendererItem:item];
}

- (void)removedRendererItem:(VLCRendererItem *)item from:(VLCRendererDiscovery *)sender
{
    [self removeRendererItem:item];
}

#pragma mark Menu actions
- (IBAction)toggleRendererDiscovery:(id)sender {
    if (_isDiscoveryEnabled) {
        [self stopRendererDiscoveries];
    } else {
        [self startRendererDiscoveries];
    }
}

@end
