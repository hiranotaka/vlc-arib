/*****************************************************************************
 * epg.c test EPG
 *****************************************************************************
 * Copyright (C) 2016 - VideoLAN Authors
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
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *****************************************************************************/
#include "../../libvlc/test.h"
#ifdef NDEBUG
 #undef NDEBUG
#endif
#include <vlc_common.h>
#include <vlc_epg.h>
#include <assert.h>

static void assert_current( const vlc_epg_t *p_epg, const char *psz_name )
{
    if( (void*)psz_name == (void*)p_epg->p_current )
    {
        assert( psz_name == NULL );
    }
    else
    {
        assert( p_epg->p_current );
        assert( p_epg->p_current->psz_name );
        assert( psz_name[0] == p_epg->p_current->psz_name[0] );
    }
}

static void print_order( const vlc_epg_t *p_epg )
{
    printf("order: ");
    for( int i=0; i<p_epg->i_event; i++ )
        printf("%s ", p_epg->pp_event[i]->psz_name );
    if( p_epg->p_current )
        printf(" current %s", p_epg->p_current->psz_name );
    printf("\n");
}

static void assert_events( const vlc_epg_t *p_epg, const char *psz_names, int i_names )
{
    assert( p_epg->i_event == i_names );
    for( int i=0; i<p_epg->i_event; i++ )
    {
        assert( p_epg->pp_event[i]->psz_name &&
                p_epg->pp_event[i]->psz_name[0] == psz_names[i] );
    }
}

#define EPG_ADD(epg, start, duration, a) \
    vlc_epg_AddEvent( epg, start, duration, a, NULL, NULL, 0 )

int main( void )
{
    test_init();

    int i=1;

    /* Simple insert/current test */
    printf("--test %d\n", i++);
    vlc_epg_t *p_epg = vlc_epg_New( NULL );
    assert(p_epg);
    EPG_ADD( p_epg,  42, 20, "A" );
    EPG_ADD( p_epg,  62, 20, "B" );
    EPG_ADD( p_epg,  82, 20, "C" );
    EPG_ADD( p_epg, 102, 20, "D" );
    print_order( p_epg );
    assert_events( p_epg, "ABCD", 4 );
    assert_current( p_epg, NULL );

    vlc_epg_SetCurrent( p_epg, 82 );
    assert_current( p_epg, "C" );

    vlc_epg_Delete( p_epg );


    /* Test reordering / head/tail inserts */
    printf("--test %d\n", i++);
    p_epg = vlc_epg_New( NULL );
    assert(p_epg);
    EPG_ADD( p_epg,  82, 20, "C" );
    EPG_ADD( p_epg,  62, 20, "B" );
    EPG_ADD( p_epg, 102, 20, "D" );
    EPG_ADD( p_epg,  42, 20, "A" );
    print_order( p_epg );
    assert_events( p_epg, "ABCD", 4 );
    vlc_epg_Delete( p_epg );

    /* Test reordering/bisect lookup on insert */
    printf("--test %d\n", i++);
    p_epg = vlc_epg_New( NULL );
    assert(p_epg);
    EPG_ADD( p_epg, 142, 20, "F" );
    EPG_ADD( p_epg, 122, 20, "E" );
    EPG_ADD( p_epg, 102, 20, "D" );
    EPG_ADD( p_epg,  82, 20, "C" );
    EPG_ADD( p_epg,  42, 20, "A" );
    EPG_ADD( p_epg,  62, 20, "B" );
    print_order( p_epg );
    assert_events( p_epg, "ABCDEF", 6 );
    vlc_epg_Delete( p_epg );

    /* Test deduplication and current pointer rebasing on insert */
    printf("--test %d\n", i++);
    p_epg = vlc_epg_New( NULL );
    assert(p_epg);
    EPG_ADD( p_epg,  62, 20, "E" );
    EPG_ADD( p_epg,  62, 20, "F" );
    EPG_ADD( p_epg,  42, 20, "A" );
    vlc_epg_SetCurrent( p_epg, 62 );
    EPG_ADD( p_epg,  82, 20, "C" );
    EPG_ADD( p_epg,  62, 20, "B" );
    EPG_ADD( p_epg, 102, 20, "D" );
    print_order( p_epg );
    assert_events( p_epg, "ABCD", 4 );
    assert_current( p_epg, "B" );
    vlc_epg_Delete( p_epg );


    /* Test epg merging */
    printf("--test %d\n", i++);
    p_epg = vlc_epg_New( NULL );
    assert(p_epg);
    EPG_ADD( p_epg, 142, 20, "F" );
    EPG_ADD( p_epg, 122, 20, "E" );
    EPG_ADD( p_epg,  42, 20, "A" );
    EPG_ADD( p_epg,  62, 20, "B" );
    print_order( p_epg );

    vlc_epg_t *p_epg2 = vlc_epg_New( NULL );
    assert(p_epg2);
    EPG_ADD( p_epg2, 102, 20, "D" );
    EPG_ADD( p_epg2,  82, 20, "C" );
    print_order( p_epg2 );

    vlc_epg_Merge( p_epg, p_epg2 );
    printf("merged " );
    print_order( p_epg );

    assert_events( p_epg, "ABCDEF", 6 );
    assert_events( p_epg2, "CD", 2 ); /* should be untouched */
    vlc_epg_Delete( p_epg );
    vlc_epg_Delete( p_epg2 );


    /* Test event overlapping */
    printf("--test %d\n", i++);
    p_epg = vlc_epg_New( NULL );
    assert(p_epg);
    EPG_ADD( p_epg,  42, 20, "A" );
    EPG_ADD( p_epg,  62, 20, "B" );
    EPG_ADD( p_epg,  82, 20, "C" );
    EPG_ADD( p_epg, 102, 20, "D" );
    print_order( p_epg );
    vlc_epg_SetCurrent( p_epg, 62 );

    p_epg2 = vlc_epg_New( NULL );
    assert(p_epg2);
    EPG_ADD( p_epg2,  41, 30, "E" );
    print_order( p_epg2 );

    vlc_epg_Merge( p_epg, p_epg2 );
    printf("merged " );
    print_order( p_epg );
    assert_events( p_epg, "ECD", 3 );

    assert_current( p_epg, "E" );

    EPG_ADD( p_epg2,  70, 42, "F" );
    print_order( p_epg2 );
    vlc_epg_Merge( p_epg, p_epg2 );
    printf("merged " );
    print_order( p_epg );
    assert_events( p_epg, "F", 1 );

    /* Test current overwriting */
    printf("--test %d\n", i++);
    vlc_epg_SetCurrent( p_epg, 70 );
    assert_current( p_epg, "F" );
    print_order( p_epg );
    print_order( p_epg2 );
    vlc_epg_Merge( p_epg, p_epg2 );
    printf("merged " );
    print_order( p_epg );
    assert_events( p_epg, "F", 1 );
    assert_current( p_epg, "F" );

    printf("--test %d\n", i++);
    print_order( p_epg );
    EPG_ADD( p_epg2,  270, 42, "Z" );
    vlc_epg_SetCurrent( p_epg2, 270 );
    print_order( p_epg2 );
    vlc_epg_Merge( p_epg, p_epg2 );
    printf("merged " );
    print_order( p_epg );
    assert_current( p_epg, "Z" );

    vlc_epg_Delete( p_epg );
    vlc_epg_Delete( p_epg2 );

    return 0;
}
