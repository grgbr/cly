#include "expr.h"
#include "arg.h"

/******************************************************************************
 * Expression utilities
 ******************************************************************************/

static ssize_t
cly_expr_unquote_arg(char ** argument, size_t length)
{
	cly_assert(argument);
	cly_assert(*argument);
	cly_assert(length);
	cly_assert(strnlen(*argument, CLY_ARG_MAX) == length);

	char * arg = *argument;
	
	if ((arg[0] != '"') && (arg[0] != '\''))
		return (ssize_t)length;

	cly_assert(arg[length - 1] == arg[0]);
	arg[length - 1] = '\0';
	*argument = &arg[1];

	return length - 2;
}

/******************************************************************************
 * Base command line expression handling.
 ******************************************************************************/

#define CLY_EXPR_ARGS_INIT (4U)

static int
cly_expr_push_arg(struct cly_expr * expression,
                  char *            argument,
                  size_t            length)
{
	cly_expr_assert(expression);
	cly_assert(argument);
	cly_assert(length);

	char * arg;
	size_t inval;
	char   rnd[CLY_RENDER_MAX];

	if (length >= CLY_ARG_MAX) {
		cly_talk("cannot parse: '%s': expression argument too long.",
		         cly_render_string(argument, rnd));
		return -ENAMETOOLONG;
	}

	cly_assert(strnlen(argument, CLY_ARG_MAX) == length);

	arg = argument;
	length = cly_expr_unquote_arg(&arg, length);
	inval = _cly_arg_isstr_valid(arg, length);
	if (inval) {
		cly_talk("cannot parse: '%s': "
		         "invalid expression argument character '%c'.",
		         cly_render_string(arg, rnd),
		         cly_render_chr(arg[inval]));

		return -EINVAL;
	}

	if (expression->cnt == expression->nr) {
		unsigned int nr = expression->nr * 2;

		if (nr >= CLY_EXPR_ARGS_MAX) {
			cly_talk("cannot parse: "
			         "too many expression arguments.");
			return -ENOBUFS;
		}

		expression->nr = nr;
		expression->args = cly_realloc(expression->args,
		                               nr * sizeof(argument));
	}

	expression->args[expression->cnt++] = arg;

	return 0;
}

int
cly_expr_parse_string(struct cly_expr * expression, char * string)
{
	cly_expr_assert(expression);
	cly_assert(string);
	cly_assert(*string != '\0');
	cly_assert(strnlen(string, CLY_LINE_MAX) < CLY_LINE_MAX);
	cly_assert(!strchr(string, '\n'));

	char * str = string;
	int    ret;

	do {
		char * tok;

		str += strspn(str, " \t");
		if (*str == '\0')
			return 0;

		tok = str;
		str += strcspn(str, " \t\"'");
		if ((*str == '"') || (*str == '\'')) {
			/* Search for closing delimiter. */
			char * delim;

			delim = strchr(str + 1, *str);
			if (!delim) {
				char rnd[CLY_RENDER_MAX];

				cly_talk("cannot parse: '%s': "
				         "missing '%c' closing delimiter.",
				         cly_render_string(str, rnd),
				         *str);
				return -EINVAL;
			}

			str = delim + 1;
		}

		switch (*str) {
		case ' ':
		case '\t':
			*str = '\0';
			ret = cly_expr_push_arg(expression, tok, str - tok);
			if (ret)
				return ret;
			str++;
			break;

		case '\0':
			return cly_expr_push_arg(expression, tok, str - tok);

		default:
			return -EINVAL;
		}
	} while (*str != '\0');

	return 0;
}

#if 0
/* Keep this just in case we need to save parsed line into history. */
static ssize_t
cly_expr_make_string(const struct cly_expr * expression,
                     char *                  string,
                     size_t                  size)
{
	cly_expr_assert(expression);
	cly_assert(string);
	cly_assert(size);
	cly_assert(size <= SSIZE_MAX);

	ssize_t      tlen; /* Total length. */
	unsigned int a;

	tlen = strlen(expression->args[0]);
	cly_assert(tlen < CLY_ARG_MAX);
	if ((size_t)tlen >= size)
		return -ENOBUFS;
	memcpy(string, expression->args[0], tlen);

	for (a = 1; a < expression->cnt; a++) {
		const char * arg = expression->args[a]; /* Argument to join. */
		size_t       alen;                      /* Argument length. */

		alen = strlen(arg);
		cly_assert(alen < CLY_ARG_MAX);
		if (((size_t)tlen + 1 + alen) >= size)
			return -ENOBUFS;

		string[tlen++] = ' ';
		memcpy(&string[tlen], arg, alen);

		tlen += alen;
		cly_assert((size_t)tlen < size);
	}

	string[tlen] = '\0';

	return tlen;
}
#endif

void
cly_expr_init(struct cly_expr * expression)
{
	cly_assert(expression);

	expression->next = NULL;
	expression->cnt = 0;
	expression->nr = CLY_EXPR_ARGS_INIT;
	expression->args = cly_malloc(CLY_EXPR_ARGS_INIT *
	                              sizeof(expression->args[0]));
	cly_assert(expression->args);
}

static struct cly_expr *
cly_expr_create(void)
{
	struct cly_expr * expr;

	expr = cly_malloc(sizeof(*expr));
	cly_assert(expr);

	cly_expr_init(expr);

	return expr;
}

void
cly_expr_fini(struct cly_expr * expression)
{
	cly_expr_assert(expression);

	cly_free(expression->args);
}

static void
cly_expr_destroy(struct cly_expr * expression)
{
	cly_expr_assert(expression);

	cly_expr_fini(expression);

	cly_free(expression);
}

static int
cly_expr_createn_parse(struct cly_expr ** expression, char * string)
{
	cly_assert(expression);
	cly_assert(string);

	struct cly_expr * expr;
	int               ret;

	expr = cly_expr_create();
	cly_assert(expr);

	ret = cly_expr_parse_string(expr, string);
	if (ret) {
		cly_expr_destroy(expr);
		return ret;
	}

	*expression = expr;

	return 0;
}

/******************************************************************************
 * Command line expression block / sequence handling.
 ******************************************************************************/

static void
cly_expr_blk_push(struct cly_expr_blk * block, struct cly_expr * expression)
{
	cly_expr_blk_assert(block);
	cly_expr_assert(expression);
	cly_assert(!expression->next);
	cly_assert(block->cnt < CLY_EXPR_BLK_MAX);

	block->cnt++;
	*block->tail = expression;
	block->tail = &expression->next;
}

int
cly_expr_blk_parse_line(struct cly_expr_blk * block, char * line)
{
	cly_expr_blk_assert(block);
	cly_assert(!block->line);
	cly_assert(line);
	cly_assert(*line != '\0');
	cly_assert(strnlen(line, CLY_LINE_MAX) < CLY_LINE_MAX);
	cly_assert(!strchr(line, '\n'));

	char * ln = line;

	do {
		char *            str;
		struct cly_expr * expr;
		int               ret;

		ln += strspn(ln, " \t;");
		if (*ln == '\0')
			break;

		if (block->cnt == CLY_EXPR_BLK_MAX) {
			cly_talk("cannot parse: too many expressions.");
			return -ENOBUFS;
		}

		str = strsep(&ln, ";");
		cly_assert(str);
		cly_assert(*str != '\0');

		ret = cly_expr_createn_parse(&expr, str);
		if (ret)
			return ret;

		cly_expr_blk_push(block, expr);
	} while (ln);

	if (!block->cnt)
		return -ENODATA;

	block->line = line;

	return 0;
}

#if 0
/* Keep this just in case we need to save parsed line into history. */
ssize_t
cly_expr_blk_make_string(const struct cly_expr_blk * block,
                         char *                      string,
                         size_t                      size)
{
	cly_expr_blk_assert(block);
	cly_assert(block->cnt);
	cly_assert(string);
	cly_assert(size);
	cly_assert(size <= SSIZE_MAX);

	const struct cly_expr * expr = block->head;
	ssize_t                 len = 0;

	len = cly_expr_make_string(expr, string, size);
	cly_assert(len);
	if (len < 0)
		return len;

	for (expr = expr->next; expr; expr = expr->next) {
		cly_assert((size_t)len < size);

		ssize_t ret;

		if (((size_t)len + 3) >= size)
			/*
			 * No more room to store at least a semicolon followed
			 * by a space and a single character.
			 */
			return -ENOBUFS;

		string[len++] = ';';
		string[len++] = ' ';

		ret = cly_expr_make_string(expr, &string[len], size - len);
		cly_assert(ret);
		if (ret < 0)
			return ret;

		len += ret;
	}

	cly_assert(string[len] == '\0');

	return len;
}
#endif

void
cly_expr_blk_init(struct cly_expr_blk * block)
{
	cly_assert(block);

	block->cnt = 0;
	block->head = NULL;
	block->tail = &block->head;
	block->line = NULL;
}

void
cly_expr_blk_fini(struct cly_expr_blk * block)
{
	cly_expr_blk_assert(block);

	struct cly_expr * expr;
	struct cly_expr * tmp;

	cly_expr_blk_foreach_safe(block, expr, tmp)
		cly_expr_destroy(expr);

	cly_free(block->line);
}
