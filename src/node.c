#include "node.h"

static int
cly_node_walk_recurs(struct cly_node *   node,
                     cly_node_visit_fn * visit,
                     void *              data)
{
	cly_node_assert(node);
	cly_assert(visit);

	int ret;

	ret = visit(node, CLY_PRE_WALK_EVT, data);
	if (ret == CLY_CONT_WALK_RET) {
		struct cly_node * child;

		cly_node_foreach_child(node, child) {
			ret = cly_node_walk_recurs(child, visit, data);
			if (ret < 0)
				return ret;
		}

		ret = visit(node, CLY_POST_WALK_EVT, data);
		cly_assert(ret <= 0);
	}

	return (ret >= 0) ? 0 : ret;
}

int
cly_node_walk_siblings(struct cly_node *   head,
                       cly_node_visit_fn * visit,
                       void *              data)
{
	cly_assert(!head || !head->prev->next);
	cly_assert(visit);

	struct cly_node * sib;
	int               ret = 0;

	cly_node_foreach_sibling(head, sib) {
		ret = cly_node_walk_recurs(sib, visit, data);
		if (ret < 0)
			return ret;
	}

	cly_assert(!ret);

	return 0;
}

static int
cly_node_walk_recurs_safe(struct cly_node *   node,
                          cly_node_visit_fn * visit,
                          void *              data)
{
#warning Remove recursion
	cly_node_assert(node);
	cly_assert(visit);

	int ret;

	ret = visit(node, CLY_PRE_WALK_EVT, data);
	if (ret == CLY_CONT_WALK_RET) {
		struct cly_node * child;
		struct cly_node * tmp;

		cly_node_foreach_child_safe(node, child, tmp) {
			ret = cly_node_walk_recurs_safe(child, visit, data);
			if (ret < 0)
				return ret;
		}

		ret = visit(node, CLY_POST_WALK_EVT, data);
		cly_assert(ret <= 0);
	}

	return (ret >= 0) ? 0 : ret;
}

int
cly_node_walk_siblings_safe(struct cly_node *   head,
                            cly_node_visit_fn * visit,
                            void *              data)
{
	cly_assert(!head || !head->prev->next);
	cly_assert(visit);

	struct cly_node * sib;
	struct cly_node * tmp;
	int               ret = 0;

	cly_node_foreach_sibling_safe(head, sib, tmp) {
		ret = cly_node_walk_recurs_safe(sib, visit, data);
		if (ret < 0)
			return ret;
	}

	cly_assert(!ret);

	return 0;
}

void
cly_node_add_sibling(struct cly_node ** head, struct cly_node * sibling)
{
	cly_assert(head);
	cly_assert(!(*head) || !(*head)->prev->next);
	cly_node_assert(sibling);
	cly_assert(!sibling->next);
	cly_assert(sibling->prev == sibling);
	cly_assert(!sibling->parent);

	struct cly_node * hd = *head;

	if (hd) {
		struct cly_node * tail = hd->prev;

		sibling->prev = tail;
		tail->next = sibling;
		hd->prev = sibling;
	}
	else
		*head = sibling;

	sibling->parent = containerof(head, struct cly_node, child);
}

void
cly_node_setup(struct cly_node * node)
{
	cly_assert(node);

	node->next = NULL;
	node->prev = node;
	node->child = NULL;
	node->parent = NULL;
}

struct cly_node *
cly_node_create(size_t size)
{
	cly_assert(size >= sizeof(struct cly_node));

	struct cly_node * node;

	node = cly_malloc(size);
	cly_assert(node);
	cly_node_setup(node);

	return node;
}

void
cly_node_destroy(struct cly_node * node)
{
	cly_free(node);
}
