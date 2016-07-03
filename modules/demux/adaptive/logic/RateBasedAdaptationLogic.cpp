/*
 * RateBasedAdaptationLogic.cpp
 *****************************************************************************
 * Copyright (C) 2010 - 2011 Klagenfurt University
 *
 * Created on: Aug 10, 2010
 * Authors: Christopher Mueller <christopher.mueller@itec.uni-klu.ac.at>
 *          Christian Timmerer  <christian.timmerer@itec.uni-klu.ac.at>
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

#include "RateBasedAdaptationLogic.h"
#include "Representationselectors.hpp"

#include "../playlist/BaseRepresentation.h"
#include "../playlist/BasePeriod.h"
#include "../http/Chunk.h"
#include "../tools/Debug.hpp"

using namespace adaptive::logic;
using namespace adaptive;

RateBasedAdaptationLogic::RateBasedAdaptationLogic  (vlc_object_t *p_obj_, int w, int h) :
                          AbstractAdaptationLogic   (),
                          bpsAvg(0),
                          currentBps(0)
{
    width  = w;
    height = h;
    usedBps = 0;
    dllength = 0;
    p_obj = p_obj_;
    for(unsigned i=0; i<10; i++) window[i].bw = window[i].diff = 0;
    window_idx = 0;
    prevbps = 0;
    dlsize = 0;
    vlc_mutex_init(&lock);
}

RateBasedAdaptationLogic::~RateBasedAdaptationLogic()
{
    vlc_mutex_destroy(&lock);
}

BaseRepresentation *RateBasedAdaptationLogic::getNextRepresentation(BaseAdaptationSet *adaptSet, BaseRepresentation *currep) const
{
    if(adaptSet == NULL)
        return NULL;

    vlc_mutex_lock(const_cast<vlc_mutex_t *>(&lock));
    size_t availBps = currentBps + ((currep) ? currep->getBandwidth() : 0);
    vlc_mutex_unlock(const_cast<vlc_mutex_t *>(&lock));
    if(availBps > usedBps)
        availBps -= usedBps;
    else
        availBps = 0;

    RepresentationSelector selector;
    BaseRepresentation *rep = selector.select(adaptSet, availBps, width, height);
    if ( rep == NULL )
    {
        rep = selector.select(adaptSet);
        if ( rep == NULL )
            return NULL;
    }

    return rep;
}

void RateBasedAdaptationLogic::updateDownloadRate(size_t size, mtime_t time)
{
    if(unlikely(time == 0))
        return;
    /* Accumulate up to observation window */
    dllength += time;
    dlsize += size;

    if(dllength < CLOCK_FREQ / 4)
        return;

    const size_t bps = CLOCK_FREQ * dlsize * 8 / dllength;

    /* set window value */
    if(window[0].bw == 0)
    {
        for(unsigned i=0; i<TOTALOBS; i++) window[i].bw = bps;
    }
    else
    {
        window_idx = (window_idx + 1) % TOTALOBS;
        window[window_idx].bw = bps;
        window[window_idx].diff = bps >= prevbps ? bps - prevbps : prevbps - bps;
    }

    /* compute for deltamax */
    size_t diffsum = 0;
    size_t omin = SIZE_MAX;
    size_t omax = 0;
    for(unsigned i=0; i < TOTALOBS; i++)
    {
        /* Find max and min */
        if(window[i].bw > omax)
            omax = window[i].bw;
        if(window[i].bw < omin)
            omin = window[i].bw;
        diffsum += window[i].diff;
    }

    /* Vertical Horizontal Filter / Moving Average
     *
     * Bandwidth stability during observation window alters the alpha parameter
     * and then defines how fast we adapt to current bandwidth */
    const size_t deltamax = omax - omin;
    double alpha = (diffsum) ? 0.33 * ((double)deltamax / diffsum) : 0.5;

    vlc_mutex_lock(&lock);
    bpsAvg = alpha * bpsAvg + (1.0 - alpha) * bps;

    BwDebug(msg_Dbg(p_obj, "alpha1 %lf alpha0 %lf dmax %ld ds %ld", alpha,
                    (double)deltamax / diffsum, deltamax, diffsum));
    BwDebug(msg_Dbg(p_obj, "bw estimation bps %zu -> avg %zu",
                            bps / 8192, bpsAvg / 8192));

    currentBps = bpsAvg * 3/4;
    dlsize = dllength = 0;
    prevbps = bps;

    BwDebug(msg_Info(p_obj, "Current bandwidth %zu KiB/s using %u%%",
                    (bpsAvg / 8192), (bpsAvg) ? (unsigned)(usedBps * 100.0 / bpsAvg) : 0));
    vlc_mutex_unlock(&lock);
}

void RateBasedAdaptationLogic::trackerEvent(const SegmentTrackerEvent &event)
{
    if(event.type == SegmentTrackerEvent::SWITCHING)
    {
        vlc_mutex_lock(&lock);
        if(event.u.switching.prev)
            usedBps -= event.u.switching.prev->getBandwidth();
        if(event.u.switching.next)
            usedBps += event.u.switching.next->getBandwidth();

        BwDebug(msg_Info(p_obj, "New bandwidth usage %zu KiB/s %u%%",
                        (usedBps / 8192), (bpsAvg) ? (unsigned)(usedBps * 100.0 / bpsAvg) : 0 ));
        vlc_mutex_unlock(&lock);
    }
}

FixedRateAdaptationLogic::FixedRateAdaptationLogic(size_t bps) :
    AbstractAdaptationLogic()
{
    currentBps = bps;
}

BaseRepresentation *FixedRateAdaptationLogic::getNextRepresentation(BaseAdaptationSet *adaptSet, BaseRepresentation *) const
{
    if(adaptSet == NULL)
        return NULL;

    RepresentationSelector selector;
    BaseRepresentation *rep = selector.select(adaptSet, currentBps);
    if ( rep == NULL )
    {
        rep = selector.select(adaptSet);
        if ( rep == NULL )
            return NULL;
    }
    return rep;
}
