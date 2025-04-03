/*
 * RISC-V OS Kernel List Implementation
 *
 * 一个Linux内核风格的侵入式双向链表实现
 * 此实现遵循内核的设计理念，提供了强大而灵活的数据结构操作
 */

#ifndef _KERNEL_LIST_H
#define _KERNEL_LIST_H

#include <kernel/util/atomic.h> // for memory barriers
#include <kernel/types.h>

/*
 * 简单的双向链表实现
 *
 * 使用注意：
 * - 链表节点嵌入到包含的结构体中
 * - 使用container_of宏将链表节点映射回包含的结构体
 */

struct list_head {
  struct list_head *next, *prev;
};

#define list_node list_head


/*
 * 链表的动态初始化函数
 */
static inline void INIT_LIST_HEAD(struct list_head *list) {
  list->next = list;
  list->prev = list;
}

/*
 * 在两个节点之间插入一个新的节点
 */
static inline void __list_add(struct list_head *new_node,
                              struct list_head *prev, struct list_head *next) {
  next->prev = new_node;
  new_node->next = next;
  new_node->prev = prev;
  prev->next = new_node;
}

/*
 * 在节点后添加新节点
 */
static inline void list_add(struct list_head *new_node,
                            struct list_head *head) {
  __list_add(new_node, head, head->next);
}

/*
 * 在节点尾部添加新节点
 */
static inline void list_add_tail(struct list_head *new_node,
                                 struct list_head *head) {
  __list_add(new_node, head->prev, head);
}

/*
 * 从链表中删除节点
 */
static inline void __list_del(struct list_head *prev, struct list_head *next) {
  next->prev = prev;
  prev->next = next;
}

/*
 * 从链表中删除节点，并重置节点的指针
 */
static inline void list_del_init(struct list_head *entry) {
  __list_del(entry->prev, entry->next);
  INIT_LIST_HEAD(entry);
}

/*
 * 从链表中删除节点
 */
static inline void list_del(struct list_head *entry) {
  __list_del(entry->prev, entry->next);
  entry->next = NULL;
  entry->prev = NULL;
}

/*
 * 将节点从原链表移到新链表的头部
 */
static inline void list_move(struct list_head *list, struct list_head *head) {
  __list_del(list->prev, list->next);
  list_add(list, head);
}

/*
 * 将节点从原链表移到新链表的尾部
 */
static inline void list_move_tail(struct list_head *list,
                                  struct list_head *head) {
  __list_del(list->prev, list->next);
  list_add_tail(list, head);
}

/*
 * 测试链表是否为空
 */
static inline int32 list_empty(const struct list_head *head) {
  return head->next == head;
}

/*
 * 合并两个链表
 */
static inline void __list_splice(const struct list_head *list,
                                 struct list_head *prev,
                                 struct list_head *next) {
  struct list_head *first = list->next;
  struct list_head *last = list->prev;

  first->prev = prev;
  prev->next = first;

  last->next = next;
  next->prev = last;
}

/*
 * 合并两个链表
 */
static inline void list_splice(const struct list_head *list,
                               struct list_head *head) {
  if (!list_empty(list))
    __list_splice(list, head, head->next);
}

/*
 * 合并两个链表，并重新初始化原链表
 */
static inline void list_splice_init(struct list_head *list,
                                    struct list_head *head) {
  if (!list_empty(list)) {
    __list_splice(list, head, head->next);
    INIT_LIST_HEAD(list);
  }
}

/**
 * list_entry - 从链表节点获取包含它的结构体指针
 * @ptr:    list_head 结构体的指针
 * @type:   包含该链表节点的结构体类型
 * @member: list_head 在该结构体中的成员名
 */
#define list_entry(ptr, type, member) container_of(ptr, type, member)

/*
 * 从结构体中的list指针获取结构体地址
 */
#define container_of(ptr, type, member)                                        \
  ({                                                                           \
    const typeof(((type *)0)->member) *__mptr = (ptr);                         \
    (type *)((char *)__mptr - offsetof(type, member));                         \
  })

/*
 * 计算结构体成员的偏移量
 */
#ifndef offsetof
#define offsetof(TYPE, MEMBER) ((size_t) & ((TYPE *)0)->MEMBER)
#endif

/*
 * 遍历链表
 */
#define list_for_each(pos, head)                                               \
  for (pos = (head)->next; pos != (head); pos = pos->next)

/*
 * 安全遍历链表（可以在遍历中删除节点）
 */
#define list_for_each_safe(pos, n, head)                                       \
  for (pos = (head)->next, n = pos->next; pos != (head); pos = n, n = pos->next)

/*
 * 遍历链表并获取结构体实例
 */
#define list_for_each_entry(pos, head, member)                                 \
  for (pos = container_of((head)->next, typeof(*pos), member);                 \
       &pos->member != (head);                                                 \
       pos = container_of(pos->member.next, typeof(*pos), member))

/*
 * 安全遍历链表并获取结构体实例（可以在遍历中删除节点）
 */
#define list_for_each_entry_safe(pos, n, head, member)                         \
  for (pos = container_of((head)->next, typeof(*pos), member),                 \
      n = container_of(pos->member.next, typeof(*pos), member);                \
       &pos->member != (head);                                                 \
       pos = n, n = container_of(n->member.next, typeof(*n), member))

/*
 * 获取链表第一个节点的包含结构体
 */
#define list_first_entry(ptr, type, member)                                    \
  container_of((ptr)->next, type, member)

/*
 * 获取链表最后一个节点的包含结构体
 */
#define list_last_entry(ptr, type, member)                                     \
  container_of((ptr)->prev, type, member)

/*
 * 获取链表第一个节点的包含结构体，如果链表为空则返回NULL
 */
#define list_first_entry_or_null(ptr, type, member)                            \
  ({                                                                           \
    struct list_head *head__ = (ptr);                                          \
    struct list_head *pos__ = head__->next;                                    \
    pos__ != head__ ? container_of(pos__, type, member) : NULL;                \
  })

#endif /* _KERNEL_LIST_H */