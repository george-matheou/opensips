/**
 * Copyright (C) 2010 Elena-Ramona Modroiu (asipto.com)
 *
 * This file is part of opensips, a free SIP server.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */


#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "../../dprint.h"
#include "../../mem/mem.h"
#include "../../mem/shm_mem.h"
#include "../../parser/parse_param.h"
#include "../../ut.h"

#include "mqueue_api.h"
#include "mqueue_db.h"

/**
 *
 */
//static mq_head_t *_mq_head_list = NULL;
mq_head_t *_mq_head_list = NULL;

/**
 *
 */
static mq_pv_t *_mq_pv_list = NULL;

/**
 *
 */
int mq_head_defined(void)
{
	if(_mq_head_list != NULL)
		return 1;
	return 0;
}

/**
 *
 */
void mq_destroy(void)
{
	mq_head_t *mh = NULL;
	mq_pv_t *mp = NULL;
	mq_item_t *mi = NULL;
	mq_head_t *mh1 = NULL;
	mq_pv_t *mp1 = NULL;
	mq_item_t *mi1 = NULL;

	mh = _mq_head_list;
	while(mh != NULL) {
		if(mh->dbmode == 1 || mh->dbmode == 3) {
			LM_INFO("mqueue[%.*s] dbmode[%d]\n", mh->name.len, mh->name.s,
					mh->dbmode);
			mqueue_db_save_queue(&mh->name);
		}
		mi = mh->ifirst;
		while(mi != NULL) {
			mi1 = mi;
			mi = mi->next;
			shm_free(mi1);
		}
		mh1 = mh;
		mh = mh->next;
		lock_destroy(&mh1->lock);
		shm_free(mh1);
	}
	_mq_head_list = 0;
	mp = _mq_pv_list;
	while(mp != NULL) {
		mp1 = mp;
		mp = mp->next;
		pkg_free(mp1);
	}
}

/**
 *
 */
int mq_head_add(str *name, int msize, int addmode)
{
	mq_head_t *mh = NULL;
	mq_pv_t *mp = NULL;
	int len;

	mh = _mq_head_list;
	while(mh != NULL) {
		if(name->len == mh->name.len
				&& strncmp(mh->name.s, name->s, name->len) == 0) {
			LM_ERR("mqueue redefined: %.*s\n", name->len, name->s);
			return -1;
		}
		mh = mh->next;
	}

	mp = (mq_pv_t *)pkg_malloc(sizeof(mq_pv_t));
	if(mp == NULL) {
		LM_ERR("no more pkg for: %.*s\n", name->len, name->s);
		return -1;
	}
	memset(mp, 0, sizeof(mq_pv_t));

	len = sizeof(mq_head_t) + name->len + 1;
	mh = (mq_head_t *)shm_malloc(len);
	if(mh == NULL) {
		LM_ERR("no more shm for: %.*s\n", name->len, name->s);
		pkg_free(mp);
		return -1;
	}
	memset(mh, 0, len);
	if(lock_init(&mh->lock) == 0) {
		LM_CRIT("failed to init lock\n");
		pkg_free(mp);
		shm_free(mh);
		return -1;
	}

	mh->name.s = (char *)mh + sizeof(mq_head_t);
	memcpy(mh->name.s, name->s, name->len);
	mh->name.len = name->len;
	mh->name.s[name->len] = '\0';
	mh->msize = msize;
	mh->addmode = addmode;
	mh->next = _mq_head_list;
	_mq_head_list = mh;

	mp->name = &mh->name;
	mp->next = _mq_pv_list;
	_mq_pv_list = mp;

	return 0;
}

/**
 *
 */
mq_head_t *mq_head_get(str *name)
{
	mq_head_t *mh = NULL;

	mh = _mq_head_list;
	if(!name) {
		return mh;
	}
	while(mh != NULL) {
		if(name->len == mh->name.len
				&& strncmp(mh->name.s, name->s, name->len) == 0) {
			return mh;
		}
		mh = mh->next;
	}
	return NULL;
}

/**
 *
 */
int mq_set_dbmode(str *name, int dbmode)
{
	mq_head_t *mh = NULL;

	mh = _mq_head_list;
	while(mh != NULL) {
		if(name->len == mh->name.len
				&& strncmp(mh->name.s, name->s, name->len) == 0) {
			mh->dbmode = dbmode;
			return 0;
		}
		mh = mh->next;
	}
	return -1;
}

/**
 *
 */
int mq_get_dbmode(str *name)
{
	mq_head_t *mh = NULL;

	mh = _mq_head_list;
	while(mh != NULL) {
		if(name->len == mh->name.len
				&& strncmp(mh->name.s, name->s, name->len) == 0)
			return mh->dbmode;
		mh = mh->next;
	}
	return -1;
}

/**
 *
 */
mq_pv_t *mq_pv_get(str *name)
{
	mq_pv_t *mp = NULL;

	mp = _mq_pv_list;
	while(mp != NULL) {
		if(mp->name->len == name->len
				&& strncmp(mp->name->s, name->s, name->len) == 0)
			return mp;
		mp = mp->next;
	}
	return NULL;
}

/**
 *
 */
mq_item_t *mq_head_fetch_item(mq_head_t *mh)
{
	mq_item_t *ret;

	if(mh->ifirst == NULL)
		return NULL;

	ret = mh->ifirst;
	mh->ifirst = mh->ifirst->next;
	if(mh->ifirst == NULL)
		mh->ilast = NULL;
	mh->csize--;
	return ret;
}

/**
 *
 */
int mq_head_fetch(str *name)
{
	mq_head_t *mh = NULL;
	mq_pv_t *mp = NULL;

	mp = mq_pv_get(name);
	if(mp == NULL)
		return -1;
	if(mp->item != NULL) {
		shm_free(mp->item);
		mp->item = NULL;
	}
	mh = mq_head_get(name);
	if(mh == NULL)
		return -1;
	lock_get(&mh->lock);

	mp->item = mq_head_fetch_item(mh);

	lock_release(&mh->lock);
	return (mp->item?0:-2);
}

/**
 *
 */
void mq_pv_free(str *name)
{
	mq_pv_t *mp = NULL;

	mp = mq_pv_get(name);
	if(mp == NULL)
		return;
	if(mp->item != NULL) {
		shm_free(mp->item);
		mp->item = NULL;
	}
}

/**
 *
 */
int mq_item_add(str *qname, str *key, str *val)
{
	mq_head_t *mh = NULL;
	mq_item_t *mi = NULL;
	mq_item_t *miter = NULL;
	mq_item_t *miter_prev = NULL;
	int oplock = 0;
	int len;

	mh = mq_head_get(qname);
	if(mh == NULL) {
		LM_ERR("mqueue not found: %.*s\n", qname->len, qname->s);
		return -1;
	}

	if(mh->addmode == 1 || mh->addmode == 2) {
		lock_get(&mh->lock);
		oplock = 1;
		miter = mh->ifirst;
		miter_prev = mh->ifirst;
		while(miter) {
			// found mqueue item
			if(miter->key.len == key->len
					&& strncmp(miter->key.s, key->s, key->len) == 0) {
				// mode unique and keep oldest: just return
				if(mh->addmode == 1) {
					lock_release(&mh->lock);
					return 0;
				}

				// mode unique and keep newest: free oldest and further add newest
				if(miter == mh->ifirst && miter == mh->ilast) {
					mh->ifirst = NULL;
					mh->ilast = NULL;
				} else if(miter == mh->ifirst) {
					mh->ifirst = miter->next;
				} else if(miter == mh->ilast) {
					mh->ilast = miter_prev;
					mh->ilast->next = NULL;
				} else {
					miter_prev->next = miter->next;
				}

				shm_free(miter);
				mh->csize--;

				break;
			}
			miter_prev = miter;
			miter = miter->next;
		}
	}

	// mode default
	len = sizeof(mq_item_t) + key->len + val->len + 2;
	mi = (mq_item_t *)shm_malloc(len);
	if(mi == NULL) {
		LM_ERR("no more shm to add to: %.*s\n", qname->len, qname->s);
		if(oplock) {
			lock_release(&mh->lock);
		}
		return -1;
	}
	memset(mi, 0, len);
	mi->key.s = (char *)mi + sizeof(mq_item_t);
	memcpy(mi->key.s, key->s, key->len);
	mi->key.len = key->len;
	mi->key.s[key->len] = '\0';

	mi->val.s = mi->key.s + mi->key.len + 1;
	memcpy(mi->val.s, val->s, val->len);
	mi->val.len = val->len;
	mi->val.s[val->len] = '\0';

	if(oplock == 0) {
		lock_get(&mh->lock);
	}
	if(mh->ifirst == NULL) {
		mh->ifirst = mi;
		mh->ilast = mi;
	} else {
		mh->ilast->next = mi;
		mh->ilast = mi;
	}
	mh->csize++;
	if(mh->msize > 0 && mh->csize > mh->msize) {
		mi = mh->ifirst;
		mh->ifirst = mh->ifirst->next;
		if(mh->ifirst == NULL) {
			mh->ilast = NULL;
		}
		mh->csize--;
		shm_free(mi);
	}
	lock_release(&mh->lock);
	return 0;
}

/**
 *
 */
int pv_parse_mq_name(pv_spec_t *sp, const str *in)
{
	sp->pvp.pvn.u.isname.name.s = *in;
	sp->pvp.pvn.type = PV_NAME_INTSTR;
	sp->pvp.pvn.u.isname.type = 1;
	return 0;
}

/**
 *
 */
int pv_get_mqk(struct sip_msg *msg, pv_param_t *param, pv_value_t *res)
{
	mq_pv_t *mp = NULL;

	if (!param)
		return pv_get_null(msg, param, res);

	if(pv_get_spec_name(msg, param, res)!=0 || (!(res->flags&PV_VAL_STR))) {
		LM_ERR("invalid name\n");
		return -1;
	}

	LM_DBG("Getting key from [%.*s]\n", res->rs.len, res->rs.s);
	if(mq_head_get(&res->rs) == NULL) {
		LM_ERR("mqueue not found: %.*s\n", res->rs.len, res->rs.s);
		return -1;
	}

	mp = mq_pv_get(&res->rs);
	if(mp == NULL || mp->item == NULL || mp->item->key.len <= 0)
		return pv_get_null(msg, param, res);
	return pv_get_strval(msg, param, res, &mp->item->key);
}

/**
 *
 */
str *get_mqk(str *in)
{
	mq_pv_t *mp = NULL;

	if(mq_head_get(in) == NULL) {
		LM_ERR("mqueue not found: %.*s\n", in->len, in->s);
		return NULL;
	}

	mp = mq_pv_get(in);
	if(mp == NULL || mp->item == NULL || mp->item->key.len <= 0)
		return NULL;
	return &mp->item->key;
}

/**
 *
 */
str *get_mqv(str *in)
{
	mq_pv_t *mp = NULL;

	if(mq_head_get(in) == NULL) {
		LM_ERR("mqueue not found: %.*s\n", in->len, in->s);
		return NULL;
	}

	mp = mq_pv_get(in);
	if(mp == NULL || mp->item == NULL || mp->item->val.len <= 0)
		return NULL;
	return &mp->item->val;
}

/**
 *
 */
int pv_get_mqv(struct sip_msg *msg, pv_param_t *param, pv_value_t *res)
{
	mq_pv_t *mp = NULL;

	if (!param)
		return pv_get_null(msg, param, res);

	if(pv_get_spec_name(msg, param, res)!=0 || (!(res->flags&PV_VAL_STR))) {
		LM_ERR("invalid name\n");
		return -1;
	}

	LM_DBG("Getting val from [%.*s]\n", res->rs.len, res->rs.s);
	if(mq_head_get(&res->rs) == NULL) {
		LM_ERR("mqueue not found: %.*s\n", res->rs.len, res->rs.s);
		return -1;
	}

	mp = mq_pv_get(&res->rs);
	if(mp == NULL || mp->item == NULL || mp->item->val.len <= 0)
		return pv_get_null(msg, param, res);
	return pv_get_strval(msg, param, res, &mp->item->val);
}

/**
 *
 */
int pv_get_mq_size(struct sip_msg *msg, pv_param_t *param, pv_value_t *res)
{
	int mqs = -1;

	if (!param)
		return pv_get_null(msg, param, res);

	if(pv_get_spec_name(msg, param, res)!=0 || (!(res->flags&PV_VAL_STR))) {
		LM_ERR("invalid name\n");
		return -1;
	}

	LM_DBG("Getting size of [%.*s]\n", res->rs.len, res->rs.s);
	mqs = _mq_get_csize(&res->rs);

	if(mqs < 0) {
		LM_ERR("mqueue not found: %.*s\n", res->rs.len, res->rs.s);
		return -1;
	}

	return pv_get_sintval(msg, param, res, mqs);
}

/**
 * Return head->csize for a given queue
 */
int _mq_get_csize(str *name)
{
	mq_head_t *mh = mq_head_get(name);
	int mqueue_size = 0;

	if(mh == NULL)
		return -1;

	lock_get(&mh->lock);
	mqueue_size = mh->csize;
	lock_release(&mh->lock);

	return mqueue_size;
}
