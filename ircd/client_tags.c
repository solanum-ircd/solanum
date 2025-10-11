/*
 *  Solanum: a slightly advanced ircd
 *  client_tags.c: client tags support
 *
 * Copyright (C) 2022 Ryan Lahfa
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1.Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 * 2.Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * 3.The name of the author may not be used to endorse or promote products
 *   derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "stdinc.h"
#include "client_tags.h"

static struct client_tag_support supported_client_tags[MAX_CLIENT_TAGS];
static int num_client_tags = 0;
static int max_client_tags = MAX_CLIENT_TAGS;

int
add_client_tag(const char *name)
{
	if (num_client_tags >= max_client_tags)
		return -1;

	strcpy(supported_client_tags[num_client_tags].name, name);
	num_client_tags++;

	return num_client_tags - 1;
}

void
remove_client_tag(const char *name)
{
	for (int index = 0; index < num_client_tags; index++)
	{
		if (!strcmp(supported_client_tags[index].name, name)) {
			if (index < num_client_tags - 1)
				strcpy(supported_client_tags[index].name, supported_client_tags[num_client_tags - 1].name);
			num_client_tags--;
			break;
		}
	}
}

void
format_client_tags(char *dst, size_t dst_sz, const char *individual_fmt, const char *join_sep)
{
	size_t start = 0;
	size_t join_len = strlen(join_sep);
	*dst = 0;
	for (size_t index = 0; index < num_client_tags; index++) {
		if (start >= dst_sz)
			break;

		if (index > 0) {
			if (start + join_len >= dst_sz)
				break;

			strcpy(dst + start, join_sep);
			start += join_len;
		}
		start += snprintf((dst + start), dst_sz - start, individual_fmt, supported_client_tags[index].name);
	}
}
