/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Modified by the GLib Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GLib Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GLib at ftp://ftp.gtk.org/pub/gtk/.
 */

#ifndef __EH_DATATYPES_H__
#define __EH_DATATYPES_H__


/* GLib single-linked lists */
typedef struct _ExecHelpSList ExecHelpSList;

struct _ExecHelpSList {
  void *data;
  ExecHelpSList *next;
};

#define CASSERT(predicate, file) _impl_CASSERT_LINE(predicate,__LINE__,file)

#define _impl_PASTE(a,b) a##b
#define _impl_CASSERT_LINE(predicate, line, file) \
    typedef char _impl_PASTE(assertion_failed_##file##_,line)[2*!!(predicate)-1];

#define EH_INT_TO_POINTER(i) ((void *) (long) (i))
#define EH_ULONG_TO_POINTER(i) ((void *) (i))
#define EH_POINTER_TO_INT(p) ((int) (long) (p))
#define EH_POINTER_TO_UINT(p) ((unsigned int) (unsigned long) (p))
#define EH_POINTER_TO_ULONG(p) ((unsigned long) (p))

typedef int          (*ExecHelpCompareFunc)     (const void *, const void *);
typedef int          (*ExecHelpCompareDataFunc) (const void *, const void *, void *);
typedef void         (*ExecHelpDestroyNotify)   (void *);
typedef void *       (*ExecHelpCopyFunc)        (const void *, void *);
typedef void         (*ExecHelpFunc)            (void *, void *);
typedef int          (*ExecHelpEqualFunc)       (const void *, const void *);
typedef unsigned int (*ExecHelpHashFunc)        (const void *);
typedef void         (*ExecHelpHFunc)           (void *, void *, void *);


/* Singly linked lists
 */
ExecHelpSList * exechelp_slist_alloc                   (void);
void            exechelp_slist_free                    (ExecHelpSList *          list);
void            exechelp_slist_free_1                  (ExecHelpSList *          list);
#define	        exechelp_slist_free1		                exechelp_slist_free_1
void            exechelp_slist_free_full               (ExecHelpSList *          list,
                                                        ExecHelpDestroyNotify);
ExecHelpSList * exechelp_slist_append                  (ExecHelpSList *          list,
					                                              void *                   data);
ExecHelpSList * exechelp_slist_prepend                 (ExecHelpSList *          list,
					                                              void *                   data);
ExecHelpSList * exechelp_slist_insert                  (ExecHelpSList *          list,
					                                              void *                   data,
					                                              int                      position);
ExecHelpSList * exechelp_slist_insert_sorted           (ExecHelpSList *          list,
					                                              void *                   data,
                                                        ExecHelpCompareFunc);
ExecHelpSList * exechelp_slist_insert_sorted_with_data (ExecHelpSList *          list,
					                                              void *                   data,
                                                        ExecHelpCompareDataFunc,
					                                              void *                   user_data);
ExecHelpSList * exechelp_slist_insert_before           (ExecHelpSList *          slist,
                                                        ExecHelpSList *          sibling,
					                                              void *                   data);
ExecHelpSList * exechelp_slist_concat                  (ExecHelpSList *          list1,
                                                        ExecHelpSList *          list2);
ExecHelpSList * exechelp_slist_remove                  (ExecHelpSList *          list,
					                                              const void *             data);
ExecHelpSList * exechelp_slist_remove_all              (ExecHelpSList *          list,
					                                              const void *             data);
ExecHelpSList * exechelp_slist_remove_link             (ExecHelpSList *          list,
                                                        ExecHelpSList *          link_);
ExecHelpSList * exechelp_slist_delete_link             (ExecHelpSList *          list,
                                                        ExecHelpSList *          link_);
ExecHelpSList * exechelp_slist_reverse                 (ExecHelpSList *          list);
ExecHelpSList * exechelp_slist_copy                    (ExecHelpSList *          list);
ExecHelpSList * exechelp_slist_copy_deep               (ExecHelpSList *          list,
                                                        ExecHelpCopyFunc,
					                                              void *                   user_data);
ExecHelpSList * exechelp_slist_nth                     (ExecHelpSList *          list,
					                                              unsigned int             n);
ExecHelpSList * exechelp_slist_find                    (ExecHelpSList *          list,
					                                              const void *             data);
ExecHelpSList * exechelp_slist_find_custom             (ExecHelpSList *          list,
					                                              const void *             data,
                                                        ExecHelpCompareFunc);
int             exechelp_slist_position                (ExecHelpSList *          list,
                                                        ExecHelpSList *          llink);
int             exechelp_slist_index                   (ExecHelpSList *          list,
					                                              const void *     data);
ExecHelpSList * exechelp_slist_last                    (ExecHelpSList *          list);
unsigned int    exechelp_slist_length                  (ExecHelpSList *          list);
void            exechelp_slist_foreach                 (ExecHelpSList *          list,
                                                        ExecHelpFunc,
					                                              void *          user_data);
ExecHelpSList * exechelp_slist_sort                    (ExecHelpSList *          list,
                                                        ExecHelpCompareFunc);
ExecHelpSList * exechelp_slist_sort_with_data          (ExecHelpSList *          list,
                                                        ExecHelpCompareDataFunc,
					                                              void *          user_data);
void *          exechelp_slist_nth_data                (ExecHelpSList *          list,
					                                              unsigned int             n);
#define         exechelp_slist_next(slist)   	         ((slist) ? (((exechelpSList *)(slist))->next) : NULL)


/* GLib doubly-linked lists */
typedef struct _ExecHelpList ExecHelpList;

struct _ExecHelpList {
  void * data;
  ExecHelpList *next;
  ExecHelpList *prev;
};

ExecHelpList *  exechelp_list_alloc                   (void);
void            exechelp_list_free                    (ExecHelpList *           list);
void            exechelp_list_free_1                  (ExecHelpList *           list);
#define         exechelp_list_free1                   exechelp_list_free_1
void            exechelp_list_free_full               (ExecHelpList *           list,
					                                             ExecHelpDestroyNotify    free_func);
ExecHelpList *  exechelp_list_append                  (ExecHelpList *           list,
					                                             void *                   data);
ExecHelpList *  exechelp_list_prepend                 (ExecHelpList *           list,
			                                            		 void *                   data);
ExecHelpList *  exechelp_list_insert                  (ExecHelpList *           list,
			                                            		 void *                   data,
			                                            		 int                      position);
ExecHelpList *  exechelp_list_insert_sorted           (ExecHelpList *           list,
				                                            	 void *                   data,
				                                            	 ExecHelpCompareFunc      func);
ExecHelpList *  exechelp_list_insert_sorted_with_data (ExecHelpList *           list,
				                                            	 void *                   data,
					                                             ExecHelpCompareDataFunc  func,
					                                             void *                   user_data);
ExecHelpList *  exechelp_list_insert_before           (ExecHelpList *           list,
					                                             ExecHelpList *           sibling,
					                                             void *                   data);
ExecHelpList *  exechelp_list_concat                  (ExecHelpList *           list1,
					                                             ExecHelpList *           list2);
ExecHelpList *  exechelp_list_remove                  (ExecHelpList *           list,
					                                             const void *             data);
ExecHelpList *  exechelp_list_remove_all              (ExecHelpList *           list,
					                                             const void *             data);
ExecHelpList *  exechelp_list_remove_link             (ExecHelpList *           list,
					                                             ExecHelpList *           llink);
ExecHelpList *  exechelp_list_delete_link             (ExecHelpList *           list,
					                                             ExecHelpList *           link_);
ExecHelpList *  exechelp_list_reverse                 (ExecHelpList *           list);
ExecHelpList *  exechelp_list_copy                    (ExecHelpList *           list);
ExecHelpList *  exechelp_list_copy_deep               (ExecHelpList *           list,
					                                             ExecHelpCopyFunc         func,
					                                             void *                   user_data);
ExecHelpList *  exechelp_list_nth                     (ExecHelpList *           list,
					                                             unsigned int             n);
ExecHelpList *  exechelp_list_nth_prev                (ExecHelpList *           list,
					                                             unsigned int             n);
ExecHelpList *  exechelp_list_find                    (ExecHelpList *           list,
					                                             const void *             data);
ExecHelpList *  exechelp_list_find_custom             (ExecHelpList *           list,
					                                             const void *             data,
					                                             ExecHelpCompareFunc      func);
int             exechelp_list_position                (ExecHelpList *           list,
				                                            	 ExecHelpList *           llink);
int             exechelp_list_index                   (ExecHelpList *           list,
					                                             const void *             data);
ExecHelpList *  exechelp_list_last                    (ExecHelpList *           list);
ExecHelpList *  exechelp_list_first                   (ExecHelpList *           list);
unsigned int    exechelp_list_length                  (ExecHelpList *           list);
void            exechelp_list_foreach                 (ExecHelpList *           list,
					                                             ExecHelpFunc             func,
					                                             void *                   user_data);
ExecHelpList *  exechelp_list_sort                    (ExecHelpList *           list,
					                                             ExecHelpCompareFunc      compare_func);
ExecHelpList *  exechelp_list_sort_with_data          (ExecHelpList *           list,
					                                             ExecHelpCompareDataFunc  compare_func,
					                                             void *                   user_data) ;
void *          exechelp_list_nth_data                (ExecHelpList *           list,
					                                             unsigned int             n);

#define       exechelp_list_previous(list)	          ((list) ? (((ExecHelpList *)(list))->prev) : NULL)
#define       exechelp_list_next(list)	              ((list) ? (((ExecHelpList *)(list))->next) : NULL)


/* GLib hash tables */
typedef struct _ExecHelpHashTable ExecHelpHashTable;
typedef int (*ExecHelpHRFunc) (void * key, void * value, void * user_data);
typedef struct _ExecHelpHashTableIter ExecHelpHashTableIter;

struct _ExecHelpHashTableIter {
 /*< private >*/
 void * dummy1;
 void * dummy2;
 void * dummy3;
 int dummy4;
 int dummy5;
 void * dummy6;
};

ExecHelpHashTable *exechelp_hash_table_new (ExecHelpHashFunc hash_func, ExecHelpEqualFunc key_equal_func);
ExecHelpHashTable *exechelp_hash_table_new_full (ExecHelpHashFunc hash_func, ExecHelpEqualFunc key_equal_func, ExecHelpDestroyNotify key_destroy_func, ExecHelpDestroyNotify value_destroy_func);
void exechelp_hash_table_destroy (ExecHelpHashTable *hash_table);
int exechelp_hash_table_insert (ExecHelpHashTable *hash_table, void * key, void * value);
int exechelp_hash_table_replace (ExecHelpHashTable *hash_table, void * key, void * value);
int exechelp_hash_table_add (ExecHelpHashTable *hash_table, void * key);
int exechelp_hash_table_remove (ExecHelpHashTable *hash_table, const void * key);
void exechelp_hash_table_remove_all (ExecHelpHashTable *hash_table);
int exechelp_hash_table_steal (ExecHelpHashTable *hash_table, const void * key);
void exechelp_hash_table_steal_all (ExecHelpHashTable *hash_table);
void * exechelp_hash_table_lookup (ExecHelpHashTable *hash_table, const void * key);
int exechelp_hash_table_contains (ExecHelpHashTable *hash_table, const void * key);
int exechelp_hash_table_lookup_extended (ExecHelpHashTable *hash_table, const void * lookup_key,
 void * *orig_key, void * *value);
 void exechelp_hash_table_foreach (ExecHelpHashTable *hash_table, ExecHelpHFunc func, void * user_data);
void * exechelp_hash_table_find (ExecHelpHashTable *hash_table, ExecHelpHRFunc predicate, void * user_data);
unsigned int exechelp_hash_table_foreach_remove (ExecHelpHashTable *hash_table, ExecHelpHRFunc func, void * user_data);
unsigned int exechelp_hash_table_foreach_steal (ExecHelpHashTable *hash_table, ExecHelpHRFunc func, void * user_data);
unsigned int exechelp_hash_table_size (ExecHelpHashTable *hash_table);
ExecHelpList * exechelp_hash_table_get_keys (ExecHelpHashTable *hash_table);
ExecHelpList * exechelp_hash_table_get_values (ExecHelpHashTable *hash_table);
void * * exechelp_hash_table_get_keys_as_array (ExecHelpHashTable *hash_table, unsigned int *length);
void exechelp_hash_table_iter_init (ExecHelpHashTableIter *iter, ExecHelpHashTable *hash_table);
int exechelp_hash_table_iter_next (ExecHelpHashTableIter *iter, void * *key, void * *value);
ExecHelpHashTable *exechelp_hash_table_iter_get_hash_table (ExecHelpHashTableIter *iter);
void exechelp_hash_table_iter_remove (ExecHelpHashTableIter *iter);
void exechelp_hash_table_iter_replace (ExecHelpHashTableIter *iter, void * value);
void exechelp_hash_table_iter_steal (ExecHelpHashTableIter *iter);

ExecHelpHashTable *exechelp_hash_table_ref (ExecHelpHashTable *hash_table);
void exechelp_hash_table_unref (ExecHelpHashTable *hash_table);

/* Hash Functions
 */
int exechelp_str_equal (const void * v1, const void * v2);
unsigned int exechelp_str_hash (const void * v);
int exechelp_int_equal (const void * v1, const void * v2);
unsigned int exechelp_int_hash (const void * v);
int exechelp_int64_equal (const void * v1, const void * v2);
unsigned int exechelp_int64_hash (const void * v);
int exechelp_double_equal (const void * v1, const void * v2);
unsigned int exechelp_double_hash (const void * v);
unsigned int exechelp_direct_hash (const void * v);
int exechelp_direct_equal (const void * v1, const void * v2);

#endif /* __EH_DATATYPES_H__ */
