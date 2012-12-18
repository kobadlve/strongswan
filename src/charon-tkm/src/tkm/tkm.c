/*
 * Copyright (C) 2012 Reto Buerki
 * Copyright (C) 2012 Adrian-Ken Rueegsegger
 * Hochschule fuer Technik Rapperswil
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.  See <http://www.fsf.org/copyleft/gpl.txt>.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 */

#include <daemon.h>

#include <tkm/client.h>
#include <tkm/constants.h>

#include "tkm.h"

#define IKE_SOCKET "/tmp/tkm.rpc.ike"
#define EES_SOCKET "/tmp/tkm.rpc.ees"

typedef struct private_tkm_t private_tkm_t;

extern result_type ees_server_init(const char * const address);
extern void ees_server_finalize(void);
extern void ehandler_init(void);

/*
 * Private additions to tkm_t.
 */
struct private_tkm_t {

	/**
	 * Public members of tkm_t.
	 */
	tkm_t public;
};

/**
 * Single instance of tkm_t.
 */
tkm_t *tkm = NULL;

/**
 * Described in header.
 */
bool tkm_init()
{
	private_tkm_t *this;

	active_requests_type max_requests;
	nc_id_type nc;
	dh_id_type dh;
	cc_id_type cc;
	ae_id_type ae;
	isa_id_type isa;
	esa_id_type esa;

	/* initialize TKM client library */
	tkmlib_init();
	ehandler_init();

	const char * const ikesock = lib->settings->get_str(lib->settings,
														"%s.ike_socket",
														IKE_SOCKET,
														charon->name);
	if (ike_init(ikesock) != TKM_OK)
	{
		tkmlib_final();
		return FALSE;
	}
	DBG1(DBG_DMN, "connected to TKM via socket '%s'", ikesock);

	const char * const eessock = lib->settings->get_str(lib->settings,
														"%s.ees_socket",
														EES_SOCKET,
														charon->name);
	ees_server_init(eessock);
	DBG1(DBG_DMN, "serving EES requests on socket '%s'", eessock);

	if (ike_tkm_reset() != TKM_OK)
	{
		ees_server_finalize();
		tkmlib_final();
		return FALSE;
	}

	/* get limits from tkm */
	if (ike_tkm_limits(&max_requests, &nc, &dh, &cc, &ae, &isa, &esa) != TKM_OK)
	{
		ees_server_finalize();
		tkmlib_final();
		return FALSE;
	}

	const tkm_limits_t limits = {nc, dh, cc, isa, ae, esa};

	INIT(this,
		.public = {
			.idmgr = tkm_id_manager_create(limits),
			.chunk_map = tkm_chunk_map_create(),
		},
	);
	tkm = &this->public;

	return TRUE;
}

/**
 * Described in header.
 */
void tkm_deinit()
{
	if (!tkm)
	{
		return;
	}
	private_tkm_t *this = (private_tkm_t*)tkm;
	this->public.idmgr->destroy(this->public.idmgr);
	this->public.chunk_map->destroy(this->public.chunk_map);

	ees_server_finalize();

	tkmlib_final();
	free(this);
	tkm = NULL;
}
