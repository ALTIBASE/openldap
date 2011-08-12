/* idl.c - ldap bdb back-end ID list functions */
/* $OpenLDAP$ */
/* This work is part of OpenLDAP Software <http://www.openldap.org/>.
 *
 * Copyright 2000-2011 The OpenLDAP Foundation.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted only as authorized by the OpenLDAP
 * Public License.
 *
 * A copy of this license is available in the file LICENSE in the
 * top-level directory of the distribution or, alternatively, at
 * <http://www.OpenLDAP.org/license.html>.
 */

#include <string.h>
#include <sys/types.h>
#include <assert.h>
#include "midl.h"

typedef unsigned long pgno_t;

/* Sort the IDLs from highest to lowest */
#define IDL_CMP(x,y)	 ( x > y ? -1 : ( x < y ? 1 : 0 ) )

unsigned mdb_midl_search( ID *ids, ID id )
{
	/*
	 * binary search of id in ids
	 * if found, returns position of id
	 * if not found, returns first position greater than id
	 */
	unsigned base = 0;
	unsigned cursor = 0;
	int val = 0;
	unsigned n = ids[0];

	while( 0 < n ) {
		int pivot = n >> 1;
		cursor = base + pivot;
		val = IDL_CMP( id, ids[cursor + 1] );

		if( val < 0 ) {
			n = pivot;

		} else if ( val > 0 ) {
			base = cursor + 1;
			n -= pivot + 1;

		} else {
			return cursor + 1;
		}
	}
	
	if( val > 0 ) {
		return cursor + 2;
	} else {
		return cursor + 1;
	}
}

int mdb_midl_insert( ID *ids, ID id )
{
	unsigned x;

	if (MDB_IDL_IS_RANGE( ids )) {
		/* if already in range, treat as a dup */
		if (id >= MDB_IDL_FIRST(ids) && id <= MDB_IDL_LAST(ids))
			return -1;
		if (id < MDB_IDL_FIRST(ids))
			ids[1] = id;
		else if (id > MDB_IDL_LAST(ids))
			ids[2] = id;
		return 0;
	}

	x = mdb_midl_search( ids, id );
	assert( x > 0 );

	if( x < 1 ) {
		/* internal error */
		return -2;
	}

	if ( x <= ids[0] && ids[x] == id ) {
		/* duplicate */
		return -1;
	}

	if ( ++ids[0] >= MDB_IDL_DB_MAX ) {
		if( id < ids[1] ) {
			ids[1] = id;
			ids[2] = ids[ids[0]-1];
		} else if ( ids[ids[0]-1] < id ) {
			ids[2] = id;
		} else {
			ids[2] = ids[ids[0]-1];
		}
		ids[0] = NOID;
	
	} else {
		/* insert id */
		AC_MEMCPY( &ids[x+1], &ids[x], (ids[0]-x) * sizeof(ID) );
		ids[x] = id;
	}

	return 0;
}
