#include "emscripten_rts.h"
// Syscall stuff

// Variadic arguments scaffolding
uint64_t varargs_position = 0;

uint32_t heap_read_int32(uint32_t ptr) {
  // NOTE: I'm not entirely sure what the >>2 in the
  // generated JS is all about. My guess is that it's an
  // artefact of the array encoding, so I'm ignoring it for
  // now.
  uint8_t* heap_base = env_memory_memory.data;
  uint32_t ret = *((uint32_t*)(heap_base + ptr));
  return ret;
}

uint64_t heap_read_int64(uint32_t ptr) {
  uint8_t* heap_base = env_memory_memory.data;
  uint64_t ret = *((uint64_t*)(heap_base + ptr));
  return ret;
}

uint32_t varargs_get() {
  uint32_t ret = heap_read_int32(varargs_position);
  varargs_position += 4;
  return ret;
}

char* varargs_get_str() {
  uint32_t ptr = heap_read_int32(varargs_position);
  uint8_t* heap_base = env_memory_memory.data;
  // TODO: This doesn't properly handle UTF8.
  char* ret = (char*) (heap_base + ptr);
  return ret;
}

uint64_t varargs_get64() {
  uint64_t ret = heap_read_int64(varargs_position);
  varargs_position += 8;
  return ret;
}

uint32_t varargs_get_zero() {
  uint32_t ret = varargs_get();
  assert(ret == 0);
  return ret;
}

// Syscall implementations
uint32_t env_cfunc____syscall54(uint32_t which, uint32_t varargs) {
  varargs_position = varargs;
  // ioctl
  return 0;
}

uint32_t env_cfunc____syscall6(uint32_t which, uint32_t varargs) {
  varargs_position = varargs;
  varargs_get(); // close
  // close
  // Since we're going to be working on either stdout, stderr, or stdin,
  // it doesn't make a whole lot of sense to close them...
  return 0;
}

uint32_t env_cfunc____syscall140(uint32_t which, uint32_t varargs) {
  varargs_position = varargs;
  // llseek
  // I can't see how the JS version of this even works -- SYSCALLS.getStreamFromFD()
  // and FS.llseek are *both* undefined.
  // So anyway, I'm gonna see what happens if I no-op this as well. Of course,
  // we need to perform the necessary varargs side-effects.
  varargs_get(); // stream
  varargs_get(); // offset_high
  varargs_get(); // offset_low
  varargs_get(); // result
  varargs_get(); // whence
  return 0;
}

uint32_t env_cfunc____syscall146(uint32_t which, uint32_t varargs) {
  varargs_position = varargs;
  // writev
  uint32_t stream = varargs_get();
  uint32_t iov = varargs_get(); // Base address of iovec array
  uint32_t iovcnt = varargs_get();
  uint32_t ret = 0;

  // Again, not doing UTF8 properly here.
  for (int i = 0; i < iovcnt; i++) {
    // Starting address
    // Base pointer. Index i is buffer number, each struct is 8 bytes.
    uint32_t ptr = heap_read_int32(iov + (i * 8));
    // Length: second int in the struct
    uint32_t len = heap_read_int32(iov + (i * 8) + 4);
    write(stream, (void*) (env_memory_memory.data + ptr), len);
    ret = ret + len;
  }
  return ret;
}

uint32_t env_cfunc__emscripten_memcpy_big(uint32_t dest,
    uint32_t src, uint32_t len) {
  uint8_t* heap_base = env_memory_memory.data;
  memcpy(heap_base + dest, heap_base + src, len);
  return dest;
}

// Aborting and errors
void err(char* str) {
  puts(str);
}

void rts_abort(char* str) {
  env_global_ABORT = 1;
  env_global_EXITSTATUS = 1;
  printf("abort: %s\n", str);
  exit(-1);
}

void env_cfunc_abort(char* str) {
  printf("abort\n");
  exit(-1);
}

void env_cfunc_nullFunc_ii(char* x) {
  err("Null pointer exception (ii)\n");
  rts_abort(x);
}

void env_cfunc_nullFunc_iiii(char* x) {
  err("Null pointer exception (iiii)\n");
  rts_abort(x);
}

void env_cfunc____assert_fail(char* condition, char* filename,
    int line, void* func) {
  char buf[100];
  sprintf(buf, "Assertion failed: %s, at %s:%d (%p)\n", condition,
      filename, line, func);
  rts_abort(buf);
}

void env_cfunc___exit(uint32_t status) {
  exit(status);
}

void env_cfunc__exit(uint32_t status) {
  exit(status);
}

uint32_t env_cfunc____setErrNo(uint32_t value) {
  // FIXME: This should call XXX_cfunc____errno_location() to
  // ascertain the error number location, and then write the
  // error number to the result >>2 in the heap.
  return value;
}


// Wrappers
uint32_t env_cfunc__gettimeofday(uint32_t ptr) {
  uint8_t* heap_base = env_memory_memory.data;
    struct timeval time_struct;
  gettimeofday(&time_struct, NULL);
  *((uint32_t*) (heap_base + ptr)) = (uint32_t) time_struct.tv_sec;
  *((uint32_t*) (heap_base + ptr + 4)) = (uint32_t) time_struct.tv_usec;
  return 0;
}

// I guess these were needed for pthreads support?
// Nothing in our output anyway.
void env_cfunc____lock() {
}

void env_cfunc____unlock() {
}

void env_cfunc_abortOnCannotGrowMemory() {
  rts_abort("Memory is static for now.");
}

void env_cfunc_enlargeMemory() {
  env_cfunc_abortOnCannotGrowMemory();
}

uint32_t env_cfunc_getTotalMemory() {
  uint32_t ret = (uint32_t) (env_global_TOTAL_MEMORY);
  return ret;
}

void env_cfunc_abortStackOverflow(uint32_t alloc_size) {
  char buffer[100];
  // TODO: It would be nice to print out the amount the stack overflowed
  // by, but for this, we need "stackSave" which we would have to link...
  sprintf(buffer,
      "Stack overflow! Attempted to allocate %d bytes on the stack.",
      alloc_size);
  rts_abort(buffer);
}

int static_alloc(int size) {
  int ret = env_global_STATICTOP;
 // Implementation taken from Emscripten JS RTS.
 // Honestly, I haven't got a clue.
 // & -16 means clearing the lowest 4 bits...
  env_global_STATICTOP =
    (env_global_STATICTOP + size + 15) & -16;
  assert(env_global_STATICTOP < env_global_TOTAL_MEMORY);
  return ret;
}

int align_memory(int size) {
  int factor = 16; // 16-bit alignment by default
  return ((int) (ceil(((double) size) / ((double) factor)))) * factor;
}


void env_init() {
  // FIXME: HACK -- this is taken from the PolyBenchC output code at the
  // moment.
  env_global_TOTAL_STACK = 5242880;
  env_global_TOTAL_MEMORY = 134217728;


  // Allocate table and memory
  wasm_rt_allocate_memory(&env_memory_memory,
      env_global_TOTAL_MEMORY / WASM_PAGE_SIZE,
      env_global_TOTAL_MEMORY / WASM_PAGE_SIZE, true);
  wasm_rt_allocate_table(&env_table_table, 1024, -1);


  // Global initialisation
  env_global_ABORT = 0;
  env_global_EXITSTATUS = 0;
  env_global_GLOBAL_BASE = 1024;
  env_global_STATIC_BUMP = 5840;
  env_global_STATIC_BASE = 0;
  env_global_STACK_BASE = 0;
  env_global_DYNAMIC_BASE = 0;
  env_global_DYNAMICTOP_PTR = 0;
  env_global_tableBase = 0;
  global_global_NaN = NAN;
  global_global_Infinity = INFINITY;


  env_global_STATIC_BASE = env_global_GLOBAL_BASE;
  env_global_memoryBase = env_global_STATIC_BASE;

  env_global_STATICTOP = env_global_STATIC_BASE + env_global_STATIC_BUMP;

  // Initialise tempDoublePtr
  env_global_tempDoublePtr = env_global_STATICTOP;
  env_global_STATICTOP += 16;


  // Initialise stack base / top / max
  env_global_DYNAMICTOP_PTR = static_alloc(4);
  uint64_t base_ptr = align_memory(env_global_STATICTOP);
  env_global_STACK_BASE = base_ptr;
  env_global_STACKTOP = base_ptr;
  env_global_STACK_MAX = env_global_STACK_BASE + env_global_TOTAL_STACK;

  // Initialise dynamic base, and save to memory
  env_global_DYNAMIC_BASE = align_memory(env_global_STACK_MAX);
  uint8_t* heap_base = env_memory_memory.data;
  *((uint32_t*) (heap_base + env_global_DYNAMICTOP_PTR)) =
    (uint32_t) env_global_DYNAMIC_BASE;
  assert(env_global_DYNAMIC_BASE < env_global_TOTAL_MEMORY);
}
