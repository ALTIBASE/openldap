/* back-mdb.h - mdb back-end header file */
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

#ifndef _BACK_MDB_H_
#define _BACK_MDB_H_

#include <portable.h>
#include "slap.h"
#include "mdb.h"

LDAP_BEGIN_DECL

#define DN_BASE_PREFIX		SLAP_INDEX_EQUALITY_PREFIX
#define DN_ONE_PREFIX	 	'%'
#define DN_SUBTREE_PREFIX 	'@'

#define MDB_AD2ID		0
#define MDB_DN2ID		1
#define MDB_ID2ENTRY	2
#define MDB_NDB			3

/* The default search IDL stack cache depth */
#define DEFAULT_SEARCH_STACK_DEPTH	16

/* The minimum we can function with */
#define MINIMUM_SEARCH_STACK_DEPTH	8

#define MDB_INDICES		128

struct mdb_db_info {
	struct berval	mdi_name;
	MDB_dbi	mdi_dbi;
};

#ifdef LDAP_DEVEL
#define MDB_MONITOR_IDX
#endif /* LDAP_DEVEL */

typedef struct mdb_monitor_t {
	void		*mdm_cb;
	struct berval	mdm_ndn;
} mdb_monitor_t;

/* From ldap_rq.h */
struct re_s;

struct mdb_info {
	MDB_env		*mi_dbenv;

	/* DB_ENV parameters */
	/* The DB_ENV can be tuned via DB_CONFIG */
	char		*mi_dbenv_home;
	u_int32_t	mi_dbenv_flags;
	int			mi_dbenv_mode;

	size_t		mi_mapsize;

	int			mi_ndatabases;
	int			mi_db_opflags;	/* db-specific flags */
	struct mdb_db_info **mi_databases;
	ldap_pvt_thread_mutex_t	mi_database_mutex;

	slap_mask_t	mi_defaultmask;
	struct mdb_attrinfo		**mi_attrs;
	int			mi_nattrs;
	void		*mi_search_stack;
	int			mi_search_stack_depth;

	int			mi_txn_cp;
	u_int32_t	mi_txn_cp_min;
	u_int32_t	mi_txn_cp_kbyte;
	struct re_s		*mi_txn_cp_task;
	struct re_s		*mi_index_task;

	mdb_monitor_t	mi_monitor;

#ifdef MDB_MONITOR_IDX
	ldap_pvt_thread_mutex_t	mi_idx_mutex;
	Avlnode		*mi_idx;
#endif /* MDB_MONITOR_IDX */

	int		mi_flags;
#define	MDB_IS_OPEN		0x01
#define	MDB_DEL_INDEX	0x08
#define	MDB_RE_OPEN		0x10
};

#define mi_id2entry	mi_databases[MDB_ID2ENTRY]
#define mi_dn2id	mi_databases[MDB_DN2ID]
#define mi_ad2id	mi_databases[MDB_AD2ID]

struct mdb_op_info {
	OpExtra		moi_oe;
	MDB_txn*	moi_txn;
	u_int32_t	moi_err;
	char		moi_acl_cache;
	char		moi_flag;
};
#define MOI_DONTFREE	1

/* Copy an ID "src" to pointer "dst" in big-endian byte order */
#define MDB_ID2DISK( src, dst )	\
	do { int i0; ID tmp; unsigned char *_p;	\
		tmp = (src); _p = (unsigned char *)(dst);	\
		for ( i0=sizeof(ID)-1; i0>=0; i0-- ) {	\
			_p[i0] = tmp & 0xff; tmp >>= 8;	\
		} \
	} while(0)

/* Copy a pointer "src" to a pointer "dst" from big-endian to native order */
#define MDB_DISK2ID( src, dst ) \
	do { unsigned i0; ID tmp = 0; unsigned char *_p;	\
		_p = (unsigned char *)(src);	\
		for ( i0=0; i0<sizeof(ID); i0++ ) {	\
			tmp <<= 8; tmp |= *_p++;	\
		} *(dst) = tmp; \
	} while (0)

LDAP_END_DECL

/* for the cache of attribute information (which are indexed, etc.) */
typedef struct mdb_attrinfo {
	AttributeDescription *ai_desc; /* attribute description cn;lang-en */
	slap_mask_t ai_indexmask;	/* how the attr is indexed	*/
	slap_mask_t ai_newmask;	/* new settings to replace old mask */
#ifdef LDAP_COMP_MATCH
	ComponentReference* ai_cr; /*component indexing*/
#endif
	int ai_idx;	/* position in AI array */
} AttrInfo;

/* These flags must not clash with SLAP_INDEX flags or ops in slap.h! */
#define	MDB_INDEX_DELETING	0x8000U	/* index is being modified */
#define	MDB_INDEX_UPDATE_OP	0x03	/* performing an index update */

/* For slapindex to record which attrs in an entry belong to which
 * index database 
 */
typedef struct AttrList {
	struct AttrList *next;
	Attribute *attr;
} AttrList;

typedef struct IndexRec {
	AttrInfo *ai;
	AttrList *attrs;
} IndexRec;

#include "proto-mdb.h"

#endif /* _BACK_MDB_H_ */
