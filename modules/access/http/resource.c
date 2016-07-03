/*****************************************************************************
 * resource.c: HTTP resource common code
 *****************************************************************************
 * Copyright (C) 2015 Rémi Denis-Courmont
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vlc_common.h>
#include <vlc_url.h>
#include <vlc_strings.h>
#include "message.h"
#include "connmgr.h"
#include "resource.h"

static struct vlc_http_msg *
vlc_http_res_req(const struct vlc_http_resource *res)
{
    struct vlc_http_msg *req;

    req = vlc_http_req_create("GET", res->secure ? "https" : "http",
                              res->authority, res->path);
    if (unlikely(req == NULL))
        return NULL;

    /* Content negotiation */
    vlc_http_msg_add_header(req, "Accept", "*/*");

    if (res->negotiate)
    {
        const char *lang = vlc_gettext("C");
        if (!strcmp(lang, "C"))
            lang = "en_US";
        vlc_http_msg_add_header(req, "Accept-Language", "%s", lang);
    }

    /* Authentication */
    /* TODO: authentication */

    /* Request context */
    if (res->agent != NULL)
        vlc_http_msg_add_agent(req, res->agent);

    if (res->referrer != NULL) /* TODO: validate URL */
        vlc_http_msg_add_header(req, "Referer", "%s", res->referrer);

    vlc_http_msg_add_cookies(req, vlc_http_mgr_get_jar(res->manager));

    /* TODO: vlc_http_msg_add_header(req, "TE", "gzip, deflate"); */

    return req;
}

struct vlc_http_msg *vlc_http_res_open(struct vlc_http_resource *res,
    int (*request_cb)(struct vlc_http_msg *, const struct vlc_http_resource *,
                      void *), void *opaque)
{
    struct vlc_http_msg *req;
retry:
    req = vlc_http_res_req(res);
    if (unlikely(req == NULL))
        return NULL;

    if (request_cb(req, res, opaque))
    {
        vlc_http_msg_destroy(req);
        return NULL;
    }

    struct vlc_http_msg *resp = vlc_http_mgr_request(res->manager, res->secure,
                                                    res->host, res->port, req);
    vlc_http_msg_destroy(req);

    resp = vlc_http_msg_get_final(resp);
    if (resp == NULL)
        return NULL;

    vlc_http_msg_get_cookies(resp, vlc_http_mgr_get_jar(res->manager),
                             res->secure, res->host, res->path);

    int status = vlc_http_msg_get_status(resp);
    if (status < 200 || status >= 599)
        goto fail;

    if (status == 406 && res->negotiate)
    {   /* Not Acceptable: Content negotiation failed. Normally it means
         * one (or more) Accept or Accept-* header line does not match any
         * representation of the entity. So we set a flag to remove those
         * header lines (unless they accept everything), and retry.
         * In principles, it could be any header line, and the server can
         * pass Vary to clarify. It cannot be caused by If-*, Range, TE or the
         * other transfer- rather than representation-affecting header lines.
         */
        vlc_http_msg_destroy(resp);
        res->negotiate = false;
        goto retry;
    }

    return resp;
fail:
    vlc_http_msg_destroy(resp);
    return NULL;
}

void vlc_http_res_deinit(struct vlc_http_resource *res)
{
    free(res->referrer);
    free(res->agent);
    free(res->path);
    free(res->authority);
    free(res->host);
}

static char *vlc_http_authority(const char *host, unsigned port)
{
    static const char *const formats[4] = { "%s", "[%s]", "%s:%u", "[%s]:%u" };
    const bool brackets = strchr(host, ':') != NULL;
    const char *fmt = formats[brackets + 2 * (port != 0)];
    char *authority;

    if (unlikely(asprintf(&authority, fmt, host, port) == -1))
        return NULL;
    return authority;
}

int vlc_http_res_init(struct vlc_http_resource *restrict res,
                      struct vlc_http_mgr *mgr, const char *uri,
                      const char *ua, const char *ref)
{
    vlc_url_t url;
    bool secure;

    vlc_UrlParse(&url, uri);
    if (url.psz_protocol == NULL || url.psz_host == NULL)
    {
        errno = EINVAL;
        goto error;
    }

    if (!vlc_ascii_strcasecmp(url.psz_protocol, "https"))
        secure = true;
    else if (!vlc_ascii_strcasecmp(url.psz_protocol, "http"))
        secure = false;
    else
    {
        errno = ENOTSUP;
        goto error;
    }

    res->secure = secure;
    res->negotiate = true;
    res->host = strdup(url.psz_host);
    res->port = url.i_port;
    res->authority = vlc_http_authority(url.psz_host, url.i_port);
    res->agent = (ua != NULL) ? strdup(ua) : NULL;
    res->referrer = (ref != NULL) ? strdup(ref) : NULL;

    const char *path = url.psz_path;
    if (path == NULL)
        path = "/";

    if (url.psz_option != NULL)
    {
        if (asprintf(&res->path, "%s?%s", path, url.psz_option) == -1)
            res->path = NULL;
    }
    else
        res->path = strdup(path);

    vlc_UrlClean(&url);
    res->manager = mgr;

    if (unlikely(res->host == NULL || res->authority == NULL
              || res->path == NULL))
    {
        vlc_http_res_deinit(res);
        return -1;
    }
    return 0;
error:
    vlc_UrlClean(&url);
    return -1;
}

char *vlc_http_res_get_redirect(const struct vlc_http_resource *restrict res,
                                const struct vlc_http_msg *resp)
{
    int status = vlc_http_msg_get_status(resp);

    if ((status / 100) == 2 && !res->secure)
    {
        char *url;

        /* HACK: Seems like an MMS server. Redirect to MMSH scheme. */
        if (vlc_http_msg_get_token(resp, "Pragma", "features") != NULL
         && asprintf(&url, "mmsh://%s%s", res->authority, res->path) >= 0)
            return url;

        /* HACK: Seems like an ICY server. Redirect to ICYX scheme. */
        if ((vlc_http_msg_get_header(resp, "Icy-Name") != NULL
          || vlc_http_msg_get_header(resp, "Icy-Genre") != NULL)
         && asprintf(&url, "icyx://%s%s", res->authority, res->path) >= 0)
            return url;
    }

    /* TODO: if (status == 426 Upgrade Required) */

    /* Location header is only meaningful for 201 and 3xx */
    if (status != 201 && (status / 100) != 3)
        return NULL;
    if (status == 304 /* Not Modified */
     || status == 305 /* Use Proxy (deprecated) */
     || status == 306 /* Switch Proxy (former) */)
        return NULL;

    const char *location = vlc_http_msg_get_header(resp, "Location");
    if (location == NULL)
        return NULL;

    /* TODO: if status is 3xx, check for Retry-After and wait */

    /* NOTE: The anchor is discard if it is present as VLC does not support
     * HTML anchors so far. */
    size_t len = strcspn(location, "#");

    /* FIXME: resolve relative URL _correctly_ */
    if (location[0] == '/')
    {
        char *url;

        if (unlikely(asprintf(&url, "%s://%s%.*s",
                              res->secure ? "https" : "http", res->authority,
                              (int)len, location) < 0))
            return NULL;
        return url;
    }
    return strndup(location, len);
}

char *vlc_http_res_get_type(const struct vlc_http_msg *resp)
{
    int status = vlc_http_msg_get_status(resp);
    if (status < 200 || status >= 300)
        return NULL;

    const char *type = vlc_http_msg_get_header(resp, "Content-Type");
    return (type != NULL) ? strdup(type) : NULL;
}

struct block_t *vlc_http_res_read(struct vlc_http_msg *resp)
{
    int status = vlc_http_msg_get_status(resp);
    if (status < 200 || status >= 300)
        return NULL; /* do not "read" redirect or error message */

    return vlc_http_msg_read(resp);
}
