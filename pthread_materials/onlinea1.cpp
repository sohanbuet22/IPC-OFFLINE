#include <iostream>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>

using namespace std;

sem_t sem_p;
sem_t sem_q;
sem_t sem_r;

int n;

void *thread_fun1(void *arg)
{
    for (int i = 1; i <= n; i++)
    {
        sem_wait(&sem_p);

        for (int j = 0; j < i; j++)
        {
            cout << "p";
        }
        sem_post(&sem_q);
    }

    return NULL;
}

void *thread_fun2(void *arg)
{
    for (int i = 1; i <= n; i++)
    {
        sem_wait(&sem_q);

        for (int j = 0; j < i; j++)
        {
            cout << "q";
        }
        sem_post(&sem_r);
    }

    return NULL;
}

void *thread_fun3(void *arg)
{
    for (int i = 1; i <= n; i++)
    {
        sem_wait(&sem_r);

        for (int j = 0; j < i; j++)
        {
            cout << "r";
        }

        cout << endl;

        sem_post(&sem_p);
    }

    return NULL;
}

int main()
{
    pthread_t thread1;
    pthread_t thread2;
    pthread_t thread3;

    cout << "Enter number of iterations: ";
    cin >> n;

    // Initialize semaphore
    // 1 means semaphore is initially available
    sem_init(&sem_p, 0, 1);
    sem_init(&sem_q, 0, 0);
    sem_init(&sem_r, 0, 0);

    // Create three threads
    pthread_create(&thread1, NULL, thread_fun1, NULL);
    pthread_create(&thread2, NULL, thread_fun2, NULL);
    pthread_create(&thread3, NULL, thread_fun3, NULL);

    // Wait until all three threads finish
    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);
    pthread_join(thread3, NULL);

    sem_destroy(&sem_p);
    sem_destroy(&sem_q);
    sem_destroy(&sem_r);

    return 0;
}


/*
#include <iostream>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>

using namespace std;

// --------------------------------------------------
// Global variables
// --------------------------------------------------

sem_t sem;

// Current iteration number
int current_iteration;

// 0 -> Thread 1
// 1 -> Thread 2
// 2 -> Thread 3
int turn = 0;


// --------------------------------------------------
// Thread 1
// Prints p
// --------------------------------------------------

void *thread_fun1(void *arg)
{
    while (true)
    {
        sem_wait(&sem);

        if (turn == 0)
        {
            // Print p current_iteration times
            for (int i = 0; i < current_iteration; i++)
            {
                cout << "p";
            }

            // Now Thread 2's turn
            turn = 1;

            sem_post(&sem);

            break;
        }

        sem_post(&sem);

        // Give other threads a chance
        usleep(1000);
    }

    return NULL;
}


// --------------------------------------------------
// Thread 2
// Prints q
// --------------------------------------------------

void *thread_fun2(void *arg)
{
    while (true)
    {
        sem_wait(&sem);

        if (turn == 1)
        {
            // Print q current_iteration times
            for (int i = 0; i < current_iteration; i++)
            {
                cout << "q";
            }

            // Now Thread 3's turn
            turn = 2;

            sem_post(&sem);

            break;
        }

        sem_post(&sem);

        usleep(1000);
    }

    return NULL;
}


// --------------------------------------------------
// Thread 3
// Prints r
// --------------------------------------------------

void *thread_fun3(void *arg)
{
    while (true)
    {
        sem_wait(&sem);

        if (turn == 2)
        {
            // Print r current_iteration times
            for (int i = 0; i < current_iteration; i++)
            {
                cout << "r";

            }

            // Make output appear properly
            cout << endl;

            // Next iteration starts with Thread 1
            turn = 0;

            sem_post(&sem);

            break;
        }

        sem_post(&sem);

        usleep(1000);
    }

    return NULL;
}


// --------------------------------------------------
// Main function
// --------------------------------------------------

int main()
{
    pthread_t thread1;
    pthread_t thread2;
    pthread_t thread3;

    int total_iterations;

    cout << "Enter number of iterations: ";
    cin >> total_iterations;

    // Initialize semaphore
    // 1 means semaphore is initially available
    sem_init(&sem, 0, 1);


    // ------------------------------------------------
    // Execute iterations one by one
    // ------------------------------------------------

    for (current_iteration = 1;
         current_iteration <= total_iterations;
         current_iteration++)
    {
        // First thread should always be Thread 1
        turn = 0;

        // Create three threads
        pthread_create(&thread1, NULL, thread_fun1, NULL);
        pthread_create(&thread2, NULL, thread_fun2, NULL);
        pthread_create(&thread3, NULL, thread_fun3, NULL);

        // Wait until all three threads finish
        pthread_join(thread1, NULL);
        pthread_join(thread2, NULL);
        pthread_join(thread3, NULL);
    }


    // Destroy semaphore
    sem_destroy(&sem);

    return 0;
}
*/