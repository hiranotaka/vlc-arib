/*
 * HTTPConnectionManager.h
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

#ifndef HTTPCONNECTIONMANAGER_H_
#define HTTPCONNECTIONMANAGER_H_

#include "../logic/IDownloadRateObserver.h"

#include <vlc_common.h>
#include <vector>
#include <string>

namespace adaptive
{
    namespace http
    {
        class ConnectionParams;
        class ConnectionFactory;
        class AbstractConnection;
        class Downloader;

        class HTTPConnectionManager : public IDownloadRateObserver
        {
            public:
                HTTPConnectionManager           (vlc_object_t *p_object, ConnectionFactory * = NULL);
                virtual ~HTTPConnectionManager  ();

                void    closeAllConnections ();
                AbstractConnection * getConnection(ConnectionParams &);

                virtual void updateDownloadRate(size_t, mtime_t); /* reimpl */
                void setDownloadRateObserver(IDownloadRateObserver *);
                Downloader *downloader;

            private:
                void    releaseAllConnections ();
                vlc_mutex_t                                         lock;
                std::vector<AbstractConnection *>                   connectionPool;
                vlc_object_t                                       *p_object;
                IDownloadRateObserver                              *rateObserver;
                ConnectionFactory                                  *factory;
                AbstractConnection * reuseConnection(ConnectionParams &);
        };
    }
}

#endif /* HTTPCONNECTIONMANAGER_H_ */
