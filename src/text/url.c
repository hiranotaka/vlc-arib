/*****************************************************************************
 * url.c: URL related functions
 *****************************************************************************
 * Copyright (C) 2006 VLC authors and VideoLAN
 * Copyright (C) 2008-2012 Rémi Denis-Courmont
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
# include "config.h"
#endif

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#ifdef _WIN32
# include <io.h>
#endif

#include <vlc_common.h>
#include <vlc_url.h>
#include <vlc_fs.h>
#include <ctype.h>

char *vlc_uri_decode_duplicate (const char *str)
{
    char *buf = strdup (str);
    if (vlc_uri_decode (buf) == NULL)
    {
        free (buf);
        buf = NULL;
    }
    return buf;
}

char *vlc_uri_decode (char *str)
{
    char *in = str, *out = str;
    if (in == NULL)
        return NULL;

    char c;
    while ((c = *(in++)) != '\0')
    {
        if (c == '%')
        {
            char hex[3];

            if (!(hex[0] = *(in++)) || !(hex[1] = *(in++)))
                return NULL;
            hex[2] = '\0';
            *(out++) = strtoul (hex, NULL, 0x10);
        }
        else
            *(out++) = c;
    }
    *out = '\0';
    return str;
}

static bool isurisafe (int c)
{
    /* These are the _unreserved_ URI characters (RFC3986 §2.3) */
    return ((unsigned char)(c - 'a') < 26)
        || ((unsigned char)(c - 'A') < 26)
        || ((unsigned char)(c - '0') < 10)
        || (strchr ("-._~", c) != NULL);
}

static bool isurisubdelim(int c)
{
    return strchr("!$&'()*+,;=", c) != NULL;
}

static bool isurihex(int c)
{   /* Same as isxdigit() but does not depend on locale and unsignedness */
    return ((unsigned char)(c - '0') < 10)
        || ((unsigned char)(c - 'A') < 6)
        || ((unsigned char)(c - 'a') < 6);
}

static char *encode_URI_bytes (const char *str, size_t *restrict lenp)
{
    char *buf = malloc (3 * *lenp + 1);
    if (unlikely(buf == NULL))
        return NULL;

    char *out = buf;
    for (size_t i = 0; i < *lenp; i++)
    {
        static const char hex[] = "0123456789ABCDEF";
        unsigned char c = str[i];

        if (isurisafe (c))
            *(out++) = c;
        /* This is URI encoding, not HTTP forms:
         * Space is encoded as '%20', not '+'. */
        else
        {
            *(out++) = '%';
            *(out++) = hex[c >> 4];
            *(out++) = hex[c & 0xf];
        }
    }

    *lenp = out - buf;
    out = realloc (buf, *lenp + 1);
    return likely(out != NULL) ? out : buf;
}

char *vlc_uri_encode (const char *str)
{
    size_t len = strlen (str);
    char *ret = encode_URI_bytes (str, &len);
    if (likely(ret != NULL))
        ret[len] = '\0';
    return ret;
}

char *vlc_path2uri (const char *path, const char *scheme)
{
    if (path == NULL)
    {
        errno = EINVAL;
        return NULL;
    }
    if (scheme == NULL && !strcmp (path, "-"))
        return strdup ("fd://0"); // standard input
    /* Note: VLC cannot handle URI schemes without double slash after the
     * scheme name (such as mailto: or news:). */

    char *buf;

#ifdef __OS2__
    char p[strlen (path) + 1];

    for (buf = p; *path; buf++, path++)
        *buf = (*path == '/') ? DIR_SEP_CHAR : *path;
    *buf = '\0';

    path = p;
#endif

#if defined (_WIN32) || defined (__OS2__)
    /* Drive letter */
    if (isalpha ((unsigned char)path[0]) && (path[1] == ':'))
    {
        if (asprintf (&buf, "%s:///%c:", scheme ? scheme : "file",
                      path[0]) == -1)
            buf = NULL;
        path += 2;
# warning Drive letter-relative path not implemented!
        if (path[0] != DIR_SEP_CHAR)
        {
            errno = ENOTSUP;
            return NULL;
        }
    }
    else
    if (!strncmp (path, "\\\\", 2))
    {   /* Windows UNC paths */
        /* \\host\share\path -> file://host/share/path */
        int hostlen = strcspn (path + 2, DIR_SEP);

        if (asprintf (&buf, "file://%.*s", hostlen, path + 2) == -1)
            buf = NULL;
        path += 2 + hostlen;

        if (path[0] == '\0')
            return buf; /* Hostname without path */
    }
    else
#endif
    if (path[0] != DIR_SEP_CHAR)
    {   /* Relative path: prepend the current working directory */
        char *cwd, *ret;

        if ((cwd = vlc_getcwd ()) == NULL)
            return NULL;
        if (asprintf (&buf, "%s"DIR_SEP"%s", cwd, path) == -1)
            buf = NULL;

        free (cwd);
        ret = (buf != NULL) ? vlc_path2uri (buf, scheme) : NULL;
        free (buf);
        return ret;
    }
    else
    if (asprintf (&buf, "%s://", scheme ? scheme : "file") == -1)
        buf = NULL;
    if (buf == NULL)
        return NULL;

    /* Absolute file path */
    assert (path[0] == DIR_SEP_CHAR);
    do
    {
        size_t len = strcspn (++path, DIR_SEP);
        path += len;

        char *component = encode_URI_bytes (path - len, &len);
        if (unlikely(component == NULL))
        {
            free (buf);
            return NULL;
        }
        component[len] = '\0';

        char *uri;
        int val = asprintf (&uri, "%s/%s", buf, component);
        free (component);
        free (buf);
        if (unlikely(val == -1))
            return NULL;
        buf = uri;
    }
    while (*path);

    return buf;
}

char *vlc_uri2path (const char *url)
{
    char *ret = NULL;
    char *end;

    char *path = strstr (url, "://");
    if (path == NULL)
        return NULL; /* unsupported scheme or invalid syntax */

    end = memchr (url, '/', path - url);
    size_t schemelen = ((end != NULL) ? end : path) - url;
    path += 3; /* skip "://" */

    /* Remove request parameters and/or HTML anchor if present */
    end = path + strcspn (path, "?#");
    path = strndup (path, end - path);
    if (unlikely(path == NULL))
        return NULL; /* boom! */

    /* Decode path */
    vlc_uri_decode (path);

    if (schemelen == 4 && !strncasecmp (url, "file", 4))
    {
#if !defined (_WIN32) && !defined (__OS2__)
        /* Leading slash => local path */
        if (*path == '/')
            return path;
        /* Local path disguised as a remote one */
        if (!strncasecmp (path, "localhost/", 10))
            return memmove (path, path + 9, strlen (path + 9) + 1);
#else
        /* cannot start with a space */
        if (*path == ' ')
            goto out;
        for (char *p = strchr (path, '/'); p; p = strchr (p + 1, '/'))
            *p = '\\';

        /* Leading backslash => local path */
        if (*path == '\\')
            return memmove (path, path + 1, strlen (path + 1) + 1);
        /* Local path disguised as a remote one */
        if (!strncasecmp (path, "localhost\\", 10))
            return memmove (path, path + 10, strlen (path + 10) + 1);
        /* UNC path */
        if (*path && asprintf (&ret, "\\\\%s", path) == -1)
            ret = NULL;
#endif
        /* non-local path :-( */
    }
    else
    if (schemelen == 2 && !strncasecmp (url, "fd", 2))
    {
        int fd = strtol (path, &end, 0);

        if (*end)
            goto out;

#if !defined( _WIN32 ) && !defined( __OS2__ )
        switch (fd)
        {
            case 0:
                ret = strdup ("/dev/stdin");
                break;
            case 1:
                ret = strdup ("/dev/stdout");
                break;
            case 2:
                ret = strdup ("/dev/stderr");
                break;
            default:
                if (asprintf (&ret, "/dev/fd/%d", fd) == -1)
                    ret = NULL;
        }
#else
        /* XXX: Does this work on WinCE? */
        if (fd < 2)
            ret = strdup ("CON");
#endif
    }

out:
    free (path);
    return ret; /* unknown scheme */
}

static char *vlc_idna_to_ascii (const char *);

static bool vlc_uri_component_validate(const char *str, const char *extras)
{
    if (str == NULL)
        return false;

    for (size_t i = 0; str[i] != '\0'; i++)
    {
        int c = str[i];

        if (isurisafe(c) || isurisubdelim(c))
            continue;
        if (strchr(extras, c) != NULL)
            continue;
        if (c == '%' && isurihex(str[i + 1]) && isurihex(str[i + 2]))
        {
            i += 2;
            continue;
        }
        return false;
    }
    return true;
}

static bool vlc_uri_host_validate(const char *str)
{
    return vlc_uri_component_validate(str, ":");
}

static bool vlc_uri_path_validate(const char *str)
{
    return vlc_uri_component_validate(str, "/@:");
}

/**
 * Splits an URL into parts.
 * \param url structure of URL parts [OUT]
 * \param str nul-terminated URL string to split
 * \note Use vlc_UrlClean() to free associated resources
 * \bug Errors cannot be detected.
 * \return nothing
 */
void vlc_UrlParse (vlc_url_t *restrict url, const char *str)
{
    url->psz_protocol = NULL;
    url->psz_username = NULL;
    url->psz_password = NULL;
    url->psz_host = NULL;
    url->i_port = 0;
    url->psz_path = NULL;
    url->psz_option = NULL;
    url->psz_buffer = NULL;

    if (str == NULL)
        return;

    char *buf = strdup (str);
    if (unlikely(buf == NULL))
        abort ();
    url->psz_buffer = buf;

    char *cur = buf, *next;

    /* URL scheme */
    next = buf;
    while ((*next >= 'A' && *next <= 'Z') || (*next >= 'a' && *next <= 'z')
        || (*next >= '0' && *next <= '9') || memchr ("+-.", *next, 3) != NULL)
        next++;
    /* This is not strictly correct. In principles, the scheme is always
     * present in an absolute URL and followed by a colon. Depending on the
     * URL scheme, the two subsequent slashes are not required.
     * VLC uses a different scheme for historical compatibility reasons - the
     * scheme is often implicit. */
    if (!strncmp (next, "://", 3))
    {
        *next = '\0';
        next += 3;
        url->psz_protocol = cur;
        cur = next;
    }

    /* Query parameters */
    char *query = strchr (cur, '?');
    if (query != NULL)
    {
        *(query++) = '\0';
        url->psz_option = query;
    }

    /* Path */
    next = strchr (cur, '/');
    if (next != NULL)
    {
        *next = '\0'; /* temporary nul, reset to slash later */
        url->psz_path = next;
    }
    /*else
        url->psz_path = "/";*/

    /* User name */
    next = strrchr (cur, '@');
    if (next != NULL)
    {
        *(next++) = '\0';
        url->psz_username = cur;
        cur = next;

        /* Password (obsolete) */
        next = strchr (url->psz_username, ':');
        if (next != NULL)
        {
            *(next++) = '\0';
            url->psz_password = next;
            vlc_uri_decode (url->psz_password);
        }
        vlc_uri_decode (url->psz_username);
    }

    /* Host name */
    if (*cur == '[' && (next = strrchr (cur, ']')) != NULL)
    {   /* Try IPv6 numeral within brackets */
        *(next++) = '\0';
        url->psz_host = strdup (cur + 1);

        if (*next == ':')
            next++;
        else
            next = NULL;
    }
    else
    {
        next = strchr (cur, ':');
        if (next != NULL)
            *(next++) = '\0';

        url->psz_host = vlc_idna_to_ascii (cur);
    }
    if (!vlc_uri_host_validate(url->psz_host))
    {
        free(url->psz_host);
        url->psz_host = NULL;
    }

    /* Port number */
    if (next != NULL)
        url->i_port = atoi (next);

    if (url->psz_path != NULL)
        *url->psz_path = '/'; /* restore leading slash */
    if (!vlc_uri_path_validate(url->psz_path))
        url->psz_path = NULL;
}

/**
 * Releases resources allocated by vlc_UrlParse().
 */
void vlc_UrlClean (vlc_url_t *restrict url)
{
    free (url->psz_host);
    free (url->psz_buffer);
}

#if defined (HAVE_IDN)
# include <idna.h>
#elif defined (_WIN32)
# include <windows.h>
# include <vlc_charset.h>
#endif

/**
 * Converts a UTF-8 nul-terminated IDN to nul-terminated ASCII domain name.
 * \param idn UTF-8 Internationalized Domain Name to convert
 * \return a heap-allocated string or NULL on error.
 */
static char *vlc_idna_to_ascii (const char *idn)
{
#if defined (HAVE_IDN)
    char *adn;

    if (idna_to_ascii_8z (idn, &adn, IDNA_ALLOW_UNASSIGNED) != IDNA_SUCCESS)
        return NULL;
    return adn;

#elif defined (_WIN32) && (_WIN32_WINNT >= 0x0601)
    char *ret = NULL;

    wchar_t *wide = ToWide (idn);
    if (wide == NULL)
        return NULL;

    int len = IdnToAscii (IDN_ALLOW_UNASSIGNED, wide, -1, NULL, 0);
    if (len == 0)
        goto error;

    wchar_t *buf = malloc (sizeof (*buf) * len);
    if (unlikely(buf == NULL))
        goto error;
    if (!IdnToAscii (IDN_ALLOW_UNASSIGNED, wide, -1, buf, len))
    {
        free (buf);
        goto error;
    }
    ret = FromWide (buf);
    free (buf);
error:
    free (wide);
    return ret;

#else
    /* No IDN support, filter out non-ASCII domain names */
    for (const char *p = idn; *p; p++)
        if (((unsigned char)*p) >= 0x80)
            return NULL;

    return strdup (idn);

#endif
}
