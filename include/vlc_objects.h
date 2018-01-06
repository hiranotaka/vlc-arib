/*****************************************************************************
 * vlc_objects.h: vlc_object_t definition and manipulation methods
 *****************************************************************************
 * Copyright (C) 2002-2008 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
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

/**
 * \defgroup vlc_object VLC objects
 * @{
 * \file
 * Common VLC object defintions
 */

/**
 * VLC object common members
 *
 * Common public properties for all VLC objects.
 * Object also have private properties maintained by the core, see
 * \ref vlc_object_internals_t
 */
struct vlc_common_members
{
    /** Object type name
     *
     * A constant string identifying the type of the object (for logging)
     */
    const char *object_type;

    /** Log messages header
     *
     * Human-readable header for log messages. This is not thread-safe and
     * only used by VLM and Lua interfaces.
     */
    char *header;

    int  flags;

    /** Module probe flag
     *
     * A boolean during module probing when the probe is "forced".
     * See \ref module_need().
     */
    bool force;

    /** LibVLC instance
     *
     * Root VLC object of the objects tree that this object belongs in.
     */
    libvlc_int_t *libvlc;

    /** Parent object
     *
     * The parent VLC object in the objects tree. For the root (the LibVLC
     * instance) object, this is NULL.
     */
    vlc_object_t *parent;
};

/**
 * Type-safe vlc_object_t cast
 *
 * This macro attempts to cast a pointer to a compound type to a
 * \ref vlc_object_t pointer in a type-safe manner.
 * It checks if the compound type actually starts with an embedded
 * \ref vlc_object_t structure.
 */
#if !defined(__cplusplus)
# define VLC_OBJECT(x) \
    _Generic((x)->obj, \
        struct vlc_common_members: (vlc_object_t *)(&(x)->obj), \
        const struct vlc_common_members: (const vlc_object_t *)(&(x)->obj) \
    )
#else
# define VLC_OBJECT( x ) ((vlc_object_t *)&(x)->obj)
#endif

/* Object flags */
#define OBJECT_FLAGS_QUIET       0x0002
#define OBJECT_FLAGS_NOINTERACT  0x0004

/*****************************************************************************
 * The vlc_object_t type. Yes, it's that simple :-)
 *****************************************************************************/
/** The main vlc_object_t structure */
struct vlc_object_t
{
    struct vlc_common_members obj;
};

/* The root object */
struct libvlc_int_t
{
    struct vlc_common_members obj;
};

/*****************************************************************************
 * Prototypes
 *****************************************************************************/
VLC_API void *vlc_object_create( vlc_object_t *, size_t ) VLC_MALLOC VLC_USED;
VLC_API vlc_object_t *vlc_object_find_name( vlc_object_t *, const char * ) VLC_USED VLC_DEPRECATED;
VLC_API void * vlc_object_hold( vlc_object_t * );
VLC_API void vlc_object_release( vlc_object_t * );
VLC_API vlc_list_t *vlc_list_children( vlc_object_t * ) VLC_USED;
VLC_API void vlc_list_release( vlc_list_t * );
VLC_API char *vlc_object_get_name( const vlc_object_t * ) VLC_USED;
#define vlc_object_get_name(o) vlc_object_get_name(VLC_OBJECT(o))

#define vlc_object_create(a,b) vlc_object_create( VLC_OBJECT(a), b )

#define vlc_object_find_name(a,b) \
    vlc_object_find_name( VLC_OBJECT(a),b)

#define vlc_object_hold(a) \
    vlc_object_hold( VLC_OBJECT(a) )

#define vlc_object_release(a) \
    vlc_object_release( VLC_OBJECT(a) )

#define vlc_list_children(a) \
    vlc_list_children( VLC_OBJECT(a) )

VLC_API VLC_MALLOC void *vlc_obj_malloc(vlc_object_t *, size_t);
VLC_API VLC_MALLOC void *vlc_obj_calloc(vlc_object_t *, size_t, size_t);
VLC_API void vlc_obj_free(vlc_object_t *, void *);

/** @} */
