/*
 * media_list_player.c - libvlc smoke test
 *
 * $Id$
 */

/**********************************************************************
 *  Copyright (C) 2007 Rémi Denis-Courmont.                           *
 *  This program is free software; you can redistribute and/or modify *
 *  it under the terms of the GNU General Public License as published *
 *  by the Free Software Foundation; version 2 of the license, or (at *
 *  your option) any later version.                                   *
 *                                                                    *
 *  This program is distributed in the hope that it will be useful,   *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of    *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.              *
 *  See the GNU General Public License for more details.              *
 *                                                                    *
 *  You should have received a copy of the GNU General Public License *
 *  along with this program; if not, you can get it from:             *
 *  http://www.gnu.org/copyleft/gpl.html                              *
 **********************************************************************/

#include "test.h"

 // For msleep
#include <vlc_common.h>
#include <vlc_mtime.h>

#include "libvlc_additions.h"

/* 
    HACK - FIX ME
    This allows for the direct addition of subitems in the playback options test.
    This would not be necessary if there were an add subitems function.
*/
#include "../../src/control/media_internal.h"

struct check_items_order_data {
    bool done_playing;
    unsigned count;
    unsigned index;
    void * items[16];
};

static inline void check_data_init(struct check_items_order_data *check)
{
    check->index = 0;
    check->count = 0;
    check->done_playing = false;
}

static inline void queue_expected_item(struct check_items_order_data *check, void *item)
{
    assert(check->count < 16);
    check->items[check->count] = item;
    check->count++;
}

static inline void wait_queued_items(struct check_items_order_data *check)
{
    // Wait dummily for check_items_order_callback() to flag 'done_playing':
    while (!check->done_playing)
        msleep(100000);
}

static void check_items_order_callback(const libvlc_event_t * p_event, void * user_data)
{
    struct check_items_order_data *checks = user_data;
    libvlc_media_t *md = p_event->u.media_list_player_next_item_set.item;
    assert(checks->index < checks->count);
    if (checks->items[checks->index] != md)
    {
        char *title = libvlc_media_get_meta(md, libvlc_meta_Title);
        log ("Got items %s\n", title);
        free(title);
    }
    assert(checks->items[checks->index] == md);
    
    char *title = libvlc_media_get_meta(md, libvlc_meta_Title);
    log ("Item %d '%s' was correctly queued\n", checks->index, title);
    free(title);
    
    if (checks->index == (checks->count - 1))
    {
        log ("Done playing with success\n");
        checks->done_playing = true;
    }
    checks->index++;
}

static void test_media_list_player_items_queue(const char** argv, int argc)
{
    libvlc_instance_t *vlc;
    libvlc_media_t *md;
    libvlc_media_list_t *ml;
    libvlc_media_list_player_t *mlp;
    
    const char * file = test_default_sample;
    
    log ("Testing media player item queue-ing\n");
    
    libvlc_exception_init (&ex);
    vlc = libvlc_new (argc, argv, &ex);
    catch ();
    
    md = libvlc_media_new (vlc, file, &ex);
    catch ();
    
    ml = libvlc_media_list_new (vlc, &ex);
    catch ();
    
    mlp = libvlc_media_list_player_new (vlc, &ex);
    catch ();
    
    libvlc_media_list_add_media (ml, md, &ex);
    catch ();

    static struct check_items_order_data check;
    check_data_init(&check);
    queue_expected_item(&check, md);

    // Add three more media
    queue_expected_item(&check, media_list_add_file_path (vlc, ml, file));
    queue_expected_item(&check, media_list_add_file_path (vlc, ml, file));
    queue_expected_item(&check, media_list_add_file_path (vlc, ml, file));

    // Add a node
    libvlc_media_t *node = libvlc_media_new_as_node(vlc, "node", &ex);
    catch ();
    libvlc_media_list_add_media(ml, node, &ex);
    catch ();
    queue_expected_item(&check, node);

    // Add items to that node
    libvlc_media_list_t *subitems = libvlc_media_subitems(node);
    queue_expected_item(&check, media_list_add_file_path(vlc, subitems, file));
    queue_expected_item(&check, media_list_add_file_path(vlc, subitems, file));
    queue_expected_item(&check, media_list_add_file_path(vlc, subitems, file));
    libvlc_media_list_release(subitems);
    
    libvlc_media_list_player_set_media_list (mlp, ml, &ex);

    libvlc_event_manager_t * em = libvlc_media_list_player_event_manager(mlp);
    libvlc_event_attach(em, libvlc_MediaListPlayerNextItemSet, check_items_order_callback, &check, &ex);
    catch ();

    libvlc_media_list_player_play(mlp, &ex);
    catch ();

    // Wait until all item are read
    wait_queued_items(&check);

    libvlc_media_list_player_stop (mlp, &ex);
    catch ();

    libvlc_media_list_player_release (mlp);
    catch ();
    
    libvlc_release (vlc);
    catch ();
}

static void test_media_list_player_previous(const char** argv, int argc)
{
    libvlc_instance_t *vlc;
    libvlc_media_t *md;
    libvlc_media_list_t *ml;
    libvlc_media_list_player_t *mlp;
    
    const char * file = test_default_sample;
    
    log ("Testing media player previous()\n");
    
    libvlc_exception_init (&ex);
    vlc = libvlc_new (argc, argv, &ex);
    catch ();
    
    md = libvlc_media_new (vlc, file, &ex);
    catch ();
    
    ml = libvlc_media_list_new (vlc, &ex);
    catch ();
    
    mlp = libvlc_media_list_player_new (vlc, &ex);
    catch ();

    libvlc_media_list_add_media (ml, md, &ex);
    catch ();

    // Add three media
    media_list_add_file_path (vlc, ml, file);
    media_list_add_file_path (vlc, ml, file);
    media_list_add_file_path (vlc, ml, file);

    libvlc_media_list_player_set_media_list (mlp, ml, &ex);

    libvlc_media_list_player_play_item (mlp, md, &ex);
    catch ();

    libvlc_media_release (md);

    msleep(100000);
    
    libvlc_media_list_player_previous (mlp, &ex);
    catch ();

    libvlc_media_list_player_pause (mlp, &ex);
    catch();

    msleep(100000);
    
    libvlc_media_list_player_previous (mlp, &ex);
    catch ();
    
    libvlc_media_list_player_stop (mlp, &ex);
    catch ();

    msleep(100000);
    
    libvlc_media_list_player_previous (mlp, &ex);
    catch ();
    
    libvlc_media_list_player_release (mlp);
    catch ();
    
    libvlc_release (vlc);
    catch ();
}

static void test_media_list_player_next(const char** argv, int argc)
{
    libvlc_instance_t *vlc;
    libvlc_media_t *md;
    libvlc_media_list_t *ml;
    libvlc_media_list_player_t *mlp;
    
    const char * file = test_default_sample;
    
    log ("Testing media player next()\n");
    
    libvlc_exception_init (&ex);
    vlc = libvlc_new (argc, argv, &ex);
    catch ();
    
    md = libvlc_media_new (vlc, file, &ex);
    catch ();
    
    ml = libvlc_media_list_new (vlc, &ex);
    catch ();
    
    mlp = libvlc_media_list_player_new (vlc, &ex);
    catch ();

    libvlc_media_list_add_media (ml, md, &ex);
    catch ();

    // Add three media
    media_list_add_file_path (vlc, ml, file);
    media_list_add_file_path (vlc, ml, file);
    media_list_add_file_path (vlc, ml, file);

    libvlc_media_list_player_set_media_list (mlp, ml, &ex);
    
    libvlc_media_list_player_play_item (mlp, md, &ex);
    catch ();

    libvlc_media_release (md);

    msleep(100000);
    
    libvlc_media_list_player_next (mlp, &ex);
    catch ();

    libvlc_media_list_player_pause (mlp, &ex);
    catch();

    msleep(100000);
    
    libvlc_media_list_player_next (mlp, &ex);
    catch ();
    
    libvlc_media_list_player_stop (mlp, &ex);
    catch ();

    msleep(100000);
    
    libvlc_media_list_player_next (mlp, &ex);
    catch ();
        
    libvlc_media_list_player_release (mlp);
    catch ();
    
    libvlc_release (vlc);
    catch ();
}

static void test_media_list_player_pause_stop(const char** argv, int argc)
{
    libvlc_instance_t *vlc;
    libvlc_media_t *md;
    libvlc_media_list_t *ml;
    libvlc_media_list_player_t *mlp;

    const char * file = test_default_sample;

    log ("Testing play and pause of %s using the media list.\n", file);

    libvlc_exception_init (&ex);
    vlc = libvlc_new (argc, argv, &ex);
    catch ();

    md = libvlc_media_new (vlc, file, &ex);
    catch ();

    ml = libvlc_media_list_new (vlc, &ex);
    catch ();

    mlp = libvlc_media_list_player_new (vlc, &ex);

    libvlc_media_list_add_media( ml, md, &ex );
    catch ();

    libvlc_media_list_player_set_media_list( mlp, ml, &ex );

    libvlc_media_list_player_play_item( mlp, md, &ex );
    catch ();

    libvlc_media_list_player_pause (mlp, &ex);
    catch();

    libvlc_media_list_player_stop (mlp, &ex);
    catch ();

    libvlc_media_release (md);

    libvlc_media_list_player_release (mlp);
    catch ();

    libvlc_release (vlc);
    catch ();
}

static void test_media_list_player_play_item_at_index(const char** argv, int argc)
{
    libvlc_instance_t *vlc;
    libvlc_media_t *md;
    libvlc_media_list_t *ml;
    libvlc_media_list_player_t *mlp;

    const char * file = test_default_sample;

    log ("Testing play_item_at_index of %s using the media list.\n", file);

    libvlc_exception_init (&ex);
    vlc = libvlc_new (argc, argv, &ex);
    catch ();

    md = libvlc_media_new (vlc, file, &ex);
    catch ();

    ml = libvlc_media_list_new (vlc, &ex);
    catch ();

    mlp = libvlc_media_list_player_new (vlc, &ex);

    for (unsigned i = 0; i < 5; i++)
    {
        libvlc_media_list_add_media( ml, md, &ex );
        catch ();
    }

    libvlc_media_list_player_set_media_list( mlp, ml, &ex );

    libvlc_media_list_player_play_item_at_index( mlp, 0, &ex );
    catch ();

    libvlc_media_list_player_stop (mlp, &ex);
    catch ();

    libvlc_media_release (md);
    catch ();

    libvlc_media_list_player_release (mlp);
    catch ();

    libvlc_release (vlc);
    catch ();
}

static void test_media_list_player_playback_options (const char** argv, int argc)
{
    libvlc_instance_t *vlc;
    libvlc_media_t *md;
    libvlc_media_t *md2;
    libvlc_media_t *md3;
    libvlc_media_t *md4;
    libvlc_media_t *md5;
    libvlc_media_list_t *ml;
    libvlc_media_list_t *ml2;
    libvlc_media_list_t *ml3;
    libvlc_media_list_t *ml4;
    libvlc_media_list_t *ml5;
    libvlc_media_list_t *ml6;
    libvlc_media_list_player_t *mlp;
    
    const char * file = test_default_sample;
    
    log ("Testing media player playback options()\n");
    
    libvlc_exception_init (&ex);
    vlc = libvlc_new (argc, argv, &ex);
    catch ();
    
    /*
     *   Create the following media tree:
     *
     *  ml1:            0 ---- 1 ---- 2
     *                 /       |       \
     *  ml2&4:      0 -- 1     |   0 -- 1 -- 2
     *                         |
     *  ml3:    0 -- 1 -- 2 -- 3 -- 4 -- 5 -- 6
     *                    |                   |
     *  ml5&6:            0                 0 -- 1
     */

    md = libvlc_media_new (vlc, file, &ex);
    catch ();

    md2 = libvlc_media_new (vlc, file, &ex);
    catch ();

    md3 = libvlc_media_new (vlc, file, &ex);
    catch ();

    md4 = libvlc_media_new (vlc, file, &ex);
    catch ();

    md5 = libvlc_media_new (vlc, file, &ex);
    catch ();
    
    ml = libvlc_media_list_new (vlc, &ex);
    catch ();
    
    ml2 = libvlc_media_list_new (vlc, &ex);
    catch ();

    ml3 = libvlc_media_list_new (vlc, &ex);
    catch ();

    ml4 = libvlc_media_list_new (vlc, &ex);
    catch ();

    ml5 = libvlc_media_list_new (vlc, &ex);
    catch ();

    ml6 = libvlc_media_list_new (vlc, &ex);
    catch ();

    media_list_add_file_path (vlc, ml2, file);
    media_list_add_file_path (vlc, ml2, file);

    media_list_add_file_path (vlc, ml3, file);
    media_list_add_file_path (vlc, ml3, file);
    libvlc_media_list_add_media (ml3, md4, &ex);
    catch ();
    media_list_add_file_path (vlc, ml3, file);
    media_list_add_file_path (vlc, ml3, file);
    media_list_add_file_path (vlc, ml3, file);
    libvlc_media_list_add_media (ml3, md5, &ex);
    catch ();

    media_list_add_file_path (vlc, ml4, file);
    media_list_add_file_path (vlc, ml4, file);
    media_list_add_file_path (vlc, ml4, file);

    media_list_add_file_path (vlc, ml5, file);

    media_list_add_file_path (vlc, ml6, file);
    media_list_add_file_path (vlc, ml6, file);

    md->p_subitems = ml2;
    md2->p_subitems = ml3;
    md3->p_subitems = ml4;
    md4->p_subitems = ml5;
    md5->p_subitems = ml6;

    libvlc_media_list_add_media (ml, md, &ex);
    catch ();

    libvlc_media_list_add_media (ml, md2, &ex);
    catch ();

    libvlc_media_list_add_media (ml, md3, &ex);
    catch ();
    
    mlp = libvlc_media_list_player_new (vlc, &ex);
    catch ();

    libvlc_media_list_player_set_media_list (mlp, ml, &ex);
    catch ();
    
    // Test default playback mode 
    libvlc_media_list_player_set_playback_mode(mlp, libvlc_playback_mode_default, &ex);
    catch ();

    libvlc_media_list_player_play_item (mlp, md, &ex);
    catch ();

    libvlc_media_release (md);
    catch ();
    libvlc_media_release (md2);
    catch ();
    libvlc_media_release (md3);
    catch ();
    libvlc_media_release (md4);
    catch ();
    libvlc_media_release (md5);
    catch ();

    msleep(500000);

    libvlc_media_list_player_stop (mlp, &ex);
    catch ();

    // Test looping playback mode
    log ("Testing media player playback option - Loop\n");
    libvlc_media_list_player_set_playback_mode(mlp, libvlc_playback_mode_loop, &ex);
    catch ();

    libvlc_media_list_player_play_item (mlp, md, &ex);
    catch ();

    msleep(500000);

    libvlc_media_list_player_stop (mlp, &ex);
    catch ();

    // Test repeat playback mode
    log ("Testing media player playback option - Repeat\n");
    libvlc_media_list_player_set_playback_mode(mlp, libvlc_playback_mode_repeat, &ex);
    catch ();

    libvlc_media_list_player_play_item (mlp, md, &ex);
    catch ();

    msleep(500000);
        
    libvlc_media_list_player_release (mlp);
    catch ();
    
    libvlc_release (vlc);
    catch ();
}


int main (void)
{
    test_init();

    test_media_list_player_pause_stop (test_defaults_args, test_defaults_nargs);
    test_media_list_player_play_item_at_index (test_defaults_args, test_defaults_nargs);
    test_media_list_player_previous (test_defaults_args, test_defaults_nargs);
    test_media_list_player_next (test_defaults_args, test_defaults_nargs);
    test_media_list_player_items_queue (test_defaults_args, test_defaults_nargs);
    test_media_list_player_playback_options (test_defaults_args, test_defaults_nargs);
    return 0;
}
