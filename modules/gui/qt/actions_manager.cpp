/*****************************************************************************
 * actions_manager.cpp : Controller for the main interface
 ****************************************************************************
 * Copyright © 2009-2014 VideoLAN and VLC authors
 * $Id$
 *
 * Authors: Jean-Baptiste Kempf <jb@videolan.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * ( at your option ) any later version.
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_vout.h>
#include <vlc_actions.h>
#include <vlc_renderer_discovery.h>

#include "actions_manager.hpp"

#include "dialogs_provider.hpp"      /* Opening Dialogs */
#include "input_manager.hpp"         /* THEMIM */
#include "main_interface.hpp"        /* Show playlist */
#include "components/controller.hpp" /* Toggle FSC controller width */
#include "components/extended_panels.hpp"
#include "menus.hpp"

ActionsManager::ActionsManager( intf_thread_t * _p_i )
    : p_intf( _p_i )
{ }

ActionsManager::~ActionsManager()
{
    foreach ( vlc_renderer_discovery_t* p_rd, m_rds )
        vlc_rd_release( p_rd );
}

void ActionsManager::doAction( int id_action )
{
    switch( id_action )
    {
        case PLAY_ACTION:
            play(); break;
        case STOP_ACTION:
            THEMIM->stop(); break;
        case OPEN_ACTION:
            THEDP->openDialog(); break;
        case PREVIOUS_ACTION:
            THEMIM->prev(); break;
        case NEXT_ACTION:
            THEMIM->next(); break;
        case SLOWER_ACTION:
            THEMIM->getIM()->slower(); break;
        case FASTER_ACTION:
            THEMIM->getIM()->faster(); break;
        case FULLSCREEN_ACTION:
            fullscreen(); break;
        case EXTENDED_ACTION:
            THEDP->extendedDialog(); break;
        case PLAYLIST_ACTION:
            playlist(); break;
        case SNAPSHOT_ACTION:
            snapshot(); break;
        case RECORD_ACTION:
            record(); break;
        case FRAME_ACTION:
            frame(); break;
        case ATOB_ACTION:
            THEMIM->getIM()->setAtoB(); break;
        case REVERSE_ACTION:
            THEMIM->getIM()->reverse(); break;
        case SKIP_BACK_ACTION:
            skipBackward();
            break;
        case SKIP_FW_ACTION:
            skipForward();
            break;
        case QUIT_ACTION:
            THEDP->quit();  break;
        case RANDOM_ACTION:
            THEMIM->toggleRandom(); break;
        case INFO_ACTION:
            THEDP->mediaInfoDialog(); break;
        case OPEN_SUB_ACTION:
            THEDP->loadSubtitlesFile(); break;
        case FULLWIDTH_ACTION:
            if( p_intf->p_sys->p_mi )
                p_intf->p_sys->p_mi->getFullscreenControllerWidget()->toggleFullwidth();
            break;
        default:
            msg_Warn( p_intf, "Action not supported: %i", id_action );
            break;
    }
}

void ActionsManager::play()
{
    if( THEPL->current.i_size == 0 && THEPL->items.i_size == 0 )
    {
        /* The playlist is empty, open a file requester */
        THEDP->openFileDialog();
        return;
    }
    THEMIM->togglePlayPause();
}

/**
 * TODO
 * This functions toggle the fullscreen mode
 * If there is no video, it should first activate Visualisations...
 * This has also to be fixed in enableVideo()
 */
void ActionsManager::fullscreen()
{
    bool fs = var_ToggleBool( THEPL, "fullscreen" );
    vout_thread_t *p_vout = THEMIM->getVout();
    if( p_vout)
    {
        var_SetBool( p_vout, "fullscreen", fs );
        vlc_object_release( p_vout );
    }
}

void ActionsManager::snapshot()
{
    vout_thread_t *p_vout = THEMIM->getVout();
    if( p_vout )
    {
        var_TriggerCallback( p_vout, "video-snapshot" );
        vlc_object_release( p_vout );
    }
}

void ActionsManager::playlist()
{
    if( p_intf->p_sys->p_mi )
        p_intf->p_sys->p_mi->togglePlaylist();
}

void ActionsManager::record()
{
    input_thread_t *p_input = THEMIM->getInput();
    if( p_input )
    {
        /* This method won't work fine if the stream can't be cut anywhere */
        var_ToggleBool( p_input, "record" );
#if 0
        else
        {
            /* 'record' access-filter is not loaded, we open Save dialog */
            input_item_t *p_item = input_GetItem( p_input );
            if( !p_item )
                return;

            char *psz = input_item_GetURI( p_item );
            if( psz )
                THEDP->streamingDialog( NULL, qfu(psz), true );
        }
#endif
    }
}

void ActionsManager::frame()
{
    input_thread_t *p_input = THEMIM->getInput();
    if( p_input )
        var_TriggerCallback( p_input, "frame-next" );
}

void ActionsManager::toggleMuteAudio()
{
    playlist_MuteToggle( THEPL );
}

void ActionsManager::AudioUp()
{
    playlist_VolumeUp( THEPL, 1, NULL );
}

void ActionsManager::AudioDown()
{
    playlist_VolumeDown( THEPL, 1, NULL );
}

void ActionsManager::skipForward()
{
    input_thread_t *p_input = THEMIM->getInput();
    if( p_input )
        THEMIM->getIM()->jumpFwd();
}

void ActionsManager::skipBackward()
{
    input_thread_t *p_input = THEMIM->getInput();
    if( p_input )
        THEMIM->getIM()->jumpBwd();
}

bool ActionsManager::compareRenderers( const QVariant &obj, vlc_renderer_item_t* p_item )
{
    if (!obj.canConvert<QVariantHash>())
        return false;
    QVariantHash qvh = obj.value<QVariantHash>();
    if (!qvh.contains( "sout" ))
        return false;
    vlc_renderer_item_t* p_existing =
            reinterpret_cast<vlc_renderer_item_t*>( qvh["sout"].value<void*>() );
    return !strcasecmp(vlc_renderer_item_sout( p_existing ),
                    vlc_renderer_item_sout( p_item ) );
}

void ActionsManager::renderer_event_item_added( vlc_renderer_discovery_t*,
                                                vlc_renderer_item_t *p_item )
{
    QAction *firstSeparator = NULL;

    foreach (QAction* action, VLCMenuBar::rendererMenu->actions())
    {
        if (action->isSeparator())
        {
            firstSeparator = action;
            break;
        }
        QVariant v = action->data();
        if (compareRenderers( action->data(), p_item ))
            return; /* we already have this item */
    }

    QAction *action = new QAction( vlc_renderer_item_flags(p_item) & VLC_RENDERER_CAN_VIDEO ? QIcon( ":/sidebar/movie.svg" ) : QIcon( ":/sidebar/music.svg" ),
                                   vlc_renderer_item_name(p_item), VLCMenuBar::rendererMenu );
    action->setCheckable(true);

    vlc_renderer_item_hold( p_item );
    QVariantHash data;
    data.insert( "sout", QVariant::fromValue( reinterpret_cast<void*>( p_item ) ) );
    action->setData( data );
    if (firstSeparator != NULL)
    {
        VLCMenuBar::rendererMenu->insertAction( firstSeparator, action );
        VLCMenuBar::rendererGroup->addAction(action);
    }
}

void ActionsManager::renderer_event_item_removed(
    vlc_renderer_discovery_t *p_rd, vlc_renderer_item_t *p_item )
{
    foreach (QAction* action, VLCMenuBar::rendererMenu->actions())
    {
        if (action->isSeparator())
            continue;
        if (compareRenderers( action->data(), p_item ))
        {
            if( action->isChecked() )
            {
                intf_thread_t* p_intf = reinterpret_cast<intf_thread_t*>( p_rd->owner.sys );
                playlist_SetRenderer( THEPL, NULL );
            }
            VLCMenuBar::rendererMenu->removeAction( action );
            VLCMenuBar::rendererGroup->removeAction( action );
            vlc_renderer_item_release( p_item );
            return;
        }
    }
}

void ActionsManager::ScanRendererAction(bool checked)
{
    if (checked == !m_rds.empty())
        return; /* nothing changed */

    if (checked)
    {
        /* reset the list of renderers */
        foreach (QAction* action, VLCMenuBar::rendererMenu->actions())
        {
            QVariant data = action->data();
            if (!data.canConvert<QVariantHash>())
                continue;
            VLCMenuBar::rendererMenu->removeAction(action);
            VLCMenuBar::rendererGroup->removeAction(action);
        }

        /* SD subnodes */
        char **ppsz_longnames;
        char **ppsz_names;
        if( vlc_rd_get_names( THEPL, &ppsz_names, &ppsz_longnames ) != VLC_SUCCESS )
            return;

        struct vlc_renderer_discovery_owner owner =
        {
            p_intf,
            renderer_event_item_added,
            renderer_event_item_removed,
        };

        char **ppsz_name = ppsz_names, **ppsz_longname = ppsz_longnames;
        for( ; *ppsz_name; ppsz_name++, ppsz_longname++ )
        {
            msg_Dbg( p_intf, "starting renderer discovery service %s", *ppsz_longname );
            vlc_renderer_discovery_t* p_rd = vlc_rd_new( VLC_OBJECT(p_intf), *ppsz_name, &owner );
            if( p_rd != NULL )
                m_rds.push_back( p_rd );
            free( *ppsz_name );
            free( *ppsz_longname );
        }
        free( ppsz_names );
        free( ppsz_longnames );
    }
    else
    {
        foreach ( vlc_renderer_discovery_t* p_rd, m_rds )
            vlc_rd_release( p_rd );
        m_rds.clear();
    }
}

void ActionsManager::RendererSelected( QAction *selected )
{
    QVariant data = selected->data();
    vlc_renderer_item_t *p_item = NULL;
    if (data.canConvert<QVariantHash>())
    {
        QVariantHash hash = data.value<QVariantHash>();
        if ( hash.contains( "sout" ) )
            p_item = reinterpret_cast<vlc_renderer_item_t*>(
                        hash["sout"].value<void*>() );
    }
    // If we failed to convert the action data to a vlc_renderer_item_t,
    // assume the selected item was invalid, or most likely that "Local" was selected
    playlist_SetRenderer( THEPL, p_item );
}

