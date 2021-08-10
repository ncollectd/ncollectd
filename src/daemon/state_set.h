/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef STATE_SET_H
#define STATE_SET_H 1

typedef struct {
  bool enabled;
  char *name;
} state_t;

typedef struct {
  size_t num;
  state_t *ptr;
} state_set_t;

state_t *state_set_read(state_set_t set, char const *name);

int state_set_create(state_set_t *set, char const *name, bool enabled);

int state_set_add(state_set_t *set, char const *name, bool enabled);

void state_set_reset(state_set_t *set);

int state_set_clone(state_set_t *dest, state_set_t src);

#endif
