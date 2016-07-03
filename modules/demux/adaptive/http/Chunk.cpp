/*
 * Chunk.cpp
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

#include "Chunk.h"
#include "HTTPConnection.hpp"
#include "HTTPConnectionManager.h"
#include "Downloader.hpp"

#include <vlc_common.h>
#include <vlc_block.h>

#include <algorithm>

using namespace adaptive::http;

AbstractChunkSource::AbstractChunkSource()
{
    contentLength = 0;
}

AbstractChunkSource::~AbstractChunkSource()
{

}

void AbstractChunkSource::setBytesRange(const BytesRange &range)
{
    bytesRange = range;
    if(bytesRange.isValid() && bytesRange.getEndByte())
        contentLength = bytesRange.getEndByte() - bytesRange.getStartByte();
}

const BytesRange & AbstractChunkSource::getBytesRange() const
{
    return bytesRange;
}

AbstractChunk::AbstractChunk(AbstractChunkSource *source_)
{
    bytesRead = 0;
    source = source_;
}

AbstractChunk::~AbstractChunk()
{
    delete source;
}

size_t AbstractChunk::getBytesRead() const
{
    return this->bytesRead;
}

uint64_t AbstractChunk::getStartByteInFile() const
{
    if(!source || !source->getBytesRange().isValid())
        return 0;

    return source->getBytesRange().getStartByte();
}

block_t * AbstractChunk::doRead(size_t size, bool b_block)
{
    if(!source)
        return NULL;

    block_t *block = (b_block) ? source->readBlock() : source->read(size);
    if(block)
    {
        if(bytesRead == 0)
            block->i_flags |= BLOCK_FLAG_HEADER;
        bytesRead += block->i_buffer;
        onDownload(&block);
        block->i_flags ^= BLOCK_FLAG_HEADER;
    }

    return block;
}

bool AbstractChunk::isEmpty() const
{
    return !source->hasMoreData();
}

block_t * AbstractChunk::readBlock()
{
    return doRead(0, true);
}

block_t * AbstractChunk::read(size_t size)
{
    return doRead(size, false);
}

HTTPChunkSource::HTTPChunkSource(const std::string& url, HTTPConnectionManager *manager) :
    AbstractChunkSource(),
    connection   (NULL),
    connManager  (manager),
    consumed     (0)
{
    prepared = false;
    eof = false;
    if(!init(url))
        throw VLC_EGENERIC;
}

HTTPChunkSource::~HTTPChunkSource()
{
    if(connection)
        connection->setUsed(false);
}

bool HTTPChunkSource::init(const std::string &url)
{
    params = ConnectionParams(url);

    if(params.getScheme() != "http" && params.getScheme() != "https")
        return false;

    if(params.getPath().empty() || params.getHostname().empty())
        return false;

    return true;
}

bool HTTPChunkSource::hasMoreData() const
{
    if(eof)
        return false;
    else if(contentLength)
        return consumed < contentLength;
    else return true;
}

block_t * HTTPChunkSource::read(size_t readsize)
{
    if(!prepare())
    {
        eof = true;
        return NULL;
    }

    if(consumed == contentLength && consumed > 0)
    {
        eof = true;
        return NULL;
    }

    if(contentLength && readsize > contentLength - consumed)
        readsize = contentLength - consumed;

    block_t *p_block = block_Alloc(readsize);
    if(!p_block)
    {
        eof = true;
        return NULL;
    }

    mtime_t time = mdate();
    ssize_t ret = connection->read(p_block->p_buffer, readsize);
    time = mdate() - time;
    if(ret < 0)
    {
        block_Release(p_block);
        p_block = NULL;
        eof = true;
    }
    else
    {
        p_block->i_buffer = (size_t) ret;
        consumed += p_block->i_buffer;
        if((size_t)ret < readsize)
            eof = true;
        connManager->updateDownloadRate(p_block->i_buffer, time);
    }

    return p_block;
}

bool HTTPChunkSource::prepare()
{
    if(prepared)
        return true;

    if(!connManager)
        return false;

    if(!connection)
    {
        connection = connManager->getConnection(params);
        if(!connection)
            return false;
    }

    if( connection->request(params.getPath(), bytesRange) != VLC_SUCCESS )
        return false;
    /* Because we don't know Chunk size at start, we need to get size
           from content length */
    contentLength = connection->getContentLength();
    prepared = true;

    return true;
}

block_t * HTTPChunkSource::readBlock()
{
    return read(HTTPChunkSource::CHUNK_SIZE);
}

HTTPChunkBufferedSource::HTTPChunkBufferedSource(const std::string& url, HTTPConnectionManager *manager) :
    HTTPChunkSource(url, manager),
    p_head     (NULL),
    pp_tail    (&p_head),
    buffered     (0)
{
    vlc_mutex_init(&lock);
    vlc_cond_init(&avail);
    done = false;
    eof = false;
    downloadstart = 0;
}

HTTPChunkBufferedSource::~HTTPChunkBufferedSource()
{
    vlc_mutex_lock(&lock);
    if(p_head)
    {
        block_ChainRelease(p_head);
        p_head = NULL;
        pp_tail = &p_head;
    }
    done = true;
    buffered = 0;
    vlc_mutex_unlock(&lock);

    connManager->downloader->cancel(this);

    vlc_cond_destroy(&avail);
    vlc_mutex_destroy(&lock);
}

bool HTTPChunkBufferedSource::isDone() const
{
    bool b_done;
    vlc_mutex_lock(const_cast<vlc_mutex_t *>(&lock));
    b_done = done;
    vlc_mutex_unlock(const_cast<vlc_mutex_t *>(&lock));
    return b_done;
}

void HTTPChunkBufferedSource::bufferize(size_t readsize)
{
    vlc_mutex_lock(&lock);
    if(!prepare())
    {
        done = true;
        eof = true;
        vlc_cond_signal(&avail);
        vlc_mutex_unlock(&lock);
        return;
    }

    if(readsize < HTTPChunkSource::CHUNK_SIZE)
        readsize = HTTPChunkSource::CHUNK_SIZE;

    if(contentLength && readsize > contentLength - buffered)
        readsize = contentLength - buffered;

    vlc_mutex_unlock(&lock);

    block_t *p_block = block_Alloc(readsize);
    if(!p_block)
    {
        eof = true;
        return;
    }

    struct
    {
        size_t size;
        mtime_t time;
    } rate = {0,0};

    ssize_t ret = connection->read(p_block->p_buffer, readsize);
    if(ret <= 0)
    {
        block_Release(p_block);
        vlc_mutex_lock(&lock);
        done = true;
        rate.size = buffered + consumed;
        rate.time = mdate() - downloadstart;
        downloadstart = 0;
        vlc_mutex_unlock(&lock);
    }
    else
    {
        p_block->i_buffer = (size_t) ret;
        vlc_mutex_lock(&lock);
        buffered += p_block->i_buffer;
        block_ChainLastAppend(&pp_tail, p_block);
        if((size_t) ret < readsize)
        {
            done = true;
            rate.size = buffered + consumed;
            rate.time = mdate() - downloadstart;
            downloadstart = 0;
        }
        vlc_mutex_unlock(&lock);
    }

    if(rate.size)
    {
        connManager->updateDownloadRate(rate.size, rate.time);
    }

    vlc_cond_signal(&avail);
}

bool HTTPChunkBufferedSource::prepare()
{
    if(!prepared)
    {
        downloadstart = mdate();
        return HTTPChunkSource::prepare();
    }
    return true;
}

bool HTTPChunkBufferedSource::hasMoreData() const
{
    bool b_hasdata;
    vlc_mutex_lock(const_cast<vlc_mutex_t *>(&lock));
    b_hasdata = !eof;
    vlc_mutex_unlock(const_cast<vlc_mutex_t *>(&lock));
    return b_hasdata;
}

block_t * HTTPChunkBufferedSource::readBlock()
{
    block_t *p_block = NULL;

    vlc_mutex_lock(&lock);

    while(!p_head && !done)
        vlc_cond_wait(&avail, &lock);

    if(!p_head && done)
    {
        if(!eof)
            p_block = block_Alloc(0);
        eof = true;
        vlc_mutex_unlock(&lock);
        return p_block;
    }

    /* dequeue */
    p_block = p_head;
    p_head = p_head->p_next;
    if(p_head == NULL)
    {
        pp_tail = &p_head;
        if(done)
            eof = true;
    }
    p_block->p_next = NULL;

    consumed += p_block->i_buffer;
    buffered -= p_block->i_buffer;

    vlc_mutex_unlock(&lock);

    return p_block;
}

block_t * HTTPChunkBufferedSource::read(size_t readsize)
{
    vlc_mutex_lock(&lock);

    while(readsize > buffered && !done)
        vlc_cond_wait(&avail, &lock);

    block_t *p_block = NULL;
    if(!readsize || !buffered || !(p_block = block_Alloc(readsize)) )
    {
        eof = true;
        vlc_mutex_unlock(&lock);
        return NULL;
    }

    size_t copied = 0;
    while(buffered && readsize)
    {
        const size_t toconsume = std::min(p_head->i_buffer, readsize);
        memcpy(&p_block->p_buffer[copied], p_head->p_buffer, toconsume);
        copied += toconsume;
        readsize -= toconsume;
        buffered -= toconsume;
        p_head->i_buffer -= toconsume;
        p_head->p_buffer += toconsume;
        if(p_head->i_buffer == 0)
        {
            block_t *next = p_head->p_next;
            p_head->p_next = NULL;
            block_Release(p_head);
            p_head = next;
            if(next == NULL)
                pp_tail = &p_head;
        }
    }

    consumed += copied;
    p_block->i_buffer = copied;

    if(copied < readsize)
        eof = true;

    vlc_mutex_unlock(&lock);

    return p_block;
}

HTTPChunk::HTTPChunk(const std::string &url, HTTPConnectionManager *manager):
    AbstractChunk(new HTTPChunkSource(url, manager))
{

}

HTTPChunk::~HTTPChunk()
{

}
