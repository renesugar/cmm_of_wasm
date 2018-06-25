/*
 * Copyright 2018 WebAssembly Community Group participants
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "wasm-rt-impl.h"

#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#define PAGE_SIZE 65536

typedef struct FuncType {
  wasm_rt_type_t* params;
  wasm_rt_type_t* results;
  uint32_t param_count;
  uint32_t result_count;
} FuncType;

uint32_t wasm_rt_call_stack_depth;

jmp_buf g_jmp_buf;
FuncType* g_func_types;
uint32_t g_func_type_count;

void wasm_rt_trap(wasm_rt_trap_t code) {
  assert(code != WASM_RT_TRAP_NONE);
  longjmp(g_jmp_buf, code);
}

static bool func_types_are_equal(FuncType* a, FuncType* b) {
  if (a->param_count != b->param_count || a->result_count != b->result_count)
    return 0;
  int i;
  for (i = 0; i < a->param_count; ++i)
    if (a->params[i] != b->params[i])
      return 0;
  for (i = 0; i < a->result_count; ++i)
    if (a->results[i] != b->results[i])
      return 0;
  return 1;
}

uint32_t wasm_rt_register_func_type(uint32_t param_count,
                                    uint32_t result_count,
                                    ...) {
  FuncType func_type;
  func_type.param_count = param_count;
  func_type.params = malloc(param_count * sizeof(wasm_rt_type_t));
  func_type.result_count = result_count;
  func_type.results = malloc(result_count * sizeof(wasm_rt_type_t));

  va_list args;
  va_start(args, result_count);

  uint32_t i;
  for (i = 0; i < param_count; ++i)
    func_type.params[i] = va_arg(args, wasm_rt_type_t);
  for (i = 0; i < result_count; ++i)
    func_type.results[i] = va_arg(args, wasm_rt_type_t);
  va_end(args);

  for (i = 0; i < g_func_type_count; ++i) {
    if (func_types_are_equal(&g_func_types[i], &func_type)) {
      free(func_type.params);
      free(func_type.results);
      return i + 1;
    }
  }

  uint32_t idx = g_func_type_count++;
  g_func_types = realloc(g_func_types, g_func_type_count * sizeof(FuncType));
  g_func_types[idx] = func_type;
  return idx + 1;
}

void wasm_rt_allocate_memory(wasm_rt_memory_t* memory,
                             uint32_t initial_pages,
                             uint32_t max_pages) {
  memory->pages = initial_pages;
  memory->max_pages = max_pages;
  memory->size = initial_pages * PAGE_SIZE;
  memory->data = calloc(memory->size, 1);
}

uint32_t wasm_rt_grow_memory(wasm_rt_memory_t* memory, uint32_t delta) {
  uint32_t old_pages = memory->pages;
  uint32_t new_pages = memory->pages + delta;
  if (new_pages < old_pages || new_pages > memory->max_pages) {
    return (uint32_t)-1;
  }
  memory->data = realloc(memory->data, new_pages);
  memory->pages = new_pages;
  memory->size = new_pages * PAGE_SIZE;
  return old_pages;
}

void wasm_rt_allocate_table(wasm_rt_table_t* table,
                            uint32_t elements,
                            uint32_t max_elements) {
  table->size = elements;
  table->max_size = max_elements;
  table->data = calloc(table->size, sizeof(wasm_rt_elem_t));
}


/* Primitives not provided by CMM that we're implementing as part of the
 * RTS. Hopefully this section will shrink with time. */

u32 wasm_rt_popcount_u32(u32 i) {
  return __builtin_popcount(i);
}

u64 wasm_rt_popcount_u64(u64 i) {
  return __builtin_popcountll(i);
}

u32 wasm_rt_clz_u32(u32 i) {
  if (i == 0) {
    return 32;
  }
  return __builtin_clz(i);
}

u64 wasm_rt_clz_u64(u64 i) {
  if (i == 0) {
    return 64;
  }
  return __builtin_clzll(i);
}

u32 wasm_rt_ctz_u32(u32 i) {
  if (i == 0) {
    return 32;
  }
  return __builtin_ctz(i);
}

u64 wasm_rt_ctz_u64(u64 i) {
  if (i == 0) {
    return 64;
  }
  return __builtin_ctzll(i);
}

f64 wasm_rt_nearest_f64(f64 f) {
  if (f > -1.0 && f < 0.0) {
    return -0.0;
  } else if (f > 0.0 && f < 1.0) {
    return 0.0;
  } else {
    return round(f);
  }
}

/* This is *hideous*. */
f32 wasm_rt_zero_min_f32(f32 f1, f32 f2) {
  if (signbit(f1)) {
    return f1;
  }
  return f2;
}

f32 wasm_rt_zero_max_f32(f32 f1, f32 f2) {
  if (signbit(f1)) {
    return f2;
  }
  return f1;
}

f64 wasm_rt_zero_min_f64(f64 f1, f64 f2) {
  if (signbit(f1)) {
    return f1;
  }
  return f2;
}

f64 wasm_rt_zero_max_f64(f64 f1, f64 f2) {
  if (signbit(f1)) {
    return f2;
  }
  return f1;
}

f32 wasm_rt_load_f32(wasm_rt_memory_t* mem, u64 offset) {
  f32 result;
  memcpy(&result, mem->data + offset, sizeof(result));
  return result;
}

void wasm_rt_store_f32(wasm_rt_memory_t* mem, u64 offset, f32 to_store) {
  memcpy(mem->data + offset, &to_store, sizeof(f32));
}

