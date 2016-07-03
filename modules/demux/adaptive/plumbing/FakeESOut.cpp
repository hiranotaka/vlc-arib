/*
 * FakeESOut.cpp
 *****************************************************************************
 * Copyright © 2014-2015 VideoLAN and VLC Authors
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "FakeESOut.hpp"
#include "FakeESOutID.hpp"
#include <vlc_es_out.h>
#include <vlc_block.h>
#include <cassert>

using namespace adaptive;

FakeESOut::FakeESOut( es_out_t *es, CommandsFactory *factory )
{
    real_es_out = es;
    fakeesout = new es_out_t;

    fakeesout->pf_add = esOutAdd_Callback;
    fakeesout->pf_control = esOutControl_Callback;
    fakeesout->pf_del = esOutDel_Callback;
    fakeesout->pf_destroy = esOutDestroy_Callback;
    fakeesout->pf_send = esOutSend_Callback;
    fakeesout->p_sys = (es_out_sys_t*) this;

    commandsFactory = factory;
    timestamps_offset = 0;
    extrainfo = NULL;
}

es_out_t * FakeESOut::getEsOut()
{
    return fakeesout;
}

FakeESOut::~FakeESOut()
{
    delete commandsFactory;

    recycleAll();
    gc();

    delete fakeesout;
}

void FakeESOut::setTimestampOffset(mtime_t offset)
{
    timestamps_offset = offset;
}

void FakeESOut::setExtraInfoProvider( ExtraFMTInfoInterface *extra )
{
    extrainfo = extra;
}

FakeESOutID * FakeESOut::createNewID( const es_format_t *p_fmt )
{
    es_format_t fmtcopy;
    es_format_Init( &fmtcopy, 0, 0 );
    es_format_Copy( &fmtcopy, p_fmt );
    fmtcopy.i_group = 0; /* Always ignore group for adaptive */

    if( extrainfo )
        extrainfo->fillExtraFMTInfo( &fmtcopy );

    FakeESOutID *es_id = new (std::nothrow) FakeESOutID( this, &fmtcopy );
    if(likely(es_id))
        fakeesidlist.push_back( es_id );

    es_format_Clean( &fmtcopy );

    return es_id;
}

void FakeESOut::createOrRecycleRealEsID( FakeESOutID *es_id )
{
    std::list<FakeESOutID *>::iterator it;
    es_out_id_t *realid = NULL;

    bool b_select = false;
    for( it=recycle_candidates.begin(); it!=recycle_candidates.end(); ++it )
    {
        FakeESOutID *cand = *it;
        if ( cand->isCompatible( es_id ) )
        {
            realid = cand->realESID();
            cand->setRealESID( NULL );
            delete *it;
            recycle_candidates.erase( it );
            break;
        }
        else if( cand->getFmt()->i_cat == es_id->getFmt()->i_cat )
        {
            /* We need to enforce same selection when not reused
               Otherwise the es will select any other compatible track
               and will end this in a activate/select loop when reactivating a track */
            es_out_Control( real_es_out, ES_OUT_GET_ES_STATE, cand->realESID(), &b_select );
            break;
        }
    }

    if( !realid )
    {
        realid = es_out_Add( real_es_out, es_id->getFmt() );
        if( b_select )
            es_out_Control( real_es_out, ES_OUT_SET_ES_STATE, realid, b_select );
    }

    es_id->setRealESID( realid );
}

mtime_t FakeESOut::getTimestampOffset() const
{
    return timestamps_offset;
}

size_t FakeESOut::esCount() const
{
    size_t i_count = 0;
    std::list<FakeESOutID *>::const_iterator it;
    for( it=fakeesidlist.begin(); it!=fakeesidlist.end(); ++it )
        if( (*it)->realESID() )
            i_count++;
    return i_count;
}

void FakeESOut::schedulePCRReset()
{
    AbstractCommand *command = commandsFactory->creatEsOutControlResetPCRCommand();
    if( likely(command) )
        commandsqueue.Schedule( command );
}

void FakeESOut::scheduleAllForDeletion()
{
    std::list<FakeESOutID *>::const_iterator it;
    for( it=fakeesidlist.begin(); it!=fakeesidlist.end(); ++it )
    {
        FakeESOutID *es_id = *it;
        if(!es_id->scheduledForDeletion())
        {
            AbstractCommand *command = commandsFactory->createEsOutDelCommand( es_id );
            if( likely(command) )
            {
                commandsqueue.Schedule( command );
                es_id->setScheduledForDeletion();
            }
        }
    }
}

void FakeESOut::recycleAll()
{
    /* Only used when demux is killed and commands queue is cancelled */
    commandsqueue.Abort( true );
    assert(commandsqueue.isEmpty());
    recycle_candidates.splice( recycle_candidates.end(), fakeesidlist );
}

void FakeESOut::gc()
{
    if( recycle_candidates.empty() )
        return;

    std::list<FakeESOutID *>::iterator it;
    for( it=recycle_candidates.begin(); it!=recycle_candidates.end(); ++it )
    {
        if( (*it)->realESID() )
        {
            es_out_Control( real_es_out, ES_OUT_SET_ES_STATE, (*it)->realESID(), false );
            es_out_Del( real_es_out, (*it)->realESID() );
        }
        delete *it;
    }
    recycle_candidates.clear();
}

bool FakeESOut::hasSelectedEs() const
{
    bool b_selected = false;
    std::list<FakeESOutID *>::const_iterator it;
    for( it=fakeesidlist.begin(); it!=fakeesidlist.end() && !b_selected; ++it )
    {
        FakeESOutID *esID = *it;
        if( esID->realESID() )
            es_out_Control( real_es_out, ES_OUT_GET_ES_STATE, esID->realESID(), &b_selected );
    }
    return b_selected;
}

bool FakeESOut::restarting() const
{
    return !recycle_candidates.empty();
}

void FakeESOut::recycle( FakeESOutID *id )
{
    fakeesidlist.remove( id );
    recycle_candidates.push_back( id );
}

/* Static callbacks */
/* Always pass Fake ES ID to slave demuxes, it is just an opaque struct to them */
es_out_id_t * FakeESOut::esOutAdd_Callback(es_out_t *fakees, const es_format_t *p_fmt)
{
    FakeESOut *me = (FakeESOut *) fakees->p_sys;
    /* Feed the slave demux/stream_Demux with FakeESOutID struct,
     * we'll create real ES later on main demux on execution */
    FakeESOutID *es_id = me->createNewID( p_fmt );
    if( likely(es_id) )
    {
        assert(!es_id->scheduledForDeletion());
        AbstractCommand *command = me->commandsFactory->createEsOutAddCommand( es_id );
        if( likely(command) )
        {
            me->commandsqueue.Schedule( command );
            return reinterpret_cast<es_out_id_t *>(es_id);
        }
        else
        {
            delete es_id;
        }
    }
    return NULL;
}

int FakeESOut::esOutSend_Callback(es_out_t *fakees, es_out_id_t *p_es, block_t *p_block)
{
    FakeESOut *me = (FakeESOut *) fakees->p_sys;
    FakeESOutID *es_id = reinterpret_cast<FakeESOutID *>( p_es );
    assert(!es_id->scheduledForDeletion());
    mtime_t offset = me->getTimestampOffset();
    if( p_block->i_dts > VLC_TS_INVALID )
    {
        p_block->i_dts += offset;
        if( p_block->i_pts > VLC_TS_INVALID )
                p_block->i_pts += offset;
    }
    AbstractCommand *command = me->commandsFactory->createEsOutSendCommand( es_id, p_block );
    if( likely(command) )
    {
        me->commandsqueue.Schedule( command );
        return VLC_SUCCESS;
    }
    return VLC_EGENERIC;
}

void FakeESOut::esOutDel_Callback(es_out_t *fakees, es_out_id_t *p_es)
{
    FakeESOut *me = (FakeESOut *) fakees->p_sys;
    FakeESOutID *es_id = reinterpret_cast<FakeESOutID *>( p_es );
    AbstractCommand *command = me->commandsFactory->createEsOutDelCommand( es_id );
    if( likely(command) )
    {
        me->commandsqueue.Schedule( command );
        es_id->setScheduledForDeletion();
    }
}

int FakeESOut::esOutControl_Callback(es_out_t *fakees, int i_query, va_list args)
{
    FakeESOut *me = (FakeESOut *) fakees->p_sys;

    switch( i_query )
    {
        case ES_OUT_SET_PCR:
        case ES_OUT_SET_GROUP_PCR:
        {
            int i_group;
            if( i_query == ES_OUT_SET_GROUP_PCR )
                i_group = static_cast<int>(va_arg( args, int ));
            else
                i_group = 0;
            int64_t  pcr = static_cast<int64_t>(va_arg( args, int64_t ));
            pcr += me->getTimestampOffset();
            AbstractCommand *command = me->commandsFactory->createEsOutControlPCRCommand( i_group, pcr );
            if( likely(command) )
            {
                me->commandsqueue.Schedule( command );
                return VLC_SUCCESS;
            }
        }
        break;

        /* For others, we don't have the delorean, so always lie */
        case ES_OUT_GET_ES_STATE:
        {
            static_cast<void*>(va_arg( args, es_out_id_t * ));
            bool *pb = static_cast<bool *>(va_arg( args, bool * ));
            *pb = true;
            // ft
        }
        case ES_OUT_SET_ES:
        case ES_OUT_SET_ES_DEFAULT:
        case ES_OUT_SET_ES_STATE:
            return VLC_SUCCESS;

    }

    return VLC_EGENERIC;
}

void FakeESOut::esOutDestroy_Callback(es_out_t *fakees)
{
    FakeESOut *me = (FakeESOut *) fakees->p_sys;
    AbstractCommand *command = me->commandsFactory->createEsOutDestroyCommand();
    if( likely(command) )
        me->commandsqueue.Schedule( command );
}
/* !Static callbacks */
