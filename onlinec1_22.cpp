#include <bits/stdc++.h>
#include <pthread.h>
#include <semaphore.h>
using namespace std;

sem_t semA, semB, semC, semPrinted;

int N;
const int perLineA = 3, perLineB = 4, perLineC = 5;
const int perLineTotal = perLineA + perLineB + perLineC; // 12

void* threadA(void* arg) {
    while (true) {
        sem_wait(&semA);           // quota শেষ হলে এখানে block হয়ে থাকবে (busy-wait না)
        cout << 'A';
        sem_post(&semPrinted);
    }
    return nullptr;
}

void* threadB(void* arg) {
    while (true) {
        sem_wait(&semB);
        cout << 'B';
        sem_post(&semPrinted);
    }
    return nullptr;
}

void* threadC(void* arg) {
    while (true) {
        sem_wait(&semC);
        cout << 'C';
        sem_post(&semPrinted);
    }
    return nullptr;
}

void* threadD(void* arg) {   // newline printer + quota refill controller
    for (int line = 0; line < N; line++) {
        for (int i = 0; i < perLineTotal; i++) {
            sem_wait(&semPrinted);   // এই লাইনের ১২টা letter print হওয়া পর্যন্ত block
        }
        cout << '\n';

        if (line != N - 1) {        // শেষ লাইন হলে আর refill দরকার নেই
            for (int i = 0; i < perLineA; i++) sem_post(&semA);
            for (int i = 0; i < perLineB; i++) sem_post(&semB);
            for (int i = 0; i < perLineC; i++) sem_post(&semC);
        }
    }
    return nullptr;
}

int main() {
    cout << "Enter number of lines: ";
    cin >> N;

    sem_init(&semA, 0, perLineA);   // প্রথম লাইনের quota দিয়ে শুরু
    sem_init(&semB, 0, perLineB);
    sem_init(&semC, 0, perLineC);
    sem_init(&semPrinted, 0, 0);

    pthread_t tA, tB, tC, tD;
    pthread_create(&tA, nullptr, threadA, nullptr);
    pthread_create(&tB, nullptr, threadB, nullptr);
    pthread_create(&tC, nullptr, threadC, nullptr);
    pthread_create(&tD, nullptr, threadD, nullptr);

    // N লাইন শেষ হওয়ার পর A/B/C চিরকাল block হয়ে থাকবে (আর কোনো refill আসবে না)
    // busy-wait না, শুধু idle-block — তাই এগুলো join না করে detach করে দিলাম
    pthread_detach(tA);
    pthread_detach(tB);
    pthread_detach(tC);

    pthread_join(tD, nullptr);   // thread4 শেষ হলেই বোঝা যাবে সব লাইন হয়ে গেছে

    sem_destroy(&semA);
    sem_destroy(&semB);
    sem_destroy(&semC);
    sem_destroy(&semPrinted);
    return 0;
}