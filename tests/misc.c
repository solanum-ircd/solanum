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

	t = valid_temp_time("3000w");
	is_int(52 * WEEK, t, MSG);
}

int main(int argc, char *argv[])
{
	plan_lazy();

	valid_temp_time1();

	return 0;
}
