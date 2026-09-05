#include <bits/stdc++.h>
#include <pthread.h>
#include <semaphore.h>
#include <sched.h>
using namespace std;

// ---- Shared state (protected by a single semaphore, as hinted) ----
sem_t mutexSem;          // binary semaphore acting as a lock
int N;                   // number of lines to print
int linesPrinted = 0;    // how many complete lines so far
bool sawA = false, sawB = false, sawC = false; // seen in the current (unfinished) line

void* threadA(void* arg) {
    while (true) {
        sem_wait(&mutexSem);              // ---- enter critical section ----

        if (linesPrinted >= N) {          // all N lines done, thread can exit
            sem_post(&mutexSem);
            break;
        }

        cout << 'A';
        cout.flush();
        sawA = true;

        // A line is complete once at least one A, one B and one C were printed
        if (sawA && sawB && sawC) {
            cout << '\n';
            sawA = sawB = sawC = false;   // reset for the next line
            linesPrinted++;
        }

        sem_post(&mutexSem);              // ---- leave critical section ----
        sched_yield();                    // give other threads a chance to run
    }
    return nullptr;
}

void* threadB(void* arg) {
    while (true) {
        sem_wait(&mutexSem);

        if (linesPrinted >= N) {
            sem_post(&mutexSem);
            break;
        }

        cout << 'B';
        cout.flush();
        sawB = true;

        if (sawA && sawB && sawC) {
            cout << '\n';
            sawA = sawB = sawC = false;
            linesPrinted++;
        }

        sem_post(&mutexSem);
        sched_yield();
    }
    return nullptr;
}

void* threadC(void* arg) {
    while (true) {
        sem_wait(&mutexSem);

        if (linesPrinted >= N) {
            sem_post(&mutexSem);
            break;
        }

        cout << 'C';
        cout.flush();
        sawC = true;

        if (sawA && sawB && sawC) {
            cout << '\n';
            sawA = sawB = sawC = false;
            linesPrinted++;
        }

        sem_post(&mutexSem);
        sched_yield();
    }
    return nullptr;
}

int main() {
    cout << "Enter N: ";
    cin >> N;

    sem_init(&mutexSem, 0, 1);            // start unlocked (value = 1)

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