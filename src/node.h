#ifndef _CLY_NODE_H
#define _CLY_NODE_H

#include "common.h"
#include <stdbool.h>

struct cly_node {
	struct cly_node * next;
	struct cly_node * prev;
	struct cly_node * child;
	struct cly_node * parent;
};

#define cly_node_assert(_node) \
	cly_assert(_node)

#define cly_node_foreach_sibling(_head, _sib) \
	for (_sib = _head; _sib; _sib = (_sib)->next)

#define cly_node_foreach_sibling_safe(_head, _sib, _tmp) \
	for (_sib = _head; \
	     (_sib) && (_tmp = (_sib)->next, 1); \
	     _sib = (_tmp))

#define cly_node_foreach_child(_node, _child) \
	cly_node_foreach_sibling((_node)->child, _child)

#define cly_node_foreach_child_safe(_node, _child, _tmp) \
	cly_node_foreach_sibling_safe((_node)->child, _child, _tmp)

static inline __cly_nonull(1)
bool
cly_node_has_child(const struct cly_node * node)
{
	cly_node_assert(node);

	return !!node->child;
}

#define CLY_CONT_WALK_RET (0)
#define CLY_SKIP_WALK_RET (1)

enum cly_walk_event {
	CLY_PRE_WALK_EVT,
	CLY_POST_WALK_EVT,
	CLY_WALK_EVT_NR
};

typedef int cly_node_visit_fn(struct cly_node *, enum cly_walk_event, void *);

extern int
cly_node_walk_siblings(struct cly_node *   head,
                       cly_node_visit_fn * visit,
                       void *              data)
	__cly_nonull(2);

static inline __cly_nonull(1, 2)
int
cly_node_walk(struct cly_node * node, cly_node_visit_fn * visit, void * data)
{
	cly_node_assert(node);
	cly_assert(visit);

	return cly_node_walk_siblings(node->child, visit, data);
}

extern int
cly_node_walk_siblings_safe(struct cly_node *   head,
                            cly_node_visit_fn * visit,
                            void *              data)
	__cly_nonull(1, 2);

static inline __cly_nonull(1, 2)
int
cly_node_walk_safe(struct cly_node *   node,
                   cly_node_visit_fn * visit,
                   void *              data)
{
	cly_node_assert(node);
	cly_assert(visit);

	return cly_node_walk_siblings_safe(node->child, visit, data);
}

extern void
cly_node_add_sibling(struct cly_node ** head, struct cly_node * sibling)
	__cly_nonull(1, 2);

static inline __cly_nonull(1, 2)
void
cly_node_add_child(struct cly_node * node, struct cly_node * child)
{
	cly_node_assert(node);
	cly_node_assert(child);

	cly_node_add_sibling(&node->child, child);
}

extern void
cly_node_setup(struct cly_node * node)
	__cly_nonull(1);

extern void
cly_node_destroy(struct cly_node * node);

extern struct cly_node *
cly_node_create(size_t size)
	__malloc(cly_node_destroy, 1) \
	__alloc_size(1) \
	__returns_nonull \
	__warn_result;

#endif /* _CLY_NODE_H */
