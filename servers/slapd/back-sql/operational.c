/* $OpenLDAP$ */
/* This work is part of OpenLDAP Software <http://www.openldap.org/>.
 *
 * Copyright 1999-2004 The OpenLDAP Foundation.
 * Portions Copyright 1999 Dmitry Kovalev.
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
/* ACKNOWLEDGEMENTS:
 * This work was initially developed by Dmitry Kovalev for inclusion
 * by OpenLDAP Software.
 */

#include "portable.h"

#ifdef SLAPD_SQL

#include <stdio.h>
#include <sys/types.h>

#include "slap.h"
#include "proto-sql.h"
#include "lutil.h"

/*
 * sets the supported operational attributes (if required)
 */

Attribute *
backsql_operational_entryUUID( backsql_info *bi, backsql_entryID *id )
{
	int			rc;
	struct berval		val, nval;
	AttributeDescription	*desc = slap_schema.si_ad_entryUUID;
	Attribute		*a;

	backsql_entryUUID( bi, id, &val, NULL );

	rc = (*desc->ad_type->sat_equality->smr_normalize)(
			SLAP_MR_VALUE_OF_ATTRIBUTE_SYNTAX,
			desc->ad_type->sat_syntax,
			desc->ad_type->sat_equality,
			&val, &nval, NULL );
	if ( rc != LDAP_SUCCESS ) {
		ber_memfree( val.bv_val );
		return NULL;
	}

	a = ch_malloc( sizeof( Attribute ) );
	a->a_desc = desc;

	a->a_vals = (BerVarray) ch_malloc( 2 * sizeof( struct berval ) );
	a->a_vals[ 0 ] = val;
	BER_BVZERO( &a->a_vals[ 1 ] );

	a->a_nvals = (BerVarray) ch_malloc( 2 * sizeof( struct berval ) );
	a->a_nvals[ 0 ] = nval;
	BER_BVZERO( &a->a_nvals[ 1 ] );
	
	a->a_next = NULL;
	a->a_flags = 0;

	return a;
}

Attribute *
backsql_operational_entryCSN( Operation *op )
{
	char		csnbuf[ LDAP_LUTIL_CSNSTR_BUFSIZE ];
	struct berval	entryCSN;
	Attribute	*a;

	a = ch_malloc( sizeof( Attribute ) );
	a->a_desc = slap_schema.si_ad_entryCSN;
	a->a_vals = ch_malloc( 2 * sizeof( struct berval ) );
	BER_BVZERO( &a->a_vals[ 1 ] );

	slap_get_csn( op, csnbuf, sizeof(csnbuf), &entryCSN, 0 );
	ber_dupbv( &a->a_vals[ 0 ], &entryCSN );

	a->a_nvals = a->a_vals;

	a->a_next = NULL;
	a->a_flags = 0;

	return a;
}

int
backsql_operational(
	Operation	*op,
	SlapReply	*rs )
{

	backsql_info 	*bi = (backsql_info*)op->o_bd->be_private;
	SQLHDBC 	dbh = SQL_NULL_HDBC;
	int		rc = 0;
	Attribute	**ap;
	enum {
		BACKSQL_OP_HASSUBORDINATES = 0,
		BACKSQL_OP_ENTRYUUID,
		BACKSQL_OP_ENTRYCSN,

		BACKSQL_OP_LAST
	};
	int		get_conn = BACKSQL_OP_LAST,
			got[ BACKSQL_OP_LAST ] = { 0 };

	Debug( LDAP_DEBUG_TRACE, "==>backsql_operational(): entry \"%s\"\n",
			rs->sr_entry->e_nname.bv_val, 0, 0 );

	for ( ap = &rs->sr_operational_attrs; *ap; ap = &(*ap)->a_next ) {
		if ( (*ap)->a_desc == slap_schema.si_ad_hasSubordinates ) {
			get_conn--;
			got[ BACKSQL_OP_HASSUBORDINATES ] = 1;

		} else if ( (*ap)->a_desc == slap_schema.si_ad_entryUUID ) {
			get_conn--;
			got[ BACKSQL_OP_ENTRYUUID ] = 1;

		} else if ( (*ap)->a_desc == slap_schema.si_ad_entryCSN ) {
			get_conn--;
			got[ BACKSQL_OP_ENTRYCSN ] = 1;
		}
	}

	if ( !get_conn ) {
		return 0;
	}

	rc = backsql_get_db_conn( op, &dbh );
	if ( rc != LDAP_SUCCESS ) {
		Debug( LDAP_DEBUG_TRACE, "backsql_operational(): "
			"could not get connection handle - exiting\n", 
			0, 0, 0 );
		return 1;
	}

	if ( ( SLAP_OPATTRS( rs->sr_attr_flags ) || ad_inlist( slap_schema.si_ad_hasSubordinates, rs->sr_attrs ) ) 
			&& !got[ BACKSQL_OP_HASSUBORDINATES ]
			&& attr_find( rs->sr_entry->e_attrs, slap_schema.si_ad_hasSubordinates ) == NULL )
	{
		rc = backsql_has_children( bi, dbh, &rs->sr_entry->e_nname );

		switch( rc ) {
		case LDAP_COMPARE_TRUE:
		case LDAP_COMPARE_FALSE:
			*ap = slap_operational_hasSubordinate( rc == LDAP_COMPARE_TRUE );
			assert( *ap );
			ap = &(*ap)->a_next;
			rc = 0;
			break;

		default:
			Debug( LDAP_DEBUG_TRACE, "backsql_operational(): "
				"has_children failed( %d)\n", rc, 0, 0 );
			return 1;
		}
	}

	if ( ( SLAP_OPATTRS( rs->sr_attr_flags ) || ad_inlist( slap_schema.si_ad_entryUUID, rs->sr_attrs ) ) 
			&& !got[ BACKSQL_OP_ENTRYUUID ]
			&& attr_find( rs->sr_entry->e_attrs, slap_schema.si_ad_entryUUID ) == NULL )
	{
		struct berval		ndn;
		backsql_srch_info	bsi;

		ndn = rs->sr_entry->e_nname;
		if ( backsql_api_dn2odbc( op, rs, &ndn ) ) {
			Debug( LDAP_DEBUG_TRACE, "backsql_operational(): "
				"backsql_api_dn2odbc failed\n", 
				0, 0, 0 );
			return 1;
		}

		rc = backsql_init_search( &bsi, &ndn, LDAP_SCOPE_BASE, 
			-1, -1, -1, NULL, dbh, op, rs, NULL, 1 );
		if ( rc != LDAP_SUCCESS ) {
			Debug( LDAP_DEBUG_TRACE, "backsql_operational(): "
				"could not retrieve entry ID - no such entry\n", 
				0, 0, 0 );
			return 1;
		}

		*ap = backsql_operational_entryUUID( bi, &bsi.bsi_base_id );

		(void)backsql_free_entryID( &bsi.bsi_base_id, 0 );

		if ( ndn.bv_val != rs->sr_entry->e_nname.bv_val ) {
			free( ndn.bv_val );
		}

		if ( *ap == NULL ) {
			Debug( LDAP_DEBUG_TRACE, "backsql_operational(): "
				"could not retrieve entryUUID\n", 
				0, 0, 0 );
			return 1;
		}

		ap = &(*ap)->a_next;
	}

	if ( ( SLAP_OPATTRS( rs->sr_attr_flags ) || ad_inlist( slap_schema.si_ad_entryCSN, rs->sr_attrs ) ) 
			&& !got[ BACKSQL_OP_ENTRYCSN ]
			&& attr_find( rs->sr_entry->e_attrs, slap_schema.si_ad_entryCSN ) == NULL )
	{
		*ap = backsql_operational_entryCSN( op );
		if ( *ap == NULL ) {
			Debug( LDAP_DEBUG_TRACE, "backsql_operational(): "
				"could not retrieve entryCSN\n", 
				0, 0, 0 );
			return 1;
		}

		ap = &(*ap)->a_next;
	}

	Debug( LDAP_DEBUG_TRACE, "<==backsql_operational(%d)\n", rc, 0, 0);

	return rc;
}

#endif /* SLAPD_SQL */

