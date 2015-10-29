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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
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

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>  /* memset */
#include "../include/exechelper.h"
#include "../include/exechelper-datatypes.h"

ExecHelpSList*
exechelp_slist_alloc (void)
{
  return malloc (sizeof (ExecHelpSList));
}

void
exechelp_slist_free (ExecHelpSList *list)
{
  if (list) {
    exechelp_slist_free (list->next);
    free (list);
  }
}

void
exechelp_slist_free_1 (ExecHelpSList *list)
{
  free (list);
}


void
exechelp_slist_free_full (ExecHelpSList *list,
                   ExecHelpDestroyNotify free_func)
{
  exechelp_slist_foreach (list, (ExecHelpFunc) free_func, NULL);
  exechelp_slist_free (list);
}

ExecHelpSList*
exechelp_slist_append (ExecHelpSList *list,
                void * data)
{
  ExecHelpSList *new_list;
  ExecHelpSList *last;

  new_list = malloc (sizeof (ExecHelpSList));
  new_list->data = data;
  new_list->next = NULL;

  if (list)
    {
      last = exechelp_slist_last (list);

      last->next = new_list;

      return list;
    }
  else
    return new_list;
}

ExecHelpSList*
exechelp_slist_prepend (ExecHelpSList *list,
                 void * data)
{
  ExecHelpSList *new_list;

  new_list = malloc (sizeof (ExecHelpSList));
  new_list->data = data;
  new_list->next = list;

  return new_list;
}

ExecHelpSList*
exechelp_slist_insert (ExecHelpSList *list,
                void * data,
                int position)
{
  ExecHelpSList *prev_list;
  ExecHelpSList *tmp_list;
  ExecHelpSList *new_list;

  if (position < 0)
    return exechelp_slist_append (list, data);
  else if (position == 0)
    return exechelp_slist_prepend (list, data);

  new_list = malloc (sizeof (ExecHelpSList));
  new_list->data = data;

  if (!list)
    {
      new_list->next = NULL;
      return new_list;
    }

  prev_list = NULL;
  tmp_list = list;

  while ((position-- > 0) && tmp_list)
    {
      prev_list = tmp_list;
      tmp_list = tmp_list->next;
    }

  new_list->next = prev_list->next;
  prev_list->next = new_list;

  return list;
}

ExecHelpSList*
exechelp_slist_insert_before (ExecHelpSList *slist,
                       ExecHelpSList *sibling,
                       void * data)
{
  if (!slist)
    {
      slist = malloc (sizeof (ExecHelpSList));
      slist->data = data;
      slist->next = NULL;
      return slist;
    }
  else
    {
      ExecHelpSList *node, *last = NULL;

      for (node = slist; node; last = node, node = last->next)
        if (node == sibling)
          break;
      if (!last)
        {
          node = malloc (sizeof (ExecHelpSList));
          node->data = data;
          node->next = slist;

          return node;
        }
      else
        {
          node = malloc (sizeof (ExecHelpSList));
          node->data = data;
          node->next = last->next;
          last->next = node;

          return slist;
        }
    }
}

ExecHelpSList *
exechelp_slist_concat (ExecHelpSList *list1, ExecHelpSList *list2)
{
  if (list2)
    {
      if (list1)
        exechelp_slist_last (list1)->next = list2;
      else
        list1 = list2;
    }

  return list1;
}

ExecHelpSList*
exechelp_slist_remove (ExecHelpSList *list,
                const void * data)
{
  ExecHelpSList *tmp, *prev = NULL;

  tmp = list;
  while (tmp)
    {
      if (tmp->data == data)
        {
          if (prev)
            prev->next = tmp->next;
          else
            list = tmp->next;

          exechelp_slist_free_1 (tmp);
          break;
        }
      prev = tmp;
      tmp = prev->next;
    }

  return list;
}

ExecHelpSList*
exechelp_slist_remove_all (ExecHelpSList *list,
                    const void * data)
{
  ExecHelpSList *tmp, *prev = NULL;

  tmp = list;
  while (tmp)
    {
      if (tmp->data == data)
        {
          ExecHelpSList *next = tmp->next;

          if (prev)
            prev->next = next;
          else
            list = next;

          exechelp_slist_free_1 (tmp);
          tmp = next;
        }
      else
        {
          prev = tmp;
          tmp = prev->next;
        }
    }

  return list;
}

static inline ExecHelpSList*
_exechelp_slist_remove_link (ExecHelpSList *list,
                      ExecHelpSList *link)
{
  ExecHelpSList *tmp;
  ExecHelpSList *prev;

  prev = NULL;
  tmp = list;

  while (tmp)
    {
      if (tmp == link)
        {
          if (prev)
            prev->next = tmp->next;
          if (list == tmp)
            list = list->next;

          tmp->next = NULL;
          break;
        }

      prev = tmp;
      tmp = tmp->next;
    }

  return list;
}

ExecHelpSList*
exechelp_slist_remove_link (ExecHelpSList *list,
                     ExecHelpSList *link_)
{
  return _exechelp_slist_remove_link (list, link_);
}

ExecHelpSList*
exechelp_slist_delete_link (ExecHelpSList *list,
                     ExecHelpSList *link_)
{
  list = _exechelp_slist_remove_link (list, link_);
  free (link_);

  return list;
}

ExecHelpSList*
exechelp_slist_copy (ExecHelpSList *list)
{
  return exechelp_slist_copy_deep (list, NULL, NULL);
}

ExecHelpSList*
exechelp_slist_copy_deep (ExecHelpSList *list, ExecHelpCopyFunc func, void * user_data)
{
  ExecHelpSList *new_list = NULL;

  if (list)
    {
      ExecHelpSList *last;

      new_list = malloc (sizeof (ExecHelpSList));
      if (func)
        new_list->data = func (list->data, user_data);
      else
        new_list->data = list->data;
      last = new_list;
      list = list->next;
      while (list)
        {
          last->next = malloc (sizeof (ExecHelpSList));
          last = last->next;
          if (func)
            last->data = func (list->data, user_data);
          else
            last->data = list->data;
          list = list->next;
        }
      last->next = NULL;
    }

  return new_list;
}

ExecHelpSList*
exechelp_slist_reverse (ExecHelpSList *list)
{
  ExecHelpSList *prev = NULL;

  while (list)
    {
      ExecHelpSList *next = list->next;

      list->next = prev;

      prev = list;
      list = next;
    }

  return prev;
}
ExecHelpSList*
exechelp_slist_nth (ExecHelpSList *list,
             unsigned int n)
{
  while (n-- > 0 && list)
    list = list->next;

  return list;
}
void *
exechelp_slist_nth_data (ExecHelpSList *list,
                  unsigned int n)
{
  while (n-- > 0 && list)
    list = list->next;

  return list ? list->data : NULL;
}
ExecHelpSList*
exechelp_slist_find (ExecHelpSList *list,
              const void * data)
{
  while (list)
    {
      if (list->data == data)
        break;
      list = list->next;
    }

  return list;
}
ExecHelpSList*
exechelp_slist_find_custom (ExecHelpSList *list,
                     const void * data,
                     ExecHelpCompareFunc func)
{
  if (!func)
    return NULL;

  while (list)
    {
      if (! func (list->data, data))
        return list;
      list = list->next;
    }

  return NULL;
}
int
exechelp_slist_position (ExecHelpSList *list,
                  ExecHelpSList *llink)
{
  int i;

  i = 0;
  while (list)
    {
      if (list == llink)
        return i;
      i++;
      list = list->next;
    }

  return -1;
}
int
exechelp_slist_index (ExecHelpSList *list,
               const void * data)
{
  int i;

  i = 0;
  while (list)
    {
      if (list->data == data)
        return i;
      i++;
      list = list->next;
    }

  return -1;
}
ExecHelpSList*
exechelp_slist_last (ExecHelpSList *list)
{
  if (list)
    {
      while (list->next)
        list = list->next;
    }

  return list;
}
unsigned int
exechelp_slist_length (ExecHelpSList *list)
{
  unsigned int length;

  length = 0;
  while (list)
    {
      length++;
      list = list->next;
    }

  return length;
}
void
exechelp_slist_foreach (ExecHelpSList *list,
                 ExecHelpFunc func,
                 void * user_data)
{
  while (list)
    {
      ExecHelpSList *next = list->next;
      (*func) (list->data, user_data);
      list = next;
    }
}

static ExecHelpSList*
exechelp_slist_insert_sorted_real (ExecHelpSList *list,
                            void * data,
                            ExecHelpFunc func,
                            void * user_data)
{
  ExecHelpSList *tmp_list = list;
  ExecHelpSList *prev_list = NULL;
  ExecHelpSList *new_list;
  int cmp;

  if (!func)
      return list;

  if (!list)
    {
      new_list = malloc (sizeof (ExecHelpSList));
      new_list->data = data;
      new_list->next = NULL;
      return new_list;
    }

  cmp = ((ExecHelpCompareDataFunc) func) (data, tmp_list->data, user_data);

  while ((tmp_list->next) && (cmp > 0))
    {
      prev_list = tmp_list;
      tmp_list = tmp_list->next;

      cmp = ((ExecHelpCompareDataFunc) func) (data, tmp_list->data, user_data);
    }

  new_list = malloc (sizeof (ExecHelpSList));
  new_list->data = data;

  if ((!tmp_list->next) && (cmp > 0))
    {
      tmp_list->next = new_list;
      new_list->next = NULL;
      return list;
    }

  if (prev_list)
    {
      prev_list->next = new_list;
      new_list->next = tmp_list;
      return list;
    }
  else
    {
      new_list->next = list;
      return new_list;
    }
}
ExecHelpSList*
exechelp_slist_insert_sorted (ExecHelpSList *list,
                       void * data,
                       ExecHelpCompareFunc func)
{
  return exechelp_slist_insert_sorted_real (list, data, (ExecHelpFunc) func, NULL);
}
ExecHelpSList*
exechelp_slist_insert_sorted_with_data (ExecHelpSList *list,
                                 void * data,
                                 ExecHelpCompareDataFunc func,
                                 void * user_data)
{
  return exechelp_slist_insert_sorted_real (list, data, (ExecHelpFunc) func, user_data);
}

static ExecHelpSList *
exechelp_slist_sort_merge (ExecHelpSList *l1,
                    ExecHelpSList *l2,
                    ExecHelpFunc compare_func,
                    void * user_data)
{
  ExecHelpSList list, *l;
  int cmp;

  l=&list;

  while (l1 && l2)
    {
      cmp = ((ExecHelpCompareDataFunc) compare_func) (l1->data, l2->data, user_data);

      if (cmp <= 0)
        {
          l=l->next=l1;
          l1=l1->next;
        }
      else
        {
          l=l->next=l2;
          l2=l2->next;
        }
    }
  l->next= l1 ? l1 : l2;

  return list.next;
}

static ExecHelpSList *
exechelp_slist_sort_real (ExecHelpSList *list,
                   ExecHelpFunc compare_func,
                   void * user_data)
{
  ExecHelpSList *l1, *l2;

  if (!list)
    return NULL;
  if (!list->next)
    return list;

  l1 = list;
  l2 = list->next;

  while ((l2 = l2->next) != NULL)
    {
      if ((l2 = l2->next) == NULL)
        break;
      l1=l1->next;
    }
  l2 = l1->next;
  l1->next = NULL;

  return exechelp_slist_sort_merge (exechelp_slist_sort_real (list, compare_func, user_data),
                             exechelp_slist_sort_real (l2, compare_func, user_data),
                             compare_func,
                             user_data);
}

ExecHelpSList *
exechelp_slist_sort (ExecHelpSList *list,
              ExecHelpCompareFunc compare_func)
{
  return exechelp_slist_sort_real (list, (ExecHelpFunc) compare_func, NULL);
}

ExecHelpSList *
exechelp_slist_sort_with_data (ExecHelpSList *list,
                        ExecHelpCompareDataFunc compare_func,
                        void * user_data)
{
  return exechelp_slist_sort_real (list, (ExecHelpFunc) compare_func, user_data);
}


ExecHelpList *
exechelp_list_alloc (void)
{
  return malloc(sizeof(ExecHelpList) * 1);
}

void
exechelp_list_free (ExecHelpList *list)
{
  if (list) {
    exechelp_list_free (list->next); 
    free (list);
  }
}

void
exechelp_list_free_1 (ExecHelpList *list)
{
  free (list);
}

void
exechelp_list_free_full (ExecHelpList          *list,
                  ExecHelpDestroyNotify  free_func)
{
  exechelp_list_foreach (list, (ExecHelpFunc) free_func, NULL);
  exechelp_list_free (list);
}

ExecHelpList *
exechelp_list_append (ExecHelpList    *list,
               void *  data)
{
  ExecHelpList *new_list;
  ExecHelpList *last;
  
  new_list = malloc(sizeof (ExecHelpList));
  new_list->data = data;
  new_list->next = NULL;
  
  if (list)
    {
      last = exechelp_list_last (list);
      /* exechelp_assert (last != NULL); */
      last->next = new_list;
      new_list->prev = last;

      return list;
    }
  else
    {
      new_list->prev = NULL;
      return new_list;
    }
} 

ExecHelpList *
exechelp_list_prepend (ExecHelpList    *list,
                void *  data)
{
  ExecHelpList *new_list;
  
  new_list = malloc(sizeof (ExecHelpList));
  new_list->data = data;
  new_list->next = list;
  
  if (list)
    {
      new_list->prev = list->prev;
      if (list->prev)
        list->prev->next = new_list;
      list->prev = new_list;
    }
  else
    new_list->prev = NULL;
  
  return new_list;
}

ExecHelpList *
exechelp_list_insert (ExecHelpList    *list,
               void *  data,
               int      position)
{
  ExecHelpList *new_list;
  ExecHelpList *tmp_list;

  if (position < 0)
    return exechelp_list_append (list, data);
  else if (position == 0)
    return exechelp_list_prepend (list, data);

  tmp_list = exechelp_list_nth (list, position);
  if (!tmp_list)
    return exechelp_list_append (list, data);

  new_list = malloc(sizeof (ExecHelpList));
  new_list->data = data;
  new_list->prev = tmp_list->prev;
  tmp_list->prev->next = new_list;
  new_list->next = tmp_list;
  tmp_list->prev = new_list;

  return list;
}

ExecHelpList *
exechelp_list_insert_before (ExecHelpList    *list,
                      ExecHelpList    *sibling,
                      void *  data)
{
  if (!list)
    {
      list = exechelp_list_alloc ();
      list->data = data;
      return list;
    }
  else if (sibling)
    {
      ExecHelpList *node;

      node = malloc(sizeof (ExecHelpList));
      node->data = data;
      node->prev = sibling->prev;
      node->next = sibling;
      sibling->prev = node;
      if (node->prev)
        {
          node->prev->next = node;
          return list;
        }
      else
        {
          return node;
        }
    }
  else
    {
      ExecHelpList *last;

      last = list;
      while (last->next)
        last = last->next;

      last->next = malloc(sizeof (ExecHelpList));
      last->next->data = data;
      last->next->prev = last;
      last->next->next = NULL;

      return list;
    }
}

ExecHelpList *
exechelp_list_concat (ExecHelpList *list1,
               ExecHelpList *list2)
{
  ExecHelpList *tmp_list;
  
  if (list2)
    {
      tmp_list = exechelp_list_last (list1);
      if (tmp_list)
        tmp_list->next = list2;
      else
        list1 = list2;
      list2->prev = tmp_list;
    }
  
  return list1;
}

static inline ExecHelpList *
_exechelp_list_remove_link (ExecHelpList *list,
                     ExecHelpList *link)
{
  if (link == NULL)
    return list;

  if (link->prev)
    {
      if (link->prev->next == link)
        link->prev->next = link->next;
      //else
        //exechelp_warning ("corrupted double-linked list detected");
    }
  if (link->next)
    {
      if (link->next->prev == link)
        link->next->prev = link->prev;
      //else
        //exechelp_warning ("corrupted double-linked list detected");
    }

  if (link == list)
    list = list->next;

  link->next = NULL;
  link->prev = NULL;

  return list;
}

ExecHelpList *
exechelp_list_remove (ExecHelpList         *list,
               const void *  data)
{
  ExecHelpList *tmp;

  tmp = list;
  while (tmp)
    {
      if (tmp->data != data)
        tmp = tmp->next;
      else
        {
          list = _exechelp_list_remove_link (list, tmp);
          free (tmp);

          break;
        }
    }
  return list;
}

ExecHelpList *
exechelp_list_remove_all (ExecHelpList         *list,
                   const void *  data)
{
  ExecHelpList *tmp = list;

  while (tmp)
    {
      if (tmp->data != data)
        tmp = tmp->next;
      else
        {
          ExecHelpList *next = tmp->next;

          if (tmp->prev)
            tmp->prev->next = next;
          else
            list = next;
          if (next)
            next->prev = tmp->prev;

          free (tmp);
          tmp = next;
        }
    }
  return list;
}

ExecHelpList *
exechelp_list_remove_link (ExecHelpList *list,
                    ExecHelpList *llink)
{
  return _exechelp_list_remove_link (list, llink);
}

ExecHelpList *
exechelp_list_delete_link (ExecHelpList *list,
                    ExecHelpList *link_)
{
  list = _exechelp_list_remove_link (list, link_);
  free (link_);

  return list;
}

ExecHelpList *
exechelp_list_copy (ExecHelpList *list)
{
  return exechelp_list_copy_deep (list, NULL, NULL);
}

ExecHelpList *
exechelp_list_copy_deep (ExecHelpList     *list,
                  ExecHelpCopyFunc  func,
                  void *   user_data)
{
  ExecHelpList *new_list = NULL;

  if (list)
    {
      ExecHelpList *last;

      new_list = malloc(sizeof (ExecHelpList));
      if (func)
        new_list->data = func (list->data, user_data);
      else
        new_list->data = list->data;
      new_list->prev = NULL;
      last = new_list;
      list = list->next;
      while (list)
        {
          last->next = malloc(sizeof (ExecHelpList));
          last->next->prev = last;
          last = last->next;
          if (func)
            last->data = func (list->data, user_data);
          else
            last->data = list->data;
          list = list->next;
        }
      last->next = NULL;
    }

  return new_list;
}

ExecHelpList *
exechelp_list_reverse (ExecHelpList *list)
{
  ExecHelpList *last;
  
  last = NULL;
  while (list)
    {
      last = list;
      list = last->next;
      last->next = last->prev;
      last->prev = list;
    }
  
  return last;
}

ExecHelpList *
exechelp_list_nth (ExecHelpList *list,
            unsigned int  n)
{
  while ((n-- > 0) && list)
    list = list->next;
  
  return list;
}

ExecHelpList *
exechelp_list_nth_prev (ExecHelpList *list,
                 unsigned int  n)
{
  while ((n-- > 0) && list)
    list = list->prev;
  
  return list;
}

void *
exechelp_list_nth_data (ExecHelpList *list,
                 unsigned int  n)
{
  while ((n-- > 0) && list)
    list = list->next;
  
  return list ? list->data : NULL;
}

ExecHelpList *
exechelp_list_find (ExecHelpList         *list,
             const void *  data)
{
  while (list)
    {
      if (list->data == data)
        break;
      list = list->next;
    }
  
  return list;
}

ExecHelpList *
exechelp_list_find_custom (ExecHelpList         *list,
                    const void *  data,
                    ExecHelpCompareFunc   func)
{
  if (!func)
    return list;

  while (list)
    {
      if (! func (list->data, data))
        return list;
      list = list->next;
    }

  return NULL;
}

int
exechelp_list_position (ExecHelpList *list,
                 ExecHelpList *llink)
{
  int i;

  i = 0;
  while (list)
    {
      if (list == llink)
        return i;
      i++;
      list = list->next;
    }

  return -1;
}

int
exechelp_list_index (ExecHelpList         *list,
              const void *  data)
{
  int i;

  i = 0;
  while (list)
    {
      if (list->data == data)
        return i;
      i++;
      list = list->next;
    }

  return -1;
}

ExecHelpList *
exechelp_list_last (ExecHelpList *list)
{
  if (list)
    {
      while (list->next)
        list = list->next;
    }
  
  return list;
}

ExecHelpList *
exechelp_list_first (ExecHelpList *list)
{
  if (list)
    {
      while (list->prev)
        list = list->prev;
    }
  
  return list;
}

unsigned int
exechelp_list_length (ExecHelpList *list)
{
  unsigned int length;
  
  length = 0;
  while (list)
    {
      length++;
      list = list->next;
    }
  
  return length;
}

void
exechelp_list_foreach (ExecHelpList    *list,
                ExecHelpFunc     func,
                void *  user_data)
{
  while (list)
    {
      ExecHelpList *next = list->next;
      (*func) (list->data, user_data);
      list = next;
    }
}

static ExecHelpList*
exechelp_list_insert_sorted_real (ExecHelpList    *list,
                           void *  data,
                           ExecHelpFunc     func,
                           void *  user_data)
{
  ExecHelpList *tmp_list = list;
  ExecHelpList *new_list;
  int cmp;

  if (!func)
    return list;
  
  if (!list) 
    {
      new_list = malloc(sizeof (ExecHelpList));
      new_list->data = data;
      return new_list;
    }
  
  cmp = ((ExecHelpCompareDataFunc) func) (data, tmp_list->data, user_data);

  while ((tmp_list->next) && (cmp > 0))
    {
      tmp_list = tmp_list->next;

      cmp = ((ExecHelpCompareDataFunc) func) (data, tmp_list->data, user_data);
    }

  new_list = malloc(sizeof (ExecHelpList));
  new_list->data = data;

  if ((!tmp_list->next) && (cmp > 0))
    {
      tmp_list->next = new_list;
      new_list->prev = tmp_list;
      return list;
    }
   
  if (tmp_list->prev)
    {
      tmp_list->prev->next = new_list;
      new_list->prev = tmp_list->prev;
    }
  new_list->next = tmp_list;
  tmp_list->prev = new_list;
 
  if (tmp_list == list)
    return new_list;
  else
    return list;
}

ExecHelpList *
exechelp_list_insert_sorted (ExecHelpList        *list,
                      void *      data,
                      ExecHelpCompareFunc  func)
{
  return exechelp_list_insert_sorted_real (list, data, (ExecHelpFunc) func, NULL);
}

ExecHelpList *
exechelp_list_insert_sorted_with_data (ExecHelpList            *list,
                                void *          data,
                                ExecHelpCompareDataFunc  func,
                                void *          user_data)
{
  return exechelp_list_insert_sorted_real (list, data, (ExecHelpFunc) func, user_data);
}

static ExecHelpList *
exechelp_list_sort_merge (ExecHelpList     *l1, 
                   ExecHelpList     *l2,
                   ExecHelpFunc     compare_func,
                   void *  user_data)
{
  ExecHelpList list, *l, *lprev;
  int cmp;

  l = &list; 
  lprev = NULL;

  while (l1 && l2)
    {
      cmp = ((ExecHelpCompareDataFunc) compare_func) (l1->data, l2->data, user_data);

      if (cmp <= 0)
        {
          l->next = l1;
          l1 = l1->next;
        } 
      else 
        {
          l->next = l2;
          l2 = l2->next;
        }
      l = l->next;
      l->prev = lprev; 
      lprev = l;
    }
  l->next = l1 ? l1 : l2;
  l->next->prev = l;

  return list.next;
}

static ExecHelpList * 
exechelp_list_sort_real (ExecHelpList    *list,
                  ExecHelpFunc     compare_func,
                  void *  user_data)
{
  ExecHelpList *l1, *l2;
  
  if (!list) 
    return NULL;
  if (!list->next) 
    return list;

  l1 = list; 
  l2 = list->next;

  while ((l2 = l2->next) != NULL)
    {
      if ((l2 = l2->next) == NULL) 
        break;
      l1 = l1->next;
    }
  l2 = l1->next; 
  l1->next = NULL; 

  return exechelp_list_sort_merge (exechelp_list_sort_real (list, compare_func, user_data),
                            exechelp_list_sort_real (l2, compare_func, user_data),
                            compare_func,
                            user_data);
}

ExecHelpList *
exechelp_list_sort (ExecHelpList        *list,
             ExecHelpCompareFunc  compare_func)
{
  return exechelp_list_sort_real (list, (ExecHelpFunc) compare_func, NULL);
}

ExecHelpList *
exechelp_list_sort_with_data (ExecHelpList            *list,
                       ExecHelpCompareDataFunc  compare_func,
                       void *          user_data)
{
  return exechelp_list_sort_real (list, (ExecHelpFunc) compare_func, user_data);
}


#define HASH_TABLE_MIN_SHIFT 3  /* 1 << 3 == 8 buckets */
#define UNUSED_HASH_VALUE 0
#define TOMBSTONE_HASH_VALUE 1
#define HASH_IS_UNUSED(h_) ((h_) == UNUSED_HASH_VALUE)
#define HASH_IS_TOMBSTONE(h_) ((h_) == TOMBSTONE_HASH_VALUE)
#define HASH_IS_REAL(h_) ((h_) >= 2)

struct _ExecHelpHashTable
{
  int                     size;
  int                     mod;
  unsigned int            mask;
  int                     nnodes;
  int                     noccupied;  /* nnodes + tombstones */

  void *                 *keys;
  unsigned int           *hashes;
  void *                 *values;

  ExecHelpHashFunc        hash_func;
  ExecHelpEqualFunc       key_equal_func;
  int                     ref_count;
#ifndef EH_DISABLE_ASSERT
  /*
   * Tracks the structure of the hash table, not its contents: is only
   * incremented when a node is added or removed (is not incremented
   * when the key or data of a node is modified).
   */
  int                     version;
#endif
  ExecHelpDestroyNotify   key_destroy_func;
  ExecHelpDestroyNotify   value_destroy_func;
};

typedef struct
{
  ExecHelpHashTable  *hash_table;
  void *              dummy1;
  void *              dummy2;
  int                 position;
  int                 dummy3;
  int                 version;
} EHRealIter;

CASSERT(sizeof (ExecHelpHashTableIter) == sizeof (EHRealIter), hash_c);

/* Each table size has an associated prime modulo (the first prime
 * lower than the table size) used to find the initial bucket. Probing
 * then works modulo 2^n. The prime modulo is necessary to get a
 * good distribution with poor hash functions.
 */
static const int prime_mod [] =
{
  1,          /* For 1 << 0 */
  2,
  3,
  7,
  13,
  31,
  61,
  127,
  251,
  509,
  1021,
  2039,
  4093,
  8191,
  16381,
  32749,
  65521,      /* For 1 << 16 */
  131071,
  262139,
  524287,
  1048573,
  2097143,
  4194301,
  8388593,
  16777213,
  33554393,
  67108859,
  134217689,
  268435399,
  536870909,
  1073741789,
  2147483647  /* For 1 << 31 */
};

static void
exechelp_hash_table_set_shift (ExecHelpHashTable *hash_table, int shift)
{
  int i;
  unsigned int mask = 0;

  hash_table->size = 1 << shift;
  hash_table->mod  = prime_mod [shift];

  for (i = 0; i < shift; i++)
    {
      mask <<= 1;
      mask |= 1;
    }

  hash_table->mask = mask;
}

static int
exechelp_hash_table_find_closest_shift (int n)
{
  int i;

  for (i = 0; n; i++)
    n >>= 1;

  return i;
}

static void
exechelp_hash_table_set_shift_from_size (ExecHelpHashTable *hash_table, int size)
{
  int shift;

  shift = exechelp_hash_table_find_closest_shift (size);
  shift = (shift > HASH_TABLE_MIN_SHIFT) ? shift : HASH_TABLE_MIN_SHIFT;

  exechelp_hash_table_set_shift (hash_table, shift);
}

/*
 * exechelp_hash_table_lookup_node:
 * @hash_table: our #ExecHelpHashTable
 * @key: the key to lookup against
 * @hash_return: key hash return location
 *
 * Performs a lookup in the hash table, preserving extra information
 * usually needed for insertion.
 *
 * This function first computes the hash value of the key using the
 * user's hash function.
 *
 * If an entry in the table matching @key is found then this function
 * returns the index of that entry in the table, and if not, the
 * index of an unused node (empty or tombstone) where the key can be
 * inserted.
 *
 * The computed hash value is returned in the variable pointed to
 * by @hash_return. This is to save insertions from having to compute
 * the hash record again for the new record.
 *
 * Returns: index of the described node
 */
static inline unsigned int
exechelp_hash_table_lookup_node (ExecHelpHashTable    *hash_table,
                          const void *  key,
                          unsigned int         *hash_return)
{
  unsigned int node_index;
  unsigned int node_hash;
  unsigned int hash_value;
  unsigned int first_tombstone = 0;
  int have_tombstone = 0;
  unsigned int step = 0;

  /* If this happens, then the application is probably doing too much work
   * from a destroy notifier. The alternative would be to crash any second
   * (as keys, etc. will be NULL).
   * Applications need to either use exechelp_hash_table_destroy, or ensure the hash
   * table is empty prior to removing the last reference using exechelp_hash_table_unref(). */
  assert (hash_table->ref_count > 0);

  hash_value = hash_table->hash_func (key);
  if (!HASH_IS_REAL (hash_value))
    hash_value = 2;

  *hash_return = hash_value;

  node_index = hash_value % hash_table->mod;
  node_hash = hash_table->hashes[node_index];

  while (!HASH_IS_UNUSED (node_hash))
    {
      /* We first check if our full hash values
       * are equal so we can avoid calling the full-blown
       * key equality function in most cases.
       */
      if (node_hash == hash_value)
        {
          void * node_key = hash_table->keys[node_index];

          if (hash_table->key_equal_func)
            {
              if (hash_table->key_equal_func (node_key, key))
                return node_index;
            }
          else if (node_key == key)
            {
              return node_index;
            }
        }
      else if (HASH_IS_TOMBSTONE (node_hash) && !have_tombstone)
        {
          first_tombstone = node_index;
          have_tombstone = 1;
        }

      step++;
      node_index += step;
      node_index &= hash_table->mask;
      node_hash = hash_table->hashes[node_index];
    }

  if (have_tombstone)
    return first_tombstone;

  return node_index;
}

/*
 * exechelp_hash_table_remove_node:
 * @hash_table: our #ExecHelpHashTable
 * @node: pointer to node to remove
 * @notify: %1 if the destroy notify handlers are to be called
 *
 * Removes a node from the hash table and updates the node count.
 * The node is replaced by a tombstone. No table resize is performed.
 *
 * If @notify is %1 then the destroy notify functions are called
 * for the key and value of the hash node.
 */
static void
exechelp_hash_table_remove_node (ExecHelpHashTable   *hash_table,
                          int          i,
                          int      notify)
{
  void * key;
  void * value;

  key = hash_table->keys[i];
  value = hash_table->values[i];

  /* Erect tombstone */
  hash_table->hashes[i] = TOMBSTONE_HASH_VALUE;

  /* Be GC friendly */
  hash_table->keys[i] = NULL;
  hash_table->values[i] = NULL;

  hash_table->nnodes--;

  if (notify && hash_table->key_destroy_func)
    hash_table->key_destroy_func (key);

  if (notify && hash_table->value_destroy_func)
    hash_table->value_destroy_func (value);

}

/*
 * exechelp_hash_table_remove_all_nodes:
 * @hash_table: our #ExecHelpHashTable
 * @notify: %1 if the destroy notify handlers are to be called
 *
 * Removes all nodes from the table.  Since this may be a precursor to
 * freeing the table entirely, no resize is performed.
 *
 * If @notify is %1 then the destroy notify functions are called
 * for the key and value of the hash node.
 */
static void
exechelp_hash_table_remove_all_nodes (ExecHelpHashTable *hash_table,
                               int    notify,
                               int    destruction)
{
  int i;
  void * key;
  void * value;
  int old_size;
  void * *old_keys;
  void * *old_values;
  unsigned int    *old_hashes;

  /* If the hash table is already empty, there is nothing to be done. */
  if (hash_table->nnodes == 0)
    return;

  hash_table->nnodes = 0;
  hash_table->noccupied = 0;

  if (!notify ||
      (hash_table->key_destroy_func == NULL &&
       hash_table->value_destroy_func == NULL))
    {
      if (!destruction)
        {
          memset (hash_table->hashes, 0, hash_table->size * sizeof (unsigned int));
          memset (hash_table->keys, 0, hash_table->size * sizeof (void *));
          memset (hash_table->values, 0, hash_table->size * sizeof (void *));
        }

      return;
    }

  /* Keep the old storage space around to iterate over it. */
  old_size = hash_table->size;
  old_keys   = hash_table->keys;
  old_values = hash_table->values;
  old_hashes = hash_table->hashes;

  /* Now create a new storage space; If the table is destroyed we can use the
   * shortcut of not creating a new storage. This saves the allocation at the
   * cost of not allowing any recursive access.
   * However, the application doesn't own any reference anymore, so access
   * is not allowed. If accesses are done, then either an assert or crash
   * *will* happen. */
  exechelp_hash_table_set_shift (hash_table, HASH_TABLE_MIN_SHIFT);
  if (!destruction)
    {
      hash_table->keys   = exechelp_malloc0 (sizeof (void *) * hash_table->size);
      hash_table->values = hash_table->keys;
      hash_table->hashes = exechelp_malloc0 (sizeof (unsigned int) * hash_table->size);
    }
  else
    {
      hash_table->keys   = NULL;
      hash_table->values = NULL;
      hash_table->hashes = NULL;
    }

  for (i = 0; i < old_size; i++)
    {
      if (HASH_IS_REAL (old_hashes[i]))
        {
          key = old_keys[i];
          value = old_values[i];

          old_hashes[i] = UNUSED_HASH_VALUE;
          old_keys[i] = NULL;
          old_values[i] = NULL;

          if (hash_table->key_destroy_func == NULL)
            hash_table->key_destroy_func (key);

          if (hash_table->value_destroy_func != NULL)
            hash_table->value_destroy_func (value);
        }
    }

  /* Destroy old storage space. */
  if (old_keys != old_values)
    free (old_values);

  free (old_keys);
  free (old_hashes);
}

/*
 * exechelp_hash_table_resize:
 * @hash_table: our #ExecHelpHashTable
 *
 * Resizes the hash table to the optimal size based on the number of
 * nodes currently held. If you call this function then a resize will
 * occur, even if one does not need to occur.
 * Use exechelp_hash_table_maybe_resize() instead.
 *
 * This function may "resize" the hash table to its current size, with
 * the side effect of cleaning up tombstones and otherwise optimizing
 * the probe sequences.
 */
static void
exechelp_hash_table_resize (ExecHelpHashTable *hash_table)
{
  void * *new_keys;
  void * *new_values;
  unsigned int *new_hashes;
  int old_size;
  int i;

  old_size = hash_table->size;
  exechelp_hash_table_set_shift_from_size (hash_table, hash_table->nnodes * 2);

  new_keys = exechelp_malloc0 (sizeof (void *) * hash_table->size);
  if (hash_table->keys == hash_table->values)
    new_values = new_keys;
  else
    new_values = exechelp_malloc0 (sizeof (void *) * hash_table->size);
  new_hashes = exechelp_malloc0 (sizeof (unsigned int) * hash_table->size);

  for (i = 0; i < old_size; i++)
    {
      unsigned int node_hash = hash_table->hashes[i];
      unsigned int hash_val;
      unsigned int step = 0;

      if (!HASH_IS_REAL (node_hash))
        continue;

      hash_val = node_hash % hash_table->mod;

      while (!HASH_IS_UNUSED (new_hashes[hash_val]))
        {
          step++;
          hash_val += step;
          hash_val &= hash_table->mask;
        }

      new_hashes[hash_val] = hash_table->hashes[i];
      new_keys[hash_val] = hash_table->keys[i];
      new_values[hash_val] = hash_table->values[i];
    }

  if (hash_table->keys != hash_table->values)
    free (hash_table->values);

  free (hash_table->keys);
  free (hash_table->hashes);

  hash_table->keys = new_keys;
  hash_table->values = new_values;
  hash_table->hashes = new_hashes;

  hash_table->noccupied = hash_table->nnodes;
}

/*
 * exechelp_hash_table_maybe_resize:
 * @hash_table: our #ExecHelpHashTable
 *
 * Resizes the hash table, if needed.
 *
 * Essentially, calls exechelp_hash_table_resize() if the table has strayed
 * too far from its ideal size for its number of nodes.
 */
static inline void
exechelp_hash_table_maybe_resize (ExecHelpHashTable *hash_table)
{
  int noccupied = hash_table->noccupied;
  int size = hash_table->size;

  if ((size > hash_table->nnodes * 4 && size > 1 << HASH_TABLE_MIN_SHIFT) ||
      (size <= noccupied + (noccupied / 16)))
    exechelp_hash_table_resize (hash_table);
}

ExecHelpHashTable *
exechelp_hash_table_new (ExecHelpHashFunc  hash_func,
                  ExecHelpEqualFunc key_equal_func)
{
  return exechelp_hash_table_new_full (hash_func, key_equal_func, NULL, NULL);
}


ExecHelpHashTable *
exechelp_hash_table_new_full (ExecHelpHashFunc      hash_func,
                       ExecHelpEqualFunc     key_equal_func,
                       ExecHelpDestroyNotify key_destroy_func,
                       ExecHelpDestroyNotify value_destroy_func)
{
  ExecHelpHashTable *hash_table;

  hash_table = malloc (sizeof(ExecHelpHashTable));
  exechelp_hash_table_set_shift (hash_table, HASH_TABLE_MIN_SHIFT);
  hash_table->nnodes             = 0;
  hash_table->noccupied          = 0;
  hash_table->hash_func          = hash_func ? hash_func : exechelp_direct_hash;
  hash_table->key_equal_func     = key_equal_func;
  hash_table->ref_count          = 1;
#ifndef EH_DISABLE_ASSERT
  hash_table->version            = 0;
#endif
  hash_table->key_destroy_func   = key_destroy_func;
  hash_table->value_destroy_func = value_destroy_func;
  hash_table->keys               = exechelp_malloc0 (sizeof (void *) * hash_table->size);
  hash_table->values             = hash_table->keys;
  hash_table->hashes             = exechelp_malloc0 (sizeof (unsigned int) * hash_table->size);

  return hash_table;
}

void
exechelp_hash_table_iter_init (ExecHelpHashTableIter *iter,
                        ExecHelpHashTable     *hash_table)
{
  EHRealIter *ri = (EHRealIter *) iter;

  if (iter == NULL || hash_table == NULL)
    return;

  ri->hash_table = hash_table;
  ri->position = -1;
#ifndef EH_DISABLE_ASSERT
  ri->version = hash_table->version;
#endif
}

int
exechelp_hash_table_iter_next (ExecHelpHashTableIter *iter,
                        void *       *key,
                        void *       *value)
{
  EHRealIter *ri = (EHRealIter *) iter;
  int position;

  if (iter == NULL || ri->version != ri->hash_table->version || ri->position >= ri->hash_table->size)
    return 0;

  position = ri->position;

  do
    {
      position++;
      if (position >= ri->hash_table->size)
        {
          ri->position = position;
          return 0;
        }
    }
  while (!HASH_IS_REAL (ri->hash_table->hashes[position]));

  if (key != NULL)
    *key = ri->hash_table->keys[position];
  if (value != NULL)
    *value = ri->hash_table->values[position];

  ri->position = position;
  return 1;
}

ExecHelpHashTable *
exechelp_hash_table_iter_get_hash_table (ExecHelpHashTableIter *iter)
{
  if (!iter)
    return NULL;

  return ((EHRealIter *) iter)->hash_table;
}

static void
iter_remove_or_steal (EHRealIter *ri, int notify)
{
  if (!ri || ri->version != ri->hash_table->version || ri->position < 0 || ri->position >= ri->hash_table->size)
    return;

  exechelp_hash_table_remove_node (ri->hash_table, ri->position, notify);

#ifndef EH_DISABLE_ASSERT
  ri->version++;
  ri->hash_table->version++;
#endif
}

void
exechelp_hash_table_iter_remove (ExecHelpHashTableIter *iter)
{
  iter_remove_or_steal ((EHRealIter *) iter, 1);
}

static int
exechelp_hash_table_insert_node (ExecHelpHashTable *hash_table,
                          unsigned int       node_index,
                          unsigned int       key_hash,
                          void *    new_key,
                          void *    new_value,
                          int    keep_new_key,
                          int    reusing_key)
{
  int already_exists;
  unsigned int old_hash;
  void * key_to_free = NULL;
  void * value_to_free = NULL;

  old_hash = hash_table->hashes[node_index];
  already_exists = HASH_IS_REAL (old_hash);

  /* Proceed in three steps.  First, deal with the key because it is the
   * most complicated.  Then consider if we need to split the table in
   * two (because writing the value will result in the set invariant
   * becoming broken).  Then deal with the value.
   *
   * There are three cases for the key:
   *
   *  - entry already exists in table, reusing key:
   *    free the just-passed-in new_key and use the existing value
   *
   *  - entry already exists in table, not reusing key:
   *    free the entry in the table, use the new key
   *
   *  - entry not already in table:
   *    use the new key, free nothing
   *
   * We update the hash at the same time...
   */
  if (already_exists)
    {
      /* Note: we must record the old value before writing the new key
       * because we might change the value in the event that the two
       * arrays are shared.
       */
      value_to_free = hash_table->values[node_index];

      if (keep_new_key)
        {
          key_to_free = hash_table->keys[node_index];
          hash_table->keys[node_index] = new_key;
        }
      else
        key_to_free = new_key;
    }
  else
    {
      hash_table->hashes[node_index] = key_hash;
      hash_table->keys[node_index] = new_key;
    }

  /* Step two: check if the value that we are about to write to the
   * table is the same as the key in the same position.  If it's not,
   * split the table.
   */
  if ((hash_table->keys == hash_table->values && hash_table->keys[node_index] != new_value))
    hash_table->values = exechelp_memdup (hash_table->keys, sizeof (void *) * hash_table->size);

  /* Step 3: Actually do the write */
  hash_table->values[node_index] = new_value;

  /* Now, the bookkeeping... */
  if (!already_exists)
    {
      hash_table->nnodes++;

      if (HASH_IS_UNUSED (old_hash))
        {
          /* We replaced an empty node, and not a tombstone */
          hash_table->noccupied++;
          exechelp_hash_table_maybe_resize (hash_table);
        }

#ifndef EH_DISABLE_ASSERT
      hash_table->version++;
#endif
    }

  if (already_exists)
    {
      if (hash_table->key_destroy_func && !reusing_key)
        (* hash_table->key_destroy_func) (key_to_free);
      if (hash_table->value_destroy_func)
        (* hash_table->value_destroy_func) (value_to_free);
    }

  return !already_exists;
}

void
exechelp_hash_table_iter_replace (ExecHelpHashTableIter *iter,
                           void *        value)
{
  EHRealIter *ri;
  unsigned int node_hash;
  void * key;

  ri = (EHRealIter *) iter;


  if (!ri || ri->version != ri->hash_table->version || ri->position < 0 || ri->position >= ri->hash_table->size)
    return;

  node_hash = ri->hash_table->hashes[ri->position];
  key = ri->hash_table->keys[ri->position];

  exechelp_hash_table_insert_node (ri->hash_table, ri->position, node_hash, key, value, 1, 1);

#ifndef EH_DISABLE_ASSERT
  ri->version++;
  ri->hash_table->version++;
#endif
}

void
exechelp_hash_table_iter_steal (ExecHelpHashTableIter *iter)
{
  iter_remove_or_steal ((EHRealIter *) iter, 0);
}


ExecHelpHashTable *
exechelp_hash_table_ref (ExecHelpHashTable *hash_table)
{
  if(!hash_table)
    return NULL;

  __sync_add_and_fetch(&hash_table->ref_count, 1);

  return hash_table;
}

void
exechelp_hash_table_unref (ExecHelpHashTable *hash_table)
{
  if(!hash_table)
    return;

  if (__sync_sub_and_fetch(&hash_table->ref_count, 1) == 0)
    {
      exechelp_hash_table_remove_all_nodes (hash_table, 1, 1);
      if (hash_table->keys != hash_table->values)
        free (hash_table->values);
      free (hash_table->keys);
      free (hash_table->hashes);
      free (hash_table);
    }
}

void
exechelp_hash_table_destroy (ExecHelpHashTable *hash_table)
{
  if(!hash_table)
    return;

  exechelp_hash_table_remove_all (hash_table);
  exechelp_hash_table_unref (hash_table);
}

void *
exechelp_hash_table_lookup (ExecHelpHashTable    *hash_table,
                     const void *  key)
{
  unsigned int node_index;
  unsigned int node_hash;

  if(!hash_table)
    return NULL;

  node_index = exechelp_hash_table_lookup_node (hash_table, key, &node_hash);

  return HASH_IS_REAL (hash_table->hashes[node_index])
    ? hash_table->values[node_index]
    : NULL;
}

int
exechelp_hash_table_lookup_extended (ExecHelpHashTable    *hash_table,
                              const void *  lookup_key,
                              void *      *oriexechelp_key,
                              void *      *value)
{
  unsigned int node_index;
  unsigned int node_hash;

  if(!hash_table)
    return 0;

  node_index = exechelp_hash_table_lookup_node (hash_table, lookup_key, &node_hash);

  if (!HASH_IS_REAL (hash_table->hashes[node_index]))
    return 0;

  if (oriexechelp_key)
    *oriexechelp_key = hash_table->keys[node_index];

  if (value)
    *value = hash_table->values[node_index];

  return 1;
}

/*
 * exechelp_hash_table_insert_internal:
 * @hash_table: our #ExecHelpHashTable
 * @key: the key to insert
 * @value: the value to insert
 * @keep_new_key: if %1 and this key already exists in the table
 *   then call the destroy notify function on the old key.  If %0
 *   then call the destroy notify function on the new key.
 *
 * Implements the common logic for the exechelp_hash_table_insert() and
 * exechelp_hash_table_replace() functions.
 *
 * Do a lookup of @key. If it is found, replace it with the new
 * @value (and perhaps the new @key). If it is not found, create
 * a new node.
 *
 * Returns: %1 if the key did not exist yet
 */
static int
exechelp_hash_table_insert_internal (ExecHelpHashTable *hash_table,
                              void *    key,
                              void *    value,
                              int    keep_new_key)
{
  unsigned int key_hash;
  unsigned int node_index;

  if(!hash_table)
    return 0;

  node_index = exechelp_hash_table_lookup_node (hash_table, key, &key_hash);

  return exechelp_hash_table_insert_node (hash_table, node_index, key_hash, key, value, keep_new_key, 0);
}

int
exechelp_hash_table_insert (ExecHelpHashTable *hash_table,
                     void *    key,
                     void *    value)
{
  return exechelp_hash_table_insert_internal (hash_table, key, value, 0);
}

int
exechelp_hash_table_replace (ExecHelpHashTable *hash_table,
                      void *    key,
                      void *    value)
{
  return exechelp_hash_table_insert_internal (hash_table, key, value, 1);
}

int
exechelp_hash_table_add (ExecHelpHashTable *hash_table,
                  void *    key)
{
  return exechelp_hash_table_insert_internal (hash_table, key, key, 1);
}

int
exechelp_hash_table_contains (ExecHelpHashTable    *hash_table,
                       const void *  key)
{
  unsigned int node_index;
  unsigned int node_hash;

  if(!hash_table)
    return 0;

  node_index = exechelp_hash_table_lookup_node (hash_table, key, &node_hash);

  return HASH_IS_REAL (hash_table->hashes[node_index]);
}

/*
 * exechelp_hash_table_remove_internal:
 * @hash_table: our #ExecHelpHashTable
 * @key: the key to remove
 * @notify: %1 if the destroy notify handlers are to be called
 * Returns: %1 if a node was found and removed, else %0
 *
 * Implements the common logic for the exechelp_hash_table_remove() and
 * exechelp_hash_table_steal() functions.
 *
 * Do a lookup of @key and remove it if it is found, calling the
 * destroy notify handlers only if @notify is %1.
 */
static int
exechelp_hash_table_remove_internal (ExecHelpHashTable    *hash_table,
                              const void *  key,
                              int       notify)
{
  unsigned int node_index;
  unsigned int node_hash;

  if(!hash_table)
    return 0;

  node_index = exechelp_hash_table_lookup_node (hash_table, key, &node_hash);

  if (!HASH_IS_REAL (hash_table->hashes[node_index]))
    return 0;

  exechelp_hash_table_remove_node (hash_table, node_index, notify);
  exechelp_hash_table_maybe_resize (hash_table);

#ifndef EH_DISABLE_ASSERT
  hash_table->version++;
#endif

  return 1;
}

int
exechelp_hash_table_remove (ExecHelpHashTable    *hash_table,
                     const void *  key)
{
  return exechelp_hash_table_remove_internal (hash_table, key, 1);
}

int
exechelp_hash_table_steal (ExecHelpHashTable    *hash_table,
                    const void *  key)
{
  return exechelp_hash_table_remove_internal (hash_table, key, 0);
}

void
exechelp_hash_table_remove_all (ExecHelpHashTable *hash_table)
{
  if(!hash_table)
    return;

#ifndef EH_DISABLE_ASSERT
  if (hash_table->nnodes != 0)
    hash_table->version++;
#endif

  exechelp_hash_table_remove_all_nodes (hash_table, 1, 0);
  exechelp_hash_table_maybe_resize (hash_table);
}

void
exechelp_hash_table_steal_all (ExecHelpHashTable *hash_table)
{
  if(!hash_table)
    return;

#ifndef EH_DISABLE_ASSERT
  if (hash_table->nnodes != 0)
    hash_table->version++;
#endif

  exechelp_hash_table_remove_all_nodes (hash_table, 0, 0);
  exechelp_hash_table_maybe_resize (hash_table);
}

static unsigned int
exechelp_hash_table_foreach_remove_or_steal (ExecHelpHashTable *hash_table,
                                      ExecHelpHRFunc     func,
                                      void *    user_data,
                                      int    notify)
{
  unsigned int deleted = 0;
  int i;
#ifndef EH_DISABLE_ASSERT
  int version = hash_table->version;
#endif

  for (i = 0; i < hash_table->size; i++)
    {
      unsigned int node_hash = hash_table->hashes[i];
      void * node_key = hash_table->keys[i];
      void * node_value = hash_table->values[i];

      if (HASH_IS_REAL (node_hash) &&
          (* func) (node_key, node_value, user_data))
        {
          exechelp_hash_table_remove_node (hash_table, i, notify);
          deleted++;
        }

#ifndef EH_DISABLE_ASSERT
      if (version != hash_table->version)
        return 0;
#endif
    }

  exechelp_hash_table_maybe_resize (hash_table);

#ifndef EH_DISABLE_ASSERT
  if (deleted > 0)
    hash_table->version++;
#endif

  return deleted;
}

unsigned int
exechelp_hash_table_foreach_remove (ExecHelpHashTable *hash_table,
                             ExecHelpHRFunc     func,
                             void *    user_data)
{
  if(!hash_table || !func)
    return 0;

  return exechelp_hash_table_foreach_remove_or_steal (hash_table, func, user_data, 1);
}

unsigned int
exechelp_hash_table_foreach_steal (ExecHelpHashTable *hash_table,
                            ExecHelpHRFunc     func,
                            void *    user_data)
{
  if(!hash_table || !func)
    return 0;
  
  return exechelp_hash_table_foreach_remove_or_steal (hash_table, func, user_data, 0);
}

void
exechelp_hash_table_foreach (ExecHelpHashTable *hash_table,
                      ExecHelpHFunc      func,
                      void *    user_data)
{
  int i;
#ifndef EH_DISABLE_ASSERT
  int version;
#endif

  if(!hash_table || !func)
    return;

#ifndef EH_DISABLE_ASSERT
  version = hash_table->version;
#endif

  for (i = 0; i < hash_table->size; i++)
    {
      unsigned int node_hash = hash_table->hashes[i];
      void * node_key = hash_table->keys[i];
      void * node_value = hash_table->values[i];

      if (HASH_IS_REAL (node_hash))
        (* func) (node_key, node_value, user_data);

#ifndef EH_DISABLE_ASSERT
      if (version != hash_table->version)
        return;
#endif
    }
}

void *
exechelp_hash_table_find (ExecHelpHashTable *hash_table,
                   ExecHelpHRFunc     predicate,
                   void *    user_data)
{
  int i;
#ifndef EH_DISABLE_ASSERT
  int version;
#endif
  int match;

  if(!hash_table || !predicate)
    return NULL;

#ifndef EH_DISABLE_ASSERT
  version = hash_table->version;
#endif

  match = 0;

  for (i = 0; i < hash_table->size; i++)
    {
      unsigned int node_hash = hash_table->hashes[i];
      void * node_key = hash_table->keys[i];
      void * node_value = hash_table->values[i];

      if (HASH_IS_REAL (node_hash))
        match = predicate (node_key, node_value, user_data);

#ifndef EH_DISABLE_ASSERT
      if (version != hash_table->version)
        return NULL;
#endif

      if (match)
        return node_value;
    }

  return NULL;
}

unsigned int
exechelp_hash_table_size (ExecHelpHashTable *hash_table)
{
  if(!hash_table)
    return 0;

  return hash_table->nnodes;
}

ExecHelpList *
exechelp_hash_table_get_keys (ExecHelpHashTable *hash_table)
{
  int i;
  ExecHelpList *retval;

  if(!hash_table)
    return NULL;

  retval = NULL;
  for (i = 0; i < hash_table->size; i++)
    {
      if (HASH_IS_REAL (hash_table->hashes[i]))
        retval = exechelp_list_prepend (retval, hash_table->keys[i]);
    }

  return retval;
}

void * *
exechelp_hash_table_get_keys_as_array (ExecHelpHashTable *hash_table,
                                unsigned int      *length)
{
  void * *result;
  unsigned int i, j = 0;

  result = malloc (sizeof(void *) * hash_table->nnodes + 1);
  for (i = 0; i < hash_table->size; i++)
    {
      if (HASH_IS_REAL (hash_table->hashes[i]))
        result[j++] = hash_table->keys[i];
    }
  //exechelp_assert_cmpint (j, ==, hash_table->nnodes);
  result[j] = NULL;

  if (length)
    *length = j;

  return result;
}

ExecHelpList *
exechelp_hash_table_get_values (ExecHelpHashTable *hash_table)
{
  int i;
  ExecHelpList *retval;

  if(hash_table == NULL)
    return NULL;

  retval = NULL;
  for (i = 0; i < hash_table->size; i++)
    {
      if (HASH_IS_REAL (hash_table->hashes[i]))
        retval = exechelp_list_prepend (retval, hash_table->values[i]);
    }

  return retval;
}

/* Hash functions.
 */

int
exechelp_str_equal (const void * v1,
             const void * v2)
{
  const char *string1 = v1;
  const char *string2 = v2;

  return strcmp (string1, string2) == 0;
}

unsigned int
exechelp_str_hash (const void * v)
{
  const signed char *p;
  uint32_t h = 5381;

  for (p = v; *p != '\0'; p++)
    h = (h << 5) + h + *p;

  return h;
}

unsigned int
exechelp_direct_hash (const void * v)
{
  return EH_POINTER_TO_UINT (v);
}

int
exechelp_direct_equal (const void * v1,
                const void * v2)
{
  return v1 == v2;
}

int
exechelp_int_equal (const void * v1,
             const void * v2)
{
  return *((const int*) v1) == *((const int*) v2);
}

unsigned int
exechelp_int_hash (const void * v)
{
  return *(const int*) v;
}

int
exechelp_int64_equal (const void * v1,
               const void * v2)
{
  return *((const int64_t*) v1) == *((const int64_t*) v2);
}

unsigned int
exechelp_int64_hash (const void * v)
{
  return (unsigned int) *(const int64_t*) v;
}

int
exechelp_double_equal (const void * v1,
                const void * v2)
{
  return *((const double*) v1) == *((const double*) v2);
}

unsigned int
exechelp_double_hash (const void * v)
{
  return (unsigned int) *(const double*) v;
}
