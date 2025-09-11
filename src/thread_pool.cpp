#include <cassert>
#include "thread_pool.h"

static void* worker(void* arg){
    TheadPool* tp = (TheadPool*) arg;
    while(true){
        pthread_mutex_lock(&tp->mutex);

        // wait for the condition : a non-empty queue
        while(tp->queue.empty()){
            pthread_cond_wait(&tp->not_empty, &tp->mutex);
        }

        // got the job
        Work w = tp->queue.front();
        tp->queue.pop_front();
        pthread_mutex_unlock(&tp->mutex);

        // do the work
        w.f(w.arg);
    }
    return nullptr;
}

void thread_pool_init(TheadPool* tp, size_t num_threads){
    assert(num_threads > 0);

    int rv = pthread_mutex_init(&tp->mutex, nullptr);
    assert(rv == 0);
    rv = pthread_cond_init(&tp->not_empty, nullptr);
    assert(rv == 0);

    tp->threads.resize(num_threads);
    for(size_t i = 0; i < num_threads; ++i){
        int rv = pthread_create(&tp->threads[i], nullptr, &worker, tp);
        assert(rv == 0);
    }
}

void thread_pool_queue(TheadPool* tp, void(*f)(void*), void* arg){
    pthread_mutex_lock(&tp->mutex);
    tp->queue.push_back(Work {f, arg});
    pthread_cond_signal(&tp->not_empty);
    pthread_mutex_unlock(&tp->mutex);
}