/*
 * Alper Akcan - 14.10.2011
 */

#ifndef _SMASHFS_LIST_H_
#define _SMASHFS_LIST_H_

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

struct smashfs_list {
	struct smashfs_list *next;
	struct smashfs_list *prev;
};

#define SMASHFS_LIST_HEAD_INIT(name) { &(name), &(name) }

#define smashfs_offsetof_(type, member) ((size_t) &((type *) 0)->member)

#define smashfs_containerof_(ptr, type, member) ({ \
	const typeof(((type *)0)->member) * __mptr = (ptr); \
	(type *)((char *)__mptr - smashfs_offsetof_(type, member)); \
})

#define smashfs_list_entry(ptr, type, member) \
	smashfs_containerof_(ptr, type, member)

#define smashfs_list_first_entry(ptr, type, member) \
	smashfs_list_entry((ptr)->next, type, member)

#define smashfs_list_last_entry(ptr, type, member) \
	smashfs_list_entry((ptr)->prev, type, member)

#define smashfs_list_next_entry(ptr, list_ptr, type, member) \
	((ptr)->next == list_ptr) ? smashfs_list_entry((list_ptr)->next, type, member) : smashfs_list_entry((ptr)->next, type, member)

#define smashfs_list_prev_entry(ptr, list_ptr, type, member) \
	((ptr)->prev == list_ptr) ? smashfs_list_entry((list_ptr)->prev, type, member) : smashfs_list_entry((ptr)->prev, type, member)

#define smashfs_list_for_each(pos, head) \
	for (pos = (head)->next; pos != (head); pos = pos->next)

#define smashfs_list_for_each_safe(pos, n, head) \
	for (pos = (head)->next, n = pos->next; pos != (head); pos = n, n = pos->next)

#define smashfs_list_for_each_entry(pos, head, member) \
	for (pos = smashfs_list_entry((head)->next, typeof(*pos), member); \
	     &pos->member != (head); \
	     pos = smashfs_list_entry(pos->member.next, typeof(*pos), member))

#define smashfs_list_for_each_entry_prev(pos, head, member) \
	for (pos = smashfs_list_entry((head)->prev, typeof(*pos), member); \
	     &pos->member != (head); \
	     pos = smashfs_list_entry(pos->member.prev, typeof(*pos), member))

#define smashfs_list_for_each_entry_safe(pos, n, head, member) \
	for (pos = smashfs_list_entry((head)->next, typeof(*pos), member), \
	     n = smashfs_list_entry(pos->member.next, typeof(*pos), member); \
	     &pos->member != (head); \
	     pos = n, n = smashfs_list_entry(n->member.next, typeof(*n), member))

static inline int smashfs_list_count (struct smashfs_list *head)
{
	int count;
	struct smashfs_list *entry;
	count = 0;
	smashfs_list_for_each(entry, head) {
		count++;
	}
	return count;
}

static inline void smashfs_list_add_actual (struct smashfs_list *elem, struct smashfs_list *prev, struct smashfs_list *next)
{
	next->prev = elem;
	elem->next = next;
	elem->prev = prev;
	prev->next = elem;
}

static inline void smashfs_list_del_actual (struct smashfs_list *prev, struct smashfs_list *next)
{
	next->prev = prev;
	prev->next = next;
}

static inline void smashfs_list_add_tail (struct smashfs_list *elem, struct smashfs_list *head)
{
	smashfs_list_add_actual(elem, head->prev, head);
}

static inline void smashfs_list_add (struct smashfs_list *elem, struct smashfs_list *head)
{
	smashfs_list_add_actual(elem, head, head->next);
}

static inline void smashfs_list_concat (struct smashfs_list *dest, struct smashfs_list *src)
{
	dest->prev->next = src->next;
	src->next->prev = dest->prev;
	src->prev->next = dest;
	dest->prev = src->prev;
	src->prev = src;
	src->next = src;
}

static inline void smashfs_list_del (struct smashfs_list *entry)
{
	smashfs_list_del_actual(entry->prev, entry->next);
	entry->next = NULL;
	entry->prev = NULL;
}

static inline int smashfs_list_is_first (struct smashfs_list *list, struct smashfs_list *head)
{
	return head->next == list;
}

static inline int smashfs_list_is_last (struct smashfs_list *list, struct smashfs_list *head)
{
	return list->next == head;
}

static inline void smashfs_list_init (struct smashfs_list *head)
{
	head->next = head;
	head->prev = head;
}

#ifdef __cplusplus
} /* end of the 'extern "C"' block */
#endif

#endif /* _SMASHFS_LIST_H_ */
