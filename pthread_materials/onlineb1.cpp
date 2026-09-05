/*

Enter N: 10
_________+
________++
_______+++
______++++
_____+++++

*/

#include <bits/stdc++.h>
#include <pthread.h>
#include <semaphore.h>
using namespace std;

sem_t semA, semB;   // দুইটা semaphore — একটা প্রতিটা thread-এর জন্য
int N;
int turnsLeft;      // মোট কয়টা লাইন বাকি আছে

void* threadA(void* arg) {
    int underscoreCount = N - 1;   // প্রথম লাইনে N-1 underscore
    for (int i = 0; i < turnsLeft; i++) {
        sem_wait(&semA);              // A এখানে ব্লক থাকে, যতক্ষণ না B post করে
        for (int j = 0; j < underscoreCount; j++) cout << '_';
        underscoreCount--;
        sem_post(&semB);              // B-কে জাগায়
    }
    return nullptr;
}

void* threadB(void* arg) {
    int plusCount = 1;             // প্রথম লাইনে 1 plus
    for (int i = 0; i < turnsLeft; i++) {
        sem_wait(&semB);              // B এখানে ব্লক থাকে, যতক্ষণ না A post করে
        for (int j = 0; j < plusCount; j++) cout << '+';
        cout << '\n';
        plusCount++;
        sem_post(&semA);              // A-কে আবার জাগায় (পরের লাইনের জন্য)
    }
    return nullptr;
}

int main() {
    cout << "Enter N: ";
    cin >> N;
    turnsLeft = N / 2;

    sem_init(&semA, 0, 1);   // A প্রথমে যাবে, তাই value = 1
    sem_init(&semB, 0, 0);   // B প্রথমে ব্লক থাকবে, তাই value = 0

    pthread_t t1, t2;
    pthread_create(&t1, nullptr, threadA, nullptr);
    pthread_create(&t2, nullptr, threadB, nullptr);

    pthread_join(t1, nullptr);
    pthread_join(t2, nullptr);

    sem_destroy(&semA);
    sem_destroy(&semB);
    return 0;
}