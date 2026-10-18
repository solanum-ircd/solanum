/*
 *  hostmask1.c: Test parse_netmask
 *  Copyright 2020 Ed Kellett
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
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "tap/basic.h"

#include "stdinc.h"
#include "ircd_defs.h"
#include "client.h"
#include "hostmask.h"

#define MSG "%s:%d (%s)", __FILE__, __LINE__, __func__

struct Client me;

static void plain_hostmask(void)
{
	int ty;

	ty = parse_netmask("foo.example", NULL, NULL);
	is_int(HM_HOST, ty, MSG);

	ty = parse_netmask("*.example", NULL, NULL);
	is_int(HM_HOST, ty, MSG);

	ty = parse_netmask("foo.examp?", NULL, NULL);
	is_int(HM_HOST, ty, MSG);
}

static void valid_ipv4(void)
{
	int ty, nb;
	struct rb_sockaddr_storage addr;
	char ip[1024];

	ty = parse_netmask("10.20.30.40", &addr, &nb);
	is_int(HM_IPV4, ty, MSG);
	if (ty == HM_IPV4)
	{
		is_string("10.20.30.40", rb_inet_ntop_sock((struct sockaddr *)&addr, ip, sizeof ip), MSG);
		is_int(32, nb, MSG);
	}
}

static void invalid_ipv4(void)
{
	int ty;

	ty = parse_netmask(".1", NULL, NULL);
	is_int(HM_HOST, ty, MSG);

	ty = parse_netmask("10.20.30", NULL, NULL);
	is_int(HM_HOST, ty, MSG);

	ty = parse_netmask("10.20.30.40.50", NULL, NULL);
	is_int(HM_HOST, ty, MSG);
}

static void valid_ipv4_cidr(void)
{
	int ty, nb;
	struct rb_sockaddr_storage addr;
	char ip[1024];

	ty = parse_netmask("10.20.30.40/7", &addr, &nb);
	is_int(HM_IPV4, ty, MSG);
	if (ty == HM_IPV4)
	{
		is_string("10.20.30.40", rb_inet_ntop_sock((struct sockaddr *)&addr, ip, sizeof ip), MSG);
		is_int(7, nb, MSG);
	}
}

static void invalid_ipv4_cidr(void)
{
	int ty, nb;

	ty = parse_netmask("10.20.30.40/aaa", NULL, NULL);
	is_int(HM_HOST, ty, MSG);

	ty = parse_netmask("10.20.30.40/-1", NULL, NULL);
	is_int(HM_HOST, ty, MSG);

	ty = parse_netmask("10.20.30.40/1000", NULL, &nb);
	is_int(HM_IPV4, ty, MSG);
	is_int(32, nb, MSG);

	ty = parse_netmask("10.20.30.40/1a", NULL, &nb);
	is_int(HM_IPV4, ty, MSG);
	is_int(32, nb, MSG);

	ty = parse_netmask_strict("10.20.30.40/1000", NULL, NULL);
	is_int(HM_ERROR, ty, MSG);

	ty = parse_netmask_strict("10.20.30.40/1a", NULL, NULL);
	is_int(HM_ERROR, ty, MSG);
}

static void valid_ipv6(void)
{
	int ty, nb;
	struct rb_sockaddr_storage addr;
	char ip[1024];

	ty = parse_netmask("1:2:3:4:5:6:7:8", &addr, &nb);
	is_int(HM_IPV6, ty, MSG);
	if (ty == HM_IPV6)
	{
		is_string("1:2:3:4:5:6:7:8", rb_inet_ntop_sock((struct sockaddr *)&addr, ip, sizeof ip), MSG);
		is_int(128, nb, MSG);
	}

	ty = parse_netmask("1:2::7:8", &addr, &nb);
	is_int(HM_IPV6, ty, MSG);
	if (ty == HM_IPV6)
	{
		is_string("1:2::7:8", rb_inet_ntop_sock((struct sockaddr *)&addr, ip, sizeof ip), MSG);
		is_int(128, nb, MSG);
	}
}

static void invalid_ipv6(void)
{
	int ty;

	ty = parse_netmask(":1", NULL, NULL);
	is_int(HM_HOST, ty, MSG);

	ty = parse_netmask("1:2:3:4:5", NULL, NULL);
	is_int(HM_HOST, ty, MSG);

	ty = parse_netmask("1:2:3:4:5:6:7:8:9", NULL, NULL);
	is_int(HM_HOST, ty, MSG);
}

static void valid_ipv6_cidr(void)
{
	int ty, nb;
	struct rb_sockaddr_storage addr;
	char ip[1024];

	ty = parse_netmask("1:2:3:4:5:6:7:8/96", &addr, &nb);
	is_int(HM_IPV6, ty, MSG);
	if (ty == HM_IPV6)
	{
		is_string("1:2:3:4:5:6:7:8", rb_inet_ntop_sock((struct sockaddr *)&addr, ip, sizeof ip), MSG);
		is_int(96, nb, MSG);
	}
}

static void invalid_ipv6_cidr(void)
{
	int ty, nb;

	ty = parse_netmask("1:2::7:8/aaa", NULL, NULL);
	is_int(HM_HOST, ty, MSG);

	ty = parse_netmask("1:2::7:8/-1", NULL, NULL);
	is_int(HM_HOST, ty, MSG);

	ty = parse_netmask("1:2::7:8/1000", NULL, &nb);
	is_int(HM_IPV6, ty, MSG);
	is_int(128, nb, MSG);

	ty = parse_netmask("1:2::7:8/1a", NULL, &nb);
	is_int(HM_IPV6, ty, MSG);
	is_int(128, nb, MSG);

	ty = parse_netmask_strict("1:2::7:8/1000", NULL, NULL);
	is_int(HM_ERROR, ty, MSG);

	ty = parse_netmask_strict("1:2::7:8/1a", NULL, NULL);
	is_int(HM_ERROR, ty, MSG);
}

int main(int argc, char *argv[])
{
	memset(&me, 0, sizeof(me));
	strcpy(me.name, "me.name.");

	rb_lib_init(NULL, NULL, NULL, 0, 1024, DNODE_HEAP_SIZE, FD_HEAP_SIZE);
	rb_linebuf_init(LINEBUF_HEAP_SIZE);

	plan_lazy();

	plain_hostmask();

	valid_ipv4();
	invalid_ipv4();
	valid_ipv4_cidr();
	invalid_ipv4_cidr();

	valid_ipv6();
	invalid_ipv6();
	valid_ipv6_cidr();
	invalid_ipv6_cidr();

	return 0;
}
