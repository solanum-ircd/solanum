/*
 *  wsproc.h: An interface to the solanum websocket helper daemon
 *  Copyright (C) 2007 Aaron Sethman <androsyn@ratbox.org>
 *  Copyright (C) 2007 ircd-ratbox development team
 *  Copyright (C) 2016 Ariadne Conill <ariadne@dereferenced.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 *  USA
 */

#ifndef INCLUDED_wsproc_h
#define INCLUDED_wsproc_h

struct ws_ctl;
typedef struct ws_ctl ws_ctl_t;

enum wsockd_status {
	WSOCKD_ACTIVE,
	WSOCKD_SHUTDOWN,
	WSOCKD_DEAD,
};

void init_wsockd(void);
int start_wsockd(int count);
ws_ctl_t *start_wsockd_accept(rb_fde_t *wsF, rb_fde_t *plainF, uint32_t id);
void wsockd_decrement_clicount(ws_ctl_t *ctl);
int get_wsockd_count(void);

#endif

