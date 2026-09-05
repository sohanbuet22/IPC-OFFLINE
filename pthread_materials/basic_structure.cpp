#include <bits/stdc++.h>
#include <pthread.h>
#include <semaphore.h>
#include <sched.h>
using namespace std;

sem_t mutexSem;
int N;

void *threadA(void *arg)
{
    while (true)
    {
        sem_wait(&mutexSem);

        sem_post(&mutexSem);
        sched_yield();
    }
    return nullptr;
}

void *threadB(void *arg)
{
    while (true)
    {
        sem_wait(&mutexSem);

        sem_post(&mutexSem);
        sched_yield();
    }
    return nullptr;
}

void *threadC(void *arg)
{
    while (true)
    {
        sem_wait(&mutexSem);

        sem_post(&mutexSem);
        sched_yield();
    }
    return nullptr;
}

int main()
{
    cout << "Enter N: ";
    cin >> N;

    sem_init(&mutexSem, 0, 1);

    pthread_t t1, t2, t3;

    pthread_create(&t1, nullptr, threadA, nullptr);
    pthread_create(&t2, nullptr, threadB, nullptr);
    pthread_create(&t3, nullptr, threadC, nullptr);

    pthread_join(t1, nullptr);
    pthread_join(t2, nullptr);
    pthread_join(t3, nullptr);

    sem_destroy(&mutexSem);
    return 0;
}