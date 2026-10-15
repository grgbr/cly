#include "common.h"
#include <srutils/srepo/schema.h>

void
cly_log_scn(const struct lysc_node * node, const char * format, ...)
{
	cly_assert(node);
	cly_assert(format);
	cly_assert(format[0]);

	char * path;
	char * fmt;
	int    ret;

	path = srepo_sch_node_path(node);
	ret = cly_asprintf(&fmt,
	                   "%s: [path:%s]: %s.\n",
	                   program_invocation_short_name,
	                   path,
	                   format);
	srepo_free(path);
	if (ret > 0) {
		va_list args;

		va_start(args, format);
		vfprintf(stderr, fmt, args);
		va_end(args);

		cly_free(fmt);
	}
	else
		fprintf(stderr,
		        "%s: logging failure: %s.\n",
		        program_invocation_short_name,
		        strerror(-ret));
}

#if defined(CONFIG_CLY_ASSERT)

#include "arg.h"
#include <stdbool.h>

void
cly_assert_args(int argc, const char * const argv[])
{
	cly_assert(argc > 0);
	cly_assert(argv);

	int    a;
	size_t len;

	for (a = 0, len = 0; a < argc; a++) {
		cly_assert(argv[a]);

		size_t alen = strnlen(argv[a], CLY_ARG_MAX);

		cly_assert(alen < CLY_ARG_MAX);

		len += alen;
		cly_assert(len < CLY_LINE_MAX);

		cly_assert(!_cly_arg_isstr_valid(argv[a], alen));
	}
}

#endif /* !defined(CONFIG_CLY_ASSERT) */

void *
cly_realloc(void * data, size_t size)
{
	cly_assert(size);

	data = realloc(data, size);
	if (!data)
		cly_abort();

	return data;
}

char *
cly_strdup(const char * string)
{
	cly_assert(string);

	char * str;

	str = strdup(string);
	if (!str)
		cly_abort();

	return str;
}

const char *
cly_render_string(const char * input, char output[CLY_RENDER_MAX])
{
	cly_assert(output);
	cly_assert(input);

	const char * in = input;
	char *       out = output;

	while ((*in != '\0') && (out < &output[CLY_RENDER_MAX - 1])) {
		if (isprint(*in))
			*out = *in;
		else
			*out = '?';

		in++;
		out++;
	}

	if (*in == '\0')
		*out = '\0';
	else
		memcpy(out - 3, "...", sizeof("..."));

	return output;
}
