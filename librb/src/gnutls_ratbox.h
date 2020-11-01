/*
 *  libratbox: a library used by ircd-ratbox and other things
 *  gnutls_ratbox.h: embedded data for GNUTLS backend
 *
 *  Copyright (C) 2007-2008 ircd-ratbox development team
 *  Copyright (C) 2007-2008 Aaron Sethman <androsyn@ratbox.org>
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
 *
 */

static const char rb_gnutls_default_priority_str[] = ""
	"+SECURE256:"
	"+SECURE128:"
	"!RSA:"
	"+NORMAL:"
	"!ARCFOUR-128:"
	"!3DES-CBC:"
	"!MD5:"
	"VERS-TLS-ALL:"
	"!VERS-SSL3.0:"
	"%SAFE_RENEGOTIATION";
