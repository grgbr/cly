#include "path.h"
#include <stdbool.h>
#include <string.h>

const char *
cly_path_strerror(int error)
{
	switch (error) {
	case EINVAL:
		return "unexpected path component character";
	case ENAMETOOLONG:
		return "path or component too long";
	case ENODATA:
		return "path component empty";
	case ENOBUFS:
		return "not enought buffer space";
	default:
		cly_assert(0);
	}
}

static inline bool
cly_path_iscomp_lead_char(const char * character)
{
	if (isalpha(*character) || (*character == '_'))
		return true;
	else
		return false;
}

static inline bool
cly_path_iscomp_char(const char * character)
{
	cly_assert(character);

	if (isalnum(*character) || (*character == '_') || (*character == '-'))
		return true;
	else
		return false;
}

int
cly_path_comp_isok(const char * component, size_t length)
{
	cly_assert(component);

	if (!length)
		return -ENODATA;
	if (length >= CLY_PATH_NAME_MAX)
		return -ENAMETOOLONG;

	if (cly_path_iscomp_lead_char(component)) {
		const char * ptr = component;

		while (++ptr < &component[length]) {
			if (!cly_path_iscomp_char(ptr))
				return -EINVAL;
		}

		return 0;
	}
	else if (*component == '.') {
		if ((length == 1) ||
		    ((length == 2) && (component[1] == '.')))
			return 0;
	}

	return -EINVAL;
}

int
cly_path_comp_ncmp(const struct cly_path_comp * component,
                   const char *                 string,
                   size_t                       length)
{
	cly_path_assert_comp(component);
	cly_assert(string);
	cly_assert(length < CLY_PATH_NAME_MAX);

	size_t len = component->len;

	if (len != length)
		return len - length;

	return strncmp(component->str, string, len);
}

int
cly_path_comp_cmp(const struct cly_path_comp * component,
                  const char *                 string)
{
	size_t len;

	len = strnlen(string, CLY_PATH_NAME_MAX);
	if (!len)
		return -ENODATA;
	if (len == CLY_PATH_NAME_MAX)
		return -ENAMETOOLONG;

	return cly_path_comp_ncmp(component, string, len);
}

/* Return the kind of a valid (i.e., already parsed) path component. */
enum cly_path_comp_kind
cly_path_comp_kind(const struct cly_path_comp * component)
{
	cly_path_assert_comp(component);

	switch (component->len) {
	case 2:
		if ((component->str[0] == '.') && (component->str[1] == '.'))
			return CLY_PATH_UPPER_COMP_KIND;
		break;

	case 1:
		if (component->str[0] == '.')
			return CLY_PATH_CURR_COMP_KIND;
		break;

	default:
		break;
	}

	return CLY_PATH_REG_COMP_KIND;
}

static size_t
cly_path_length(const struct cly_path * path)
{
	cly_path_assert(path);

	unsigned int                 c;
	unsigned int                 cnt;
	const struct cly_path_comp * comp;
	size_t                       len = 0;

	cly_path_foreach_comp(path, c, cnt, comp) {
		cly_path_assert_comp(comp);

		len += comp->len;
	}

	/*
	 * Return sum of all component lengths + space required for the number
	 * of path delimiter characters.
	 */
	return len ? (len + path->cnt - 1) : 0;
}

/* Append the content of component to string given in argument. */
static char *
cly_path_comp_strcat(const struct cly_path_comp * comp, char * string)
{
	cly_path_assert_comp(comp);
	cly_assert(string);

	memcpy(string, comp->str, comp->len);
	
	return string + comp->len;
}

/* Append the content of the entire path to string given in argument. */
static void
cly_path_strcat(const struct cly_path * path, char * string)
{
	cly_path_assert(path);
	cly_assert(string);

	char * str = string;

	if (path->cnt) {
		unsigned int                 c = path->head;
		unsigned int                 cnt = path->cnt;
		const struct cly_path_comp * comp;

		str = cly_path_comp_strcat(&path->comps[c], str);
		cly_path_foreach_comp_continue(path, c, cnt, comp) {
			*(str++) = '/';
			str = cly_path_comp_strcat(comp, str);
		}
	}

	*str = '\0';
}

ssize_t
cly_path_mkrel(struct cly_path * path, char * string, size_t size)
{
	cly_path_assert(path);
	cly_assert(string);
	cly_assert(size);
	cly_assert(size <= CLY_PATH_MAX);

	size_t len;

	len = cly_path_length(path);
	if (len >=  size)
		return -ENOBUFS;

	cly_path_strcat(path, string);

	return len;
}

ssize_t
cly_path_mkabs(struct cly_path * path, char * string, size_t size)
{
	cly_path_assert(path);
	cly_assert(string);
	cly_assert(size);
	cly_assert(size <= CLY_PATH_MAX);

	size_t len;

	len = 1 + cly_path_length(path);
	if (len >= size)
		return -ENOBUFS;

	string[0] = '/';
	cly_path_strcat(path, &string[1]);

	return len;
}

static const struct cly_path_comp *
cly_path_peek_tail(const struct cly_path * path)
{
	cly_path_assert(path);

	if (path->cnt) {
		struct cly_path_comp * comp;

		comp = &path->comps[(path->head + path->cnt - 1) % path->nr];
		cly_path_assert_comp(comp);

		return comp;
	}
	else
		return NULL;
}

static inline void
cly_path_grow(struct cly_path * path)
{
	cly_path_assert(path);

	if (path->cnt == path->nr) {
		struct cly_path_comp * comps;
		unsigned int           hcnt;  /* Count of head components. */
		unsigned int           tcnt;  /* Count of tail components. */

		hcnt = (path->nr - path->head);
		if (hcnt > path->cnt)
			hcnt = path->cnt;
		tcnt = path->cnt - hcnt;

		path->nr *= 2;
		comps = cly_malloc(path->nr * sizeof(path->comps[0]));

		/*
		 * Copy leading components first starting from the begining of
		 * the new memory area. Then, append the trailing components
		 * right after the leading components.
		 */
		memcpy(comps,
		       &path->comps[path->head],
		       hcnt * sizeof(comps[0]));
		if (tcnt)
			memcpy(&comps[hcnt],
			       &path->comps[0],
			       tcnt * sizeof(comps[0]));

		path->head = 0;

		cly_free(path->comps);
		path->comps = comps;
	}
}

void
cly_path_push_head(struct cly_path * path,
                   const char *      component,
                   size_t            length)
{
	cly_path_assert(path);
	cly_assert(component);
	cly_assert(length);
	cly_assert(!cly_path_comp_isok(component, length));

	unsigned int           head;
	struct cly_path_comp * comp;

	cly_path_grow(path);

	head = (path->head + path->nr - 1) % path->nr;

	comp = &path->comps[head];
	comp->str = component;
	comp->len = length;

	path->head = head;
	path->cnt++;
}

const struct cly_path_comp *
cly_path_pop_head(struct cly_path * path)
{
	cly_path_assert(path);

	if (path->cnt) {
		const struct cly_path_comp * comp;

		comp = &path->comps[path->head];

		path->head = (path->head + 1) % path->nr;
		path->cnt--;

		return comp;
	}
	else
		return NULL;
}

void
cly_path_push_tail(struct cly_path * path,
                   const char *      component,
                   size_t            length)
{
	cly_path_assert(path);
	cly_assert(component);
	cly_assert(length);
	cly_assert(!cly_path_comp_isok(component, length));

	unsigned int           tail;
	struct cly_path_comp * comp;

	cly_path_grow(path);

	tail = (path->head + path->cnt) % path->nr;

	comp = &path->comps[tail];
	comp->str = component;
	comp->len = length;

	path->cnt++;
}

const struct cly_path_comp *
cly_path_pop_tail(struct cly_path * path)
{
	cly_path_assert(path);

	if (path->cnt) {
		const struct cly_path_comp * comp;

		comp = &path->comps[(path->head + path->cnt - 1) % path->nr];

		path->cnt--;

		return comp;
	}
	else
		return NULL;
}

static size_t
cly_path_skip_delim(const char * string, size_t size)
{
	cly_assert(string);
	cly_assert(size);
	cly_assert(size < CLY_PATH_MAX);

	const char * str = string;

	do {
		if (*str != '/')
			break;
	} while (++str < &string[size]);

	return (size_t)(str - string);
}

ssize_t
cly_path_parse_comp(enum cly_path_comp_kind * kind,
                    const char *              string,
                    size_t                    size)
{
	cly_assert(kind);
	cly_assert(string);
	cly_assert(size);

	const char * str = string;
	const char * end = &string[size];

	if (cly_path_iscomp_lead_char(str)) {
		while (++str < end) {
			if (!cly_path_iscomp_char(str))
				break;
		}

		if ((str == end) || (*str == '/') || (*str == '\0')) {
			*kind = CLY_PATH_REG_COMP_KIND;
			return (ssize_t)(str - string);
		}
	}
	else if (*str == '.') {
		if ((size == 1) || (str[1] == '/') || (str[1] == '\0')) {
			*kind = CLY_PATH_CURR_COMP_KIND;
			return 1;
		}
		else if ((str[1] == '.') &&
		    ((size == 2) || (str[2] == '/') || (str[2] == '\0'))) {
			*kind = CLY_PATH_UPPER_COMP_KIND;
			return 2;
		}
	}
	else if (*str == '\0')
		return 0;

	return -EINVAL;
}

static void
cly_push_upper_comp_tail(struct cly_path * path,
                         const char *      string,
                         size_t            length,
                         bool              abspath)
{
	const struct cly_path_comp * last;

	last = cly_path_peek_tail(path);

	if (!abspath) {
		/*
		 * We are parsing a relative path.
		 * If a previously (upper) parsed regular component exists, pop
		 * it out.
		 * Else, push the "upper directory" component along the existing
		 * path.
		 */
		if (last &&
		    (cly_path_comp_kind(last) == CLY_PATH_REG_COMP_KIND))
			path->cnt--;
		else
			cly_path_push_tail(path, string, length);
	}
	else {
		/*
		 * We are parsing an absolute path.
		 * If a previously (upper) parsed regular component exists, pop
		 * it out.
		 * In any other cases, no need to push an "upper directory"
		 * component since we are already at the root level.
		 */
		if (last) {
		    cly_assert(cly_path_comp_kind(last) ==
		               CLY_PATH_REG_COMP_KIND);
			path->cnt--;
		}
	}
}

static int
_cly_path_parse(struct cly_path * path,
                const char *      string,
                size_t            size,
                bool              abspath)
{
	cly_path_assert(path);
	cly_assert(string);
	cly_assert(size);
	cly_assert(size < CLY_PATH_MAX);
	cly_assert(strnlen(string, CLY_PATH_MAX) == (size - 1));

	const char * str = string;

	do {
		ssize_t                 len;
		enum cly_path_comp_kind kind;

		len = (size_t)cly_path_skip_delim(str, size);
		str += len;
		size -= len;
		if (!size)
			break;

		len = cly_path_parse_comp(&kind, str, size);
		if (len <= 0)
			return len;

		switch (kind) {
		case CLY_PATH_REG_COMP_KIND:
			cly_path_push_tail(path, str, len);
			break;

		case CLY_PATH_UPPER_COMP_KIND:
			cly_push_upper_comp_tail(path, str, len, abspath);
			break;

		case CLY_PATH_CURR_COMP_KIND:
			break;

		default:
			cly_assert(0);
		}

		str += len;
		size -= len;
	} while (size);

	return 0;
}

int
cly_path_parse(struct cly_path * path, const char * string)
{
	cly_path_assert(path);
	cly_assert(string);

	size_t len;

	len = strnlen(string, CLY_PATH_MAX);
	if (len == CLY_PATH_MAX)
		return -ENAMETOOLONG;

	return _cly_path_parse(path, string, len + 1, *string == '/');
}

void
cly_path_init(struct cly_path * path)
{
	cly_assert(path);

	path->nr = CLY_PATH_INIT_NR;
	path->head = 0;
	path->cnt = 0;
	path->comps = cly_malloc(CLY_PATH_INIT_NR * sizeof(path->comps[0]));
}

void
cly_path_fini(struct cly_path * path)
{
	cly_path_assert(path);

	cly_free(path->comps);
}

struct cly_path *
cly_path_create(void)
{
	struct cly_path * path;

	path = cly_malloc(sizeof(*path));
	cly_assert(path);

	cly_path_init(path);

	return path;
}

void
cly_path_destroy(struct cly_path * path)
{
	cly_path_assert(path);

	cly_path_fini(path);
	cly_free(path);
}

ssize_t
cly_path_normalize(const char * path, char * norm, size_t size)
{
	cly_assert(path);
	cly_assert(norm);
	cly_assert(size);
	cly_assert(size <= CLY_PATH_MAX);

	if (*path != '\0') {
		struct cly_path pth;
		ssize_t         len;

		cly_path_init(&pth);

		len = cly_path_parse(&pth, path);
		if (len)
			goto out;

		if (*path != '/')
			len = cly_path_mkrel(&pth, norm, size);
		else
			len = cly_path_mkabs(&pth, norm, size);

out:
		cly_path_fini(&pth);

		return (int)len;
	}
	else {
		norm[0] = '\0';

		return 0;
	}
}

int
cly_path_isok(const char * path)
{
	cly_assert(path);
	cly_assert(path[0] != '\0');

	struct cly_path pth;
	int             ret;

	cly_path_init(&pth);
	ret = cly_path_parse(&pth, path);
	cly_path_fini(&pth);

	return ret;
}
