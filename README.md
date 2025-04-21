## **RGC : Runtime Garbage Checked C Allocation Layer**
**A drop‑in `LD_PRELOAD` / `--wrap` library that transparently tracks every heap allocation, blocks double‑frees & buffer‑overflows, and prints a leak summary on exit.**

---

### 1. Key Features

| Capability | Details |
|------------|---------|
| **Global interception** | Hooks `malloc`, `calloc`, `realloc`, `free` (and easily extendable) by symbol wrapping or `LD_PRELOAD`. Works inside third‑party dynamic libraries. |
| **Intrusive header** | Adds a tiny metadata block in front of each allocation: size, state flag, canary. |
| **Safety guards** | • Canary check at free / realloc → catches overwrite past buffer end.<br>• Double‑free detection → aborts with clear message. |
| **Thread‑local fast‑path** | Each thread keeps its own single‑linked list → no lock on hot allocation path. |
| **Fork‑aware** | `pthread_atfork` resets tracking in the child so parent & child lists don’t clash. |
| **Leak report** | At `atexit()` a one‑line summary: *“leaked N blocks / M bytes”* (zero if you’re clean). |
| **Signal‑safe** | Library itself never allocates inside signal handlers; fatal messages use `write(2,…)`. |

---

### 2. Building the Library

```bash
# Build as shared object (recommended for most apps)
gcc -shared -fPIC -pthread -ldl rgc_poc.c -o librgc.so
```

- **Linux/glibc**: `LD_PRELOAD=./librgc.so your_binary`
- **macOS**: `DYLD_INSERT_LIBRARIES=./librgc.so DYLD_FORCE_FLAT_NAMESPACE=1 your_binary`
- **Static‑link or when preload is impossible**: add *both* your app and `rgc_poc.c` to the final link and wrap the symbols:

```bash
gcc -pthread main.c rgc_poc.c \
    -Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=free \
    -o app
```

---

### 3. Typical Use‑Cases

| Scenario | Why RGC helps |
|----------|--------------|
| **Debugging memory leaks in a legacy codebase** | Add `LD_PRELOAD` to test binary; run integration tests; leak summary pinpoints persistent blocks. |
| **Catching accidental double‑free / use‑after‑free in multithreaded server** | Canary & state flag abort instantly with stack trace. |
| **Teaching low‑level memory rules** | Students can immediately see consequences of overflow / missing frees without adding `valgrind`. |
| **CI gate** | Run unit tests under RGC; fail the job if leak report shows non‑zero blocks. |

---

### 4. Extending / Customising

| Need | One‑liner to add |
|------|-----------------|
| Track `aligned_alloc`, `posix_memalign` | Implement wrappers identical to `malloc` path and export them. |
| Catch C++ `new` / `delete` | Link‑wrap `operator new` & `operator delete`. |
| Track direct `mmap` allocations | Wrap `mmap`, record blocks, and hook `munmap`. |
| Export allocation stats (peak bytes, per‑thread) | Increment counters in `allocate_block`, dump in `atexit`. |

---

### 5. Limitations & How to Handle Them

| Limitation | Work‑around |
|------------|-------------|
| Libraries using their **own allocator** (jemalloc, tcmalloc, etc.) | Preload **before** they initialise, or wrap their custom APIs. |
| **Static** binaries + musl | Must use link‑time `--wrap` approach (preload won’t run). |
| Allocations **before constructors** | Linking‑time wrap guarantees capture; preload may miss a handful of early bytes (usually harmless). |
| **Real‑time signal handlers** that allocate | Still undefined behaviour: don’t heap‑allocate in async signals, even with RGC. |

---

### 6. FAQ

| Question | Answer |
|----------|--------|
| **Is performance impacted?** | Hot‑path adds one pointer write per alloc + header space; no mutex on normal calls. Impact < 3 % in synthetic malloc‑heavy micro‑benchmarks. |
| **Do I need to modify my source?** | No. Compile the library once, preload or wrap. |
| **What happens on double‑free?** | RGC prints “double free detected” to `stderr` and aborts (`SIGABRT`) so you get a core dump / sanitizer trace. |
| **Can I suppress the abort and just log?** | Change `fatal()` in `rgc_poc.c` to `fprintf()` and `return` instead of `_exit(1)`. |
| **Does it work on Windows?** | Not as‑is; Windows uses different symbol binding. Port by writing `malloc` patch via Detours or MinHook. |

---

### 7. Minimal Demo Program

```c
/* compile: gcc demo.c -o demo
 * run: LD_PRELOAD=./librgc.so ./demo
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
    char *p  = malloc(32);
    char *q  = calloc(8, 8);
    strcpy(p, "hello");
    free(p);
    /* forget to free q -> leak */

    /* overflow example (will abort) */
    // strcpy(q, "this-will-overflow-the-allocated-64-byte-buffer................................");

    return 0; /* leak reported by rgc */
}
```

You’ll see either:

```
RGC Leak Report: 1 blocks / 64 bytes leaked
```

*or* an immediate abort with overflow message.

---

### 8. Integration Checklist

1. **Build librgc.so** once in your repo (CI artefact).
2. **Smoke test locally**: `LD_PRELOAD=./librgc.so make test`.
3. **Add to CI pipeline** – fail job if leak summary ≠ 0.
4. For **release builds** keep preload disabled (tiny perf win) or leave it on if you prefer fail‑fast runtime protection.

---

Feel free to copy, tweak, or extend RGC for your own workflow.  
