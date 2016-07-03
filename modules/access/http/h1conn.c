/*****************************************************************************
 * h1conn.c: HTTP 1.x connection handling
 *****************************************************************************
 * Copyright © 2015 Rémi Denis-Courmont
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
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
# include <config.h>
#endif

#include <assert.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vlc_common.h>
#include <vlc_tls.h>
#include <vlc_block.h>

#include "conn.h"
#include "message.h"

static unsigned vlc_http_can_read(const char *buf, size_t len)
{
    static const char end[4] = { '\r', '\n', '\r', '\n' };

    for (unsigned i = 4; i > 0; i--)
        if (len >= i && !memcmp(buf + len - i, end, i))
            return 4 - i;
    return 4;
}

/**
 * Receives HTTP headers.
 *
 * Receives HTTP 1.x response line and headers.
 *
 * @return A heap-allocated null-terminated string contained the full
 * headers, including the final CRLF.
 */
static char *vlc_https_headers_recv(struct vlc_tls *tls, size_t *restrict lenp)
{
    size_t size = 0, len = 0, canread;
    char *buf = NULL;

    while ((canread = vlc_http_can_read(buf, len)) > 0)
    {
        ssize_t val;

        if (len + canread >= size)
        {
            size += 2048;
            if (size > 65536)
                goto fail;

            char *newbuf = realloc(buf, size);
            if (unlikely(newbuf == NULL))
                goto fail;

            buf = newbuf;
        }

        assert(size - len >= canread);
        //vlc_cleanup_push(free, buf);
        val = vlc_tls_Read(tls, buf + len, canread, true);
        //vlc_cleanup_pop();

        if (val != (ssize_t)canread)
            goto fail;

        len += val;
    }

    assert(size - len >= 1);
    buf[len] = '\0'; /* for convenience */
    if (lenp != NULL)
        *lenp = len;
    return buf;
fail:
    free(buf);
    return NULL;
}

/** Gets minor HTTP version */
static int vlc_http_minor(const char *msg)
{
    int minor;

    if (sscanf(msg, "HTTP/1.%1d", &minor) == 1)
        return minor;
    return -1;
}

struct vlc_h1_conn
{
    struct vlc_http_conn conn;
    struct vlc_http_stream stream;
    uintmax_t content_length;
    bool connection_close;
    bool active;
    bool released;
    bool proxy;
};

#define CO(conn) ((conn)->conn.tls->obj)

static void vlc_h1_conn_destroy(struct vlc_h1_conn *conn);

static void *vlc_h1_stream_fatal(struct vlc_h1_conn *conn)
{
    if (conn->conn.tls != NULL)
    {
        msg_Dbg(CO(conn), "connection failed");
        vlc_tls_Shutdown(conn->conn.tls, true);
        vlc_tls_Close(conn->conn.tls);
        conn->conn.tls = NULL;
    }
    return NULL;
}

static_assert(offsetof(struct vlc_h1_conn, conn) == 0, "Cast error");

static struct vlc_h1_conn *vlc_h1_stream_conn(struct vlc_http_stream *stream)
{
    return (void *)(((char *)stream) - offsetof(struct vlc_h1_conn, stream));
}

static struct vlc_http_stream *vlc_h1_stream_open(struct vlc_http_conn *c,
                                                const struct vlc_http_msg *req)
{
    struct vlc_h1_conn *conn = (struct vlc_h1_conn *)c;
    size_t len;
    ssize_t val;

    if (conn->active || conn->conn.tls == NULL)
        return NULL;

    char *payload = vlc_http_msg_format(req, &len, conn->proxy);
    if (unlikely(payload == NULL))
        return NULL;

    msg_Dbg(CO(conn), "outgoing request:\n%.*s", (int)len, payload);
    val = vlc_tls_Write(conn->conn.tls, payload, len);
    free(payload);

    if (val < (ssize_t)len)
        return vlc_h1_stream_fatal(conn);

    conn->active = true;
    conn->content_length = 0;
    conn->connection_close = false;
    return &conn->stream;
}

static struct vlc_http_msg *vlc_h1_stream_wait(struct vlc_http_stream *stream)
{
    struct vlc_h1_conn *conn = vlc_h1_stream_conn(stream);
    struct vlc_http_msg *resp;
    const char *str;
    size_t len;
    int minor;

    assert(conn->active);

    if (conn->conn.tls == NULL)
        return NULL;

    char *payload = vlc_https_headers_recv(conn->conn.tls, &len);
    if (payload == NULL)
        return vlc_h1_stream_fatal(conn);

    msg_Dbg(CO(conn), "incoming response:\n%.*s", (int)len, payload);

    resp = vlc_http_msg_headers(payload);
    minor = vlc_http_minor(payload);
    free(payload);

    if (resp == NULL)
        return vlc_h1_stream_fatal(conn);

    assert(minor >= 0);

    conn->content_length = vlc_http_msg_get_size(resp);
    conn->connection_close = false;

    if (minor >= 1)
    {
        if (vlc_http_msg_get_token(resp, "Connection", "close") != NULL)
            conn->connection_close = true;

        str = vlc_http_msg_get_token(resp, "Transfer-Encoding", "chunked");
        if (str != NULL)
        {
            if (vlc_http_next_token(str) != NULL)
            {
                vlc_http_msg_destroy(resp);
                return vlc_h1_stream_fatal(conn); /* unsupported TE */
            }

            assert(conn->content_length == UINTMAX_MAX);
            stream = vlc_chunked_open(stream, conn->conn.tls);
            if (unlikely(stream == NULL))
            {
                vlc_http_msg_destroy(resp);
                return vlc_h1_stream_fatal(conn);
            }
        }
    }
    else
        conn->connection_close = true;

    vlc_http_msg_attach(resp, stream);
    return resp;
}

static block_t *vlc_h1_stream_read(struct vlc_http_stream *stream)
{
    struct vlc_h1_conn *conn = vlc_h1_stream_conn(stream);
    size_t size = 2048;

    assert(conn->active);

    if (conn->conn.tls == NULL)
        return NULL;

    if (size > conn->content_length)
        size = conn->content_length;
    if (size == 0)
        return NULL;

    block_t *block = block_Alloc(size);
    if (unlikely(block == NULL))
        return NULL;

    ssize_t val = vlc_tls_Read(conn->conn.tls, block->p_buffer, size, false);
    if (val <= 0)
    {
        block_Release(block);
        return NULL;
    }

    block->i_buffer = val;
    if (conn->content_length != UINTMAX_MAX)
        conn->content_length -= val;

    return block;
}

static void vlc_h1_stream_close(struct vlc_http_stream *stream, bool abort)
{
    struct vlc_h1_conn *conn = vlc_h1_stream_conn(stream);

    assert(conn->active);

    if (abort)
        vlc_h1_stream_fatal(conn);

    conn->active = false;

    if (conn->released)
        vlc_h1_conn_destroy(conn);
}

static const struct vlc_http_stream_cbs vlc_h1_stream_callbacks =
{
    vlc_h1_stream_wait,
    vlc_h1_stream_read,
    vlc_h1_stream_close,
};

static void vlc_h1_conn_destroy(struct vlc_h1_conn *conn)
{
    assert(!conn->active);
    assert(conn->released);

    if (conn->conn.tls != NULL)
    {
        vlc_tls_Shutdown(conn->conn.tls, true);
        vlc_tls_Close(conn->conn.tls);
    }
    free(conn);
}

static void vlc_h1_conn_release(struct vlc_http_conn *c)
{
    struct vlc_h1_conn *conn = (struct vlc_h1_conn *)c;

    assert(!conn->released);
    conn->released = true;

    if (!conn->active)
        vlc_h1_conn_destroy(conn);
}

static const struct vlc_http_conn_cbs vlc_h1_conn_callbacks =
{
    vlc_h1_stream_open,
    vlc_h1_conn_release,
};

struct vlc_http_conn *vlc_h1_conn_create(vlc_tls_t *tls, bool proxy)
{
    struct vlc_h1_conn *conn = malloc(sizeof (*conn));
    if (unlikely(conn == NULL))
        return NULL;

    conn->conn.cbs = &vlc_h1_conn_callbacks;
    conn->conn.tls = tls;
    conn->stream.cbs = &vlc_h1_stream_callbacks;
    conn->active = false;
    conn->released = false;
    conn->proxy = proxy;

    return &conn->conn;
}
