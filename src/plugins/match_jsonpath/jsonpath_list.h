/* SPDX-License-Identifier: GPL-2.0-only OR PostgreSQL                                  */
/* SPDX-FileCopyrightText: Copyright (c) 1996-2022, PostgreSQL Global Development Group */
/* SPDX-FileCopyrightText: Copyright (c) 1994, Regents of the University of California  */

#pragma once

typedef struct {
    void *ptr_value;
} jsonpath_list_cell_t;

typedef struct {
    int length;                      /* number of elements currently present */
    int max_length;                  /* allocated length of elements[] */
    jsonpath_list_cell_t *elements;  /* re-allocatable array of cells */
    /* We may allocate some cells along with the List header: */
    jsonpath_list_cell_t initial_elements[];
    /* If elements == initial_elements, it's not a separate allocation */
} jsonpath_list_t;

static inline int jsonpath_list_length(const jsonpath_list_t *l)
{
    return l ? l->length : 0;
}

static inline jsonpath_list_cell_t *jsonpath_list_head(const jsonpath_list_t *l)
{
    return l ? &l->elements[0] : NULL;
}

static inline jsonpath_list_cell_t *jsonpath_list_second_cell(const jsonpath_list_t *l)
{
    if (l && l->length >= 2)
        return &l->elements[1];
    else
        return NULL;
}

static inline jsonpath_list_cell_t *jsonpath_list_nth_cell(const jsonpath_list_t *list, int n)
{
    return &list->elements[n];
}


static inline jsonpath_list_cell_t *jsonpath_list_last_cell(const jsonpath_list_t *list)
{
//    Assert(list != NIL);
    return &list->elements[list->length - 1];
}

#define jsonpath_list_first(lc)    ((lc)->ptr_value)
#define jsonpath_list_initial(l)   jsonpath_list_first(jsonpath_list_nth_cell(l, 0))
#define jsonpath_list_last(l)      jsonpath_list_first(jsonpath_list_last_cell(l))

static int jsonpath_list_enlarge(jsonpath_list_t *list, int min_size)
{
    int new_max_len = min_size;

    if (list->elements == list->initial_elements) {
        /*
         * Replace original in-line allocation with a separate palloc block.
         * Ensure it is in the same memory context as the List header.  (The
         * previous List implementation did not offer any guarantees about
         * keeping all list cells in the same context, but it seems reasonable
         * to create such a guarantee now.)
         */
        list->elements = calloc(1, new_max_len * sizeof(jsonpath_list_cell_t));
        if (list->elements == NULL)
            return -1;
        memcpy(list->elements, list->initial_elements, list->length * sizeof(jsonpath_list_cell_t));
    } else {
        /* Normally, let repalloc deal with enlargement */
        jsonpath_list_cell_t *tmp = realloc(list->elements,
                                            new_max_len * sizeof(jsonpath_list_cell_t));
        if (tmp == NULL)
            return -1;
        list->elements = tmp;
    }

    list->max_length = new_max_len;
    return 0;
}

static int jsonpath_list_new_tail_cell(jsonpath_list_t *list)
{
    /* Enlarge array if necessary */
    if (list->length >= list->max_length) {
        if (jsonpath_list_enlarge(list, list->length + 1) != 0)
            return -1;
    }
    list->length++;
    return 0;
}

static inline jsonpath_list_t *jsonpath_list_new(int min_size)
{
    int max_size = min_size;

    jsonpath_list_t *list = calloc(1, sizeof(*list) + max_size * sizeof(jsonpath_list_cell_t));
    list->length = min_size;
    list->max_length = max_size;
    list->elements = list->initial_elements;

    return list;
}

static inline jsonpath_list_t *jsonpath_list_append(jsonpath_list_t *list, void *datum)
{
    if (list == NULL) {
        list = jsonpath_list_new(1);
    } else {
        if (jsonpath_list_new_tail_cell(list) != 0)
            return list;
    }

    jsonpath_list_last(list) = datum;
    return list;
}

static inline jsonpath_list_t *jsonpath_list_prepend(jsonpath_list_t *list, void *datum)
{
    if (list == NULL) {
        list = jsonpath_list_new(1);
        jsonpath_list_last(list) = datum;
        return list;
    }

    if (list->length >= list->max_length)
        jsonpath_list_enlarge(list, list->length + 1);

    memmove(list->elements + 1, list->elements, sizeof(jsonpath_list_cell_t) * list->length);
    list->elements[0].ptr_value = datum;
    list->length++;
    return list;
}

static inline jsonpath_list_cell_t *jsonpath_list_next(jsonpath_list_t *l, jsonpath_list_cell_t *c)
{
    c++;
    if (c < &l->elements[l->length])
        return c;
    else
        return NULL;
}

static inline void jsonpath_list_free(jsonpath_list_t *list)
{
    if (list == NULL)
        return;

    if (list->elements != list->initial_elements)
        free(list->elements);

    free(list);
}

static inline jsonpath_list_t *jsonpath_list_delete_nth_cell(jsonpath_list_t *list, int n)
{
    if (list->length == 1) {
        jsonpath_list_free(list);
        return NULL;
    }

    memmove(&list->elements[n], &list->elements[n + 1],
                                (list->length - 1 - n) * sizeof(jsonpath_list_cell_t));
    list->length--;

    return list;
}

static inline jsonpath_list_t *jsonpath_list_delete_first(jsonpath_list_t *list)
{
    if (list == NULL)
        return NULL;

    return jsonpath_list_delete_nth_cell(list, 0);
}

static inline jsonpath_list_t *jsonpath_list_make1(void *datum1)
{
    jsonpath_list_t *list = jsonpath_list_new(1);

    list->elements[0] = (jsonpath_list_cell_t) {.ptr_value = (datum1)};
    return list;
}

static inline jsonpath_list_t *jsonpath_list_make2(void *datum1, void *datum2)
{
    jsonpath_list_t *list = jsonpath_list_new(2);

    list->elements[0] = (jsonpath_list_cell_t) {.ptr_value = (datum1)};
    list->elements[1] = (jsonpath_list_cell_t) {.ptr_value = (datum2)};
    return list;
}

typedef struct {
    const jsonpath_list_t *l;  /* list we're looping through */
    int         i;             /* current element index */
} jsonpath_list_for_each_state_t;

#define for_each_from(cell, lst, N) \
    for (jsonpath_list_for_each_state_t cell##__state = for_each_from_setup(lst, N); \
         (cell##__state.l != NULL && \
          cell##__state.i < cell##__state.l->length) ? \
         (cell = &cell##__state.l->elements[cell##__state.i], true) : \
         (cell = NULL, false); \
         cell##__state.i++)

static inline jsonpath_list_for_each_state_t for_each_from_setup(const jsonpath_list_t *lst, int N)
{
    jsonpath_list_for_each_state_t r = {lst, N};
    return r;
}

#define foreach(cell, lst)  \
    for (jsonpath_list_for_each_state_t cell##__state = {(lst), 0}; \
         (cell##__state.l != NULL && \
          cell##__state.i < cell##__state.l->length) ? \
         (cell = &cell##__state.l->elements[cell##__state.i], true) : \
         (cell = NULL, false); \
         cell##__state.i++)
