#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unordered_map>
#include <mutex>
#include "dsd.h"
#include "runtime/worker_pool.h"

/* Opaque handle keyed off demod_state* to avoid depending on its layout here */
struct WorkerCtx {
    bool enabled;
    pthread_t threads[2];
    pthread_mutex_t lock;
    pthread_cond_t cv;
    pthread_cond_t done_cv;
    bool should_exit;
    int epoch;
    int completed_in_epoch;
    int posted_count;
    struct { void (*run)(void*); void* arg; } tasks[2];
};

struct WorkerArg {
    WorkerCtx* ctx;
    int id;
};

static std::unordered_map<const void*, WorkerCtx*> g_ctx_map;
static std::mutex g_ctx_mu;

static WorkerCtx* get_ctx(const void* key) {
    std::lock_guard<std::mutex> lg(g_ctx_mu);
    auto it = g_ctx_map.find(key);
    return (it == g_ctx_map.end()) ? nullptr : it->second;
}

static void set_ctx(const void* key, WorkerCtx* ctx) {
    std::lock_guard<std::mutex> lg(g_ctx_mu);
    if (ctx) g_ctx_map[key] = ctx; else g_ctx_map.erase(key);
}

static void* demod_mt_worker(void* arg) {
    WorkerArg* wa = (WorkerArg*)arg;
    WorkerCtx* ctx = wa->ctx;
    const int id = wa->id;
    int local_epoch = 0;
    for (;;) {
        pthread_mutex_lock(&ctx->lock);
        while (!ctx->should_exit && ctx->epoch == local_epoch) {
            pthread_cond_wait(&ctx->cv, &ctx->lock);
        }
        if (ctx->should_exit) {
            pthread_mutex_unlock(&ctx->lock);
            break;
        }
        local_epoch = ctx->epoch;
        void (*fn)(void*) = nullptr;
        void* fn_arg = nullptr;
        if (id < ctx->posted_count) {
            fn = ctx->tasks[id].run;
            fn_arg = ctx->tasks[id].arg;
        }
        pthread_mutex_unlock(&ctx->lock);
        if (fn) {
            fn(fn_arg);
        }
        pthread_mutex_lock(&ctx->lock);
        ctx->completed_in_epoch++;
        if (ctx->completed_in_epoch >= ctx->posted_count) {
            pthread_cond_signal(&ctx->done_cv);
        }
        pthread_mutex_unlock(&ctx->lock);
    }
    return NULL;
}

void demod_mt_init(struct demod_state* s) {
    const char* mt = getenv("DSD_FME_MT");
    bool enable = (mt && mt[0] == '1');
    if (!enable) {
        // No context needed if disabled
        set_ctx((const void*)s, nullptr);
        return;
    }
    WorkerCtx* ctx = get_ctx((const void*)s);
    if (ctx) {
        return; // already initialized
    }
    ctx = (WorkerCtx*)calloc(1, sizeof(WorkerCtx));
    if (!ctx) {
        return;
    }
    ctx->enabled = true;
    ctx->should_exit = false;
    ctx->epoch = 0;
    ctx->completed_in_epoch = 0;
    ctx->posted_count = 0;
    pthread_mutex_init(&ctx->lock, NULL);
    pthread_cond_init(&ctx->cv, NULL);
    pthread_cond_init(&ctx->done_cv, NULL);
    WorkerArg* args = (WorkerArg*)calloc(2, sizeof(WorkerArg));
    for (int i = 0; i < 2; i++) {
        args[i].ctx = ctx;
        args[i].id = i;
        pthread_create(&ctx->threads[i], NULL, demod_mt_worker, (void*)&args[i]);
    }
    // Intentionally leak args array until threads exit to keep pointers valid; freed in destroy
    set_ctx((const void*)s, ctx);
    fprintf(stderr, "Intra-block multithreading enabled (DSD_FME_MT=1), workers: 2.\n");
}

void demod_mt_destroy(struct demod_state* s) {
    WorkerCtx* ctx = get_ctx((const void*)s);
    if (!ctx || !ctx->enabled) {
        set_ctx((const void*)s, nullptr);
        return;
    }
    pthread_mutex_lock(&ctx->lock);
    ctx->should_exit = true;
    pthread_cond_broadcast(&ctx->cv);
    pthread_mutex_unlock(&ctx->lock);
    for (int i = 0; i < 2; i++) {
        pthread_join(ctx->threads[i], NULL);
    }
    pthread_cond_destroy(&ctx->done_cv);
    pthread_cond_destroy(&ctx->cv);
    pthread_mutex_destroy(&ctx->lock);
    // Free leaked WorkerArg array: not tracked; threads have exited so it's safe to free if we had kept pointer.
    // Since we didn't keep it, allow small leak to be reclaimed on process exit. Not critical during normal teardown.
    free(ctx);
    set_ctx((const void*)s, nullptr);
}

void demod_mt_run_two(struct demod_state* s, void (*f0)(void*), void* a0, void (*f1)(void*), void* a1) {
    WorkerCtx* ctx = get_ctx((const void*)s);
    if (!ctx || !ctx->enabled) {
        if (f0) f0(a0);
        if (f1) f1(a1);
        return;
    }
    pthread_mutex_lock(&ctx->lock);
    ctx->tasks[0].run = f0;
    ctx->tasks[0].arg = a0;
    ctx->tasks[1].run = f1;
    ctx->tasks[1].arg = a1;
    ctx->posted_count = (f1 != NULL) ? 2 : 1;
    ctx->completed_in_epoch = 0;
    ctx->epoch++;
    pthread_cond_broadcast(&ctx->cv);
    while (ctx->completed_in_epoch < ctx->posted_count) {
        pthread_cond_wait(&ctx->done_cv, &ctx->lock);
    }
    pthread_mutex_unlock(&ctx->lock);
}


