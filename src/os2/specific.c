/*****************************************************************************
 * specific.c: OS/2 specific features
 *****************************************************************************
 * Copyright (C) 2010 KO Myung-Hun
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

#include <vlc_common.h>
#include "../libvlc.h"

#include <fcntl.h>
#include <io.h>

extern int _fmode_bin;

void system_Init( void )
{
    /* Set the default file-translation mode */
    _fmode_bin = 1;
    setmode( fileno( stdin ), O_BINARY ); /* Needed for pipes */
}

void system_Configure( libvlc_int_t *p_this, int i_argc, const char *const ppsz_argv[] )
{
    VLC_UNUSED( i_argc ); VLC_UNUSED( ppsz_argv );
    if( var_InheritBool( p_this, "high-priority" ) )
    {
        if( !DosSetPriority( PRTYS_PROCESS, PRTYC_REGULAR, PRTYD_MAXIMUM, 0 ) )
        {
            msg_Dbg( p_this, "raised process priority" );
        }
        else
        {
            msg_Dbg( p_this, "could not raise process priority" );
        }
    }
}
