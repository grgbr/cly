#include "common.h"
#include <srutils/srepo/schema.h>

#if defined(CONFIG_CLY_LOG)

static struct elog_stdio cly_logger;

static void
cly_srlog_cb(sr_log_level_t level, const char * message)
{
	cly_assert(message);

	enum elog_severity svrt;

	switch (level) {
	case SR_LL_ERR:
		svrt = ELOG_ERR_SEVERITY;
		break;
	case SR_LL_WRN:
		svrt = ELOG_WARNING_SEVERITY;
		break;
	case SR_LL_INF:
		svrt = ELOG_NOTICE_SEVERITY;
		break;
	case SR_LL_VRB:
		svrt = ELOG_INFO_SEVERITY;
		break;
	case SR_LL_DBG:
		svrt = ELOG_DEBUG_SEVERITY;
		break;
	default:
		cly_assert(0);
	}

	elog_log((struct elog *)&cly_logger, svrt, "%s", message);
}

void
cly_init_log(enum elog_severity severity)
{
	struct elog_stdio_conf conf = {
		.super.severity = CONFIG_CLY_LOG_SEVERITY_VALUE,
		.format         = ELOG_TAG_FMT
	};

	/* Setup our internal logger. */
	conf.super.severity = severity;
	elog_init_stdio(&cly_logger, &conf);

	/* Disable all internal sysrepo / libyang logging messages... */
	sr_log_stderr(SR_LL_NONE);

	/*
	 * Install our logging callback. It will always be called for
	 * every messages, regardless of any log level.
	 */
	sr_log_set_cb(cly_srlog_cb);

	srepo_log_setup((struct elog *)&cly_logger);
}

void
cly_fini_log(void)
{
	elog_fini_stdio(&cly_logger);
}

#else  /* !defined(CONFIG_CLY_LOG) */

void
cly_init_log(enum elog_severity severity)
{
	/* Disable sysrepo / libyang logging entirely. */
	sr_log_set_cb(NULL);

	srepo_log_setup(NULL);
}

#endif /* defined(CONFIG_CLY_LOG) */

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
