#include "match.h"
#include "arg.h"
#include <string.h>

#define CLY_MATCH_INIT_NR (4U)

void
cly_match_push(struct cly_match * matches, char * string)
{
	cly_match_assert(matches);
	cly_assert(!string || (string[0] != '\0'));
	cly_assert(!string || (strnlen(string, CLY_MATCH_MAX) < CLY_MATCH_MAX));

	if (matches->cnt >= matches->nr) {
		unsigned int nr = matches->nr ? matches->nr * 2
		                              : CLY_MATCH_INIT_NR;

		if (nr >= CLY_MATCH_MAX)
			/*
			 * Since we are in the middle of a completion process,
			 * silently ignore unexpected loads of matches...
			 */
			return;

		matches->data = cly_realloc(matches->data,
		                            nr * sizeof(matches->data[0]));
		cly_assert(matches->data);

		matches->nr = nr;
	}

	matches->data[matches->cnt++] = string;
}

void
cly_match_init(struct cly_match * matches)
{
	cly_assert(matches);

	matches->cnt = 0;
	matches->nr = 0;
	matches->data = NULL;
}
