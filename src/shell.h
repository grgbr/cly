#ifndef _CLY_SHELL_H
#define _CLY_SHELL_H

#include "match.h"
#include <readline/readline.h>
#include <stdbool.h>
#include <signal.h>

typedef void cly_shell_collect_compl_fn(struct cly_match *,
                                        const char *,
                                        size_t,
                                        int,
                                        const char * const [],
                                        void *);

typedef void cly_shell_display_compl_fn(char **, int, int, void *);

extern void
cly_shell_set_prompt(const char * prompt);

struct cly_expr_blk;

extern int
cly_shell_read_expr(struct cly_expr_blk * expr_block);

/*
 * Request the completion logic to prevent from appending the completion
 * character once it has appended a selected match at the current command line
 * cursor position.
 * When required, this MUST be called each time the completion logic is
 * invoked, i.e., this behavior is always resetted before calling any
 * readline(3) custom completion function.
 */
static inline void
cly_shell_suppress_complete_char(void)
{
	rl_completion_suppress_append = 1;
}

extern void
cly_shell_enroll_display_compl(cly_shell_display_compl_fn * display,
                               void *                       data);

extern void
cly_shell_shutdown(void);

extern int
cly_shell_init(bool                         history,
               const char *                 break_chars,
               cly_shell_collect_compl_fn * complete,
               void *                       data);

extern void
cly_shell_fini(void);

#endif  /* _CLY_SHELL_H */
