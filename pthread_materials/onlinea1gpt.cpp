#include <iostream>
#include <pthread.h>
#include <semaphore.h>

using namespace std;

// Three semaphores
sem_t sem_p;
sem_t sem_q;
sem_t sem_r;

// Number of iterations
int n;


// --------------------------------------------------
// Thread 1
// Prints p
// --------------------------------------------------

void *thread_fun1(void *arg)
{
    for (int i = 1; i <= n; i++)
    {
        // Wait for permission to print p
        sem_wait(&sem_p);

        // Print p i times
        for (int j = 0; j < i; j++)
        {
            cout << "p";
        }

        // Give permission to Thread 2
        sem_post(&sem_q);
    }

    return NULL;
}


// --------------------------------------------------
// Thread 2
// Prints q
// --------------------------------------------------

void *thread_fun2(void *arg)
{
    for (int i = 1; i <= n; i++)
    {
        // Wait for permission to print q
        sem_wait(&sem_q);

        // Print q i times
        for (int j = 0; j < i; j++)
        {
            cout << "q";
        }

        // Give permission to Thread 3
        sem_post(&sem_r);
    }

    return NULL;
}


// --------------------------------------------------
// Thread 3
// Prints r
// --------------------------------------------------

void *thread_fun3(void *arg)
{
    for (int i = 1; i <= n; i++)
    {
        // Wait for permission to print r
        sem_wait(&sem_r);

        // Print r i times
        for (int j = 0; j < i; j++)
        {
            cout << "r";
        }

        cout << endl;

        // Give permission to Thread 1
        sem_post(&sem_p);
    }

    return NULL;
}


// --------------------------------------------------
// Main
// --------------------------------------------------

int main()
{
    pthread_t thread1;
    pthread_t thread2;
    pthread_t thread3;

    // Take iteration count as input
    cout << "Enter number of iterations: ";
    cin >> n;

    // Initialize semaphores
    //
    // sem_p = 1  -> Thread 1 can start
    // sem_q = 0  -> Thread 2 must wait
    // sem_r = 0  -> Thread 3 must wait

    sem_init(&sem_p, 0, 1);
    sem_init(&sem_q, 0, 0);
    sem_init(&sem_r, 0, 0);

    // Create threads
    pthread_create(&thread1, NULL, thread_fun1, NULL);
    pthread_create(&thread2, NULL, thread_fun2, NULL);
    pthread_create(&thread3, NULL, thread_fun3, NULL);

    // Wait for all threads to finish
    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);
    pthread_join(thread3, NULL);

    // Destroy semaphores
    sem_destroy(&sem_p);
    sem_destroy(&sem_q);
    sem_destroy(&sem_r);

    return 0;
}