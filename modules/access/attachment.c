/*****************************************************************************
 * attachment.c: Input reading an attachment.
 *****************************************************************************
 * Copyright (C) 2009 Laurent Aimar
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir _AT_ videolan _DOT_ org>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_access.h>
#include <vlc_input.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open (vlc_object_t *);
static void Close(vlc_object_t *);

vlc_module_begin()
    set_shortname(N_("Attachment"))
    set_description(N_("Attachment input"))
    set_category(CAT_INPUT)
    set_subcategory(SUBCAT_INPUT_ACCESS)

    set_capability("access", 0)
    add_shortcut("attachment")
    set_callbacks(Open, Close)
vlc_module_end()

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static ssize_t Read(access_t *, uint8_t *, size_t);
static int     Seek(access_t *, uint64_t);
static int     Control(access_t *, int, va_list);

struct access_sys_t
{
    input_attachment_t *attachment;
    size_t offset;
};

/* */
static int Open(vlc_object_t *object)
{
    access_t     *access = (access_t *)object;

    input_thread_t *input = access->p_input;
    if (!input)
        return VLC_EGENERIC;

    access_sys_t *sys = malloc(sizeof (*sys));
    if (unlikely(sys == NULL))
        return VLC_ENOMEM;

    if (input_Control(input, INPUT_GET_ATTACHMENT, &sys->attachment,
                      access->psz_location))
        sys->attachment = NULL;

    if (sys->attachment == NULL) {
        msg_Err(access, "Failed to find the attachment '%s'",
                access->psz_location);
        free(sys);
        return VLC_EGENERIC;
    }

    sys->offset = 0;

    /* */
    access_InitFields(access);
    access->pf_read    = Read;
    access->pf_block   = NULL;
    access->pf_control = Control;
    access->pf_seek    = Seek;
    access->p_sys      = sys;
    return VLC_SUCCESS;
}

/* */
static void Close(vlc_object_t *object)
{
    access_t     *access = (access_t *)object;
    access_sys_t *sys = access->p_sys;

    vlc_input_attachment_Delete(sys->attachment);
    free(sys);
}

/* */
static ssize_t Read(access_t *access, uint8_t *buffer, size_t size)
{
    access_sys_t *sys = access->p_sys;
    input_attachment_t *a = sys->attachment;

    access->info.b_eof = sys->offset >= (uint64_t)a->i_data;
    if (access->info.b_eof)
        return 0;

    const size_t copy = __MIN(size, a->i_data - sys->offset);
    memcpy(buffer, (uint8_t *)a->p_data + sys->offset, copy);
    sys->offset += copy;
    return copy;
}

/* */
static int Seek(access_t *access, uint64_t position)
{
    access_sys_t *sys = access->p_sys;
    input_attachment_t *a = sys->attachment;

    if (position > a->i_data)
        position = a->i_data;

    sys->offset = position;
    access->info.b_eof = false;
    return VLC_SUCCESS;
}

/* */
static int Control(access_t *access, int query, va_list args)
{
    access_sys_t *sys = access->p_sys;

    switch (query)
    {
    case ACCESS_CAN_SEEK:
    case ACCESS_CAN_FASTSEEK:
    case ACCESS_CAN_PAUSE:
    case ACCESS_CAN_CONTROL_PACE:
        *va_arg(args, bool *) = true;
        break;
    case ACCESS_GET_SIZE:
        *va_arg(args, uint64_t *) = sys->attachment->i_data;
        break;
    case ACCESS_GET_PTS_DELAY:
        *va_arg(args, int64_t *) = DEFAULT_PTS_DELAY;
        break;
    case ACCESS_SET_PAUSE_STATE:
        return VLC_SUCCESS;

    default:
        return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}

