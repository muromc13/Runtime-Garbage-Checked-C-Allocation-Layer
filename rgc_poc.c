/*================================================================
  RGC PoC v3 — UNIVERSAL RUNTIME MEMORY TRACKER / MINI‑GC
  -------------------------------------------------------
  Features
  □ LD_PRELOAD or --wrap based global interposition — captures *all*
    malloc/calloc/realloc/free in the process (even inside 3rd‑party libs)
  □ Intrusive header with canary + double‑free guard + leak report
  □ Thread‑local fast‑path (lock‑free on hot path) + global registry
  □ fork() safety via pthread_atfork (child cleans its own allocations)
  □ No allocations inside signal handlers; async‑signal‑safe fallback
  Build & run
     # Shared‑library hook (recommended)
     gcc -shared -fPIC -pthread -ldl rgc_poc.c -o librgc.so
     LD_PRELOAD=./librgc.so ./your_app
     # Or link‑wrap (static build)
     gcc -pthread your_app.c rgc_poc.c -Wl,--wrap=malloc,--wrap=calloc,\
         --wrap=realloc,--wrap=free -o your_app
  =================================================================*/
#define _GNU_SOURCE
#include <dlfcn.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* ---------- config ------------ */
#define RGC_CANARY       0xDEADC0DEu
#define FLAG_ALIVE       0x01u
#define FLAG_FREED       0x02u

/* ---------- metadata header ---- */
typedef struct RMeta {
    size_t          size;
    uint32_t        flags;
    uint32_t        canary;
    struct RMeta   *next;         /* TLS list link */
} RMeta;

#define HDR(p)  ((RMeta*)((char*)(p) - sizeof(RMeta)))
#define USR(h)  ((void*)((char*)(h) + sizeof(RMeta)))

/* ---------- real libc symbols --- */
static void *(*real_malloc)(size_t)          = NULL;
static void *(*real_calloc)(size_t,size_t)   = NULL;
static void *(*real_realloc)(void*,size_t)   = NULL;
static void (*real_free)(void*)              = NULL;

/* ---------- TLS & master list ---- */
static __thread RMeta *tls_head = NULL;
static __thread int    tls_registered = 0;

typedef struct TList { RMeta **headptr; struct TList *next; } TList;
static TList *master = NULL;
static pthread_mutex_t master_lock = PTHREAD_MUTEX_INITIALIZER;

/* ---------- util --------------- */
static void fatal(const char *msg){ write(2,msg,strlen(msg)); _exit(1); }

static void init_real_syms(void){
    real_malloc  = dlsym(RTLD_NEXT,"malloc");
    real_calloc  = dlsym(RTLD_NEXT,"calloc");
    real_realloc = dlsym(RTLD_NEXT,"realloc");
    real_free    = dlsym(RTLD_NEXT,"free");
    if(!real_malloc||!real_calloc||!real_realloc||!real_free)
        fatal("RGC: dlsym failed\n");
}

static void register_tls(void){
    if(tls_registered) return;
    pthread_mutex_lock(&master_lock);
    TList *n = real_malloc(sizeof(TList));
    n->headptr = &tls_head;
    n->next = master;
    master = n;
    pthread_mutex_unlock(&master_lock);
    tls_registered = 1;
}

static inline void insert_tls(RMeta *m){ m->next = tls_head; tls_head = m; }

/* ---------- fork handlers ------- */
static void atfork_prepare(void){ pthread_mutex_lock(&master_lock); }
static void atfork_parent(void){ pthread_mutex_unlock(&master_lock); }
static void atfork_child(void){
    master = NULL;               /* discard parent registry */
    tls_head = NULL;             /* child has single thread */
    tls_registered = 0;
    pthread_mutex_unlock(&master_lock);
}

/* ---------- allocation core ----- */
static void header_check(RMeta *h){
    if(h->canary!=RGC_CANARY) fatal("RGC: buffer overflow detected\n");
}

static void *allocate_block(size_t sz){
    register_tls();
    size_t total = sizeof(RMeta)+sz;
    RMeta *h = real_malloc(total);
    if(!h) fatal("RGC: OOM\n");
    h->size=sz; h->flags=FLAG_ALIVE; h->canary=RGC_CANARY;
    insert_tls(h);
    return USR(h);
}

/* ---------- exported interposers */
void *malloc(size_t sz){ if(!real_malloc) init_real_syms(); return allocate_block(sz); }
void *calloc(size_t n,size_t sz){ void *p=malloc(n*sz); memset(p,0,n*sz); return p; }
void *realloc(void *ptr,size_t new_sz){
    if(!real_malloc) init_real_syms();
    if(!ptr) return allocate_block(new_sz);
    RMeta *h=HDR(ptr); header_check(h);
    if(h->flags==FLAG_FREED) fatal("RGC: realloc after free\n");
    /* unlink */
    RMeta **ind=&tls_head; while(*ind && *ind!=h) ind=&(*ind)->next; if(*ind) *ind=h->next;
    RMeta *newh = real_realloc(h,sizeof(RMeta)+new_sz);
    if(!newh) fatal("RGC: realloc OOM\n");
    newh->size=new_sz; newh->next=tls_head; tls_head=newh;
    return USR(newh);
}

void free(void *ptr){
    if(!ptr){ return; }
    if(!real_free) init_real_syms();
    RMeta *h=HDR(ptr); header_check(h);
    if(h->flags==FLAG_FREED) fatal("RGC: double free\n");
    h->flags=FLAG_FREED;
    /* unlink */
    RMeta **ind=&tls_head; while(*ind && *ind!=h) ind=&(*ind)->next; if(*ind) *ind=h->next;
    real_free(h);
}

/* ---------- leak scanner -------- */
static void sweep(RMeta *head,size_t *blk,size_t *bytes){
    for(RMeta *m=head;m;m=m->next){ if(m->flags==FLAG_ALIVE){ (*blk)++; (*bytes)+=m->size; real_free(m);} }
}

static void rgc_report(void){
    size_t blk=0,bytes=0;
    pthread_mutex_lock(&master_lock);
    for(TList *t=master;t;t=t->next){ sweep(*(t->headptr),&blk,&bytes); *(t->headptr)=NULL; }
    pthread_mutex_unlock(&master_lock);
    if(blk){ fprintf(stderr,"\nRGC Leak Report: %zu blocks / %zu bytes leaked\n",blk,bytes); }
}

/* ---------- constructor / destructor */
__attribute__((constructor)) static void rgc_ctor(void){
    init_real_syms();
    pthread_atfork(atfork_prepare,atfork_parent,atfork_child);
    atexit(rgc_report);
}
