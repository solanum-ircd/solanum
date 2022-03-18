#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "tap/basic.h"

#include "s_newconf.h"

#define MSG "%s:%d (%s)", __FILE__, __LINE__, __FUNCTION__

#define MINUTE (60)
#define HOUR (MINUTE * 60)
#define DAY (HOUR * 24)
#define WEEK (DAY * 7)

static void valid_temp_time1(void)
{
	time_t t;
	t = valid_temp_time("1");
	is_int(MINUTE, t, MSG);
	t = valid_temp_time("1m");
	is_int(MINUTE, t, MSG);
	t = valid_temp_time("1h");
	is_int(HOUR, t, MSG);
	t = valid_temp_time("1d");
	is_int(DAY, t, MSG);
	t = valid_temp_time("1w");
	is_int(WEEK, t, MSG);

	t = valid_temp_time("2d");
	is_int(2 * DAY, t, MSG);

	t = valid_temp_time("1w2d3h4m");
	is_int(1 * WEEK + 2 * DAY + 3 * HOUR + 4 * MINUTE, t, MSG);
	t = valid_temp_time("1w2d3h4");
	is_int(1 * WEEK + 2 * DAY + 3 * HOUR + 4 * MINUTE, t, MSG);

	t = valid_temp_time("4m3h2d1w");
	is_int(1 * WEEK + 2 * DAY + 3 * HOUR + 4 * MINUTE, t, MSG);

	t = valid_temp_time("7000w");
	is_int(52 * WEEK, t, MSG);
}

static void valid_temp_time_invalid(void)
{
	time_t t;
	t = valid_temp_time("-2w");
	is_int(-1, t, MSG);

	t = valid_temp_time("hello");
	is_int(-1, t, MSG);

	t = valid_temp_time("m");
	is_int(-1, t, MSG);

	t = valid_temp_time("1w-1w");
	is_int(-1, t, MSG);
}

static void valid_temp_time_overflow(void)
{
	time_t max_time = (uintmax_t) (~(time_t)0) >> 1;
	char s[100];
	time_t t;

	snprintf(s, sizeof s, "%" PRIuMAX "m", (uintmax_t) max_time / 60 + 2);
	t = valid_temp_time(s);
	is_int(52 * WEEK, t, MSG);

	snprintf(s, sizeof s, "%" PRIuMAX "m%" PRIuMAX "m", (uintmax_t) max_time / 60 - 1, (uintmax_t) max_time / 60 - 1);
	t = valid_temp_time(s);
	is_int(52 * WEEK, t, MSG);
}

int main(int argc, char *argv[])
{
	plan_lazy();

	valid_temp_time1();
	valid_temp_time_invalid();
	valid_temp_time_overflow();

	return 0;
}
