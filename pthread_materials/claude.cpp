/*
  CSE 314 - Operating System Sessional, Assignment 3 (IPC)
  "The Shadows of Small Heath" - Peaky Blinders document distribution system

  ------------------------------------------------------------------------
  WHAT THIS PROGRAM SIMULATES
  ------------------------------------------------------------------------
  - N operatives (IDs 1..N) are split into N/M "units" of size M.
    Unit g (1-indexed) contains operatives (g-1)*M+1 .. g*M.
    The operative with the HIGHEST id in a unit is that unit's leader.

  - PHASE 1 (Document Recreation): every operative walks to a typewriting
    station chosen as (id mod 4) -> TS1..TS4, waits if it is occupied,
    types for X time units, then frees the station. Once every member of
    a unit has finished typing, the unit's leader may move to Phase 2.

  - PHASE 2 (Logbook Entry): the leader writes an entry in the single
    shared "master logbook" (taking Y time units). Meanwhile two
    "Intelligence Staff" reader threads repeatedly read the logbook at
    random intervals. This is the classic Reader-Writer problem, solved
    here with READERS GIVEN PRIORITY, as required by the spec.

  ------------------------------------------------------------------------
  SYNCHRONIZATION TOOLS USED (all blocking -> NO busy waiting anywhere)
  ------------------------------------------------------------------------
  1. station_sem[4]      - binary semaphore per station => mutual exclusion
                            for "only one operative typing at a station".
                            sem_wait() blocks a waiting operative instead of
                            spinning; sem_post() wakes the next one in line.
  2. Group.count_mtx +
     Group.leader_sem    - a simple semaphore "barrier": every member
                            increments a protected counter after finishing
                            typing; whoever pushes the counter to M posts
                            leader_sem exactly once, releasing the leader
                            (who may itself be that last member).
  3. logbook_sem +
     read_count_mtx +
     read_count          - the textbook first Readers-Writers solution
                            (reader priority): the first reader locks
                            logbook_sem (keeping writers out) and the last
                            reader to leave unlocks it; writers simply
                            sem_wait/sem_post logbook_sem for exclusive
                            access.
  4. print_mtx            - purely cosmetic: stops two threads' printf/cout
                            lines from interleaving mid-line.

  ------------------------------------------------------------------------
  RANDOMNESS / TIMING MODEL
  ------------------------------------------------------------------------
  - All random delays (arrival jitter, staff read intervals) are drawn
    from a Poisson distribution (poisson_delay), as required.
  - "Relative time units" from the input file are mapped to real time via
    UNIT_MS milliseconds per unit, and get_time_units() reports elapsed
    time back in the same unit so the printed timestamps line up with X
    and Y from the input.

  ------------------------------------------------------------------------
  COMPILATION
    g++ -pthread -std=c++11 2205XXX_ipc_assignment.cpp -o ipc.out
  USAGE
    ./ipc.out <input_file> <output_file>
  INPUT FILE FORMAT
    N M
    x y
  ------------------------------------------------------------------------
  Prepared by: <Your Name> (<Your Student ID>)
*/

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <random>
#include <chrono>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>

using namespace std;

// ---------------------------------------------------------------------
// Tunable constants
// ---------------------------------------------------------------------
#define NUM_STATIONS 4
#define UNIT_MS 100          // 1 "relative time unit" == 100 ms of wall time
#define ARRIVAL_LAMBDA 5.0   // avg units of jitter before an operative starts
#define READ_TIME_UNITS 1    // how long a staff member spends reading
#define STAFF1_LAMBDA 3.0    // avg units between Staff 1's visits
#define STAFF2_LAMBDA 5.0    // avg units between Staff 2's visits (different
                              // from Staff 1's, as the spec asks for)

// ---------------------------------------------------------------------
// Problem parameters read from the input file
// ---------------------------------------------------------------------
int N, M, X, Y, NUM_GROUPS;

// ---------------------------------------------------------------------
// Task 1 state: one binary semaphore per typewriting station
// ---------------------------------------------------------------------
sem_t station_sem[NUM_STATIONS];

// Per-unit bookkeeping: how many members have finished typing, and the
// semaphore that finally releases the leader towards the logbook phase.
struct Group {
    int count;
    pthread_mutex_t count_mtx;
    sem_t leader_sem;
};
vector<Group> groups;

// ---------------------------------------------------------------------
// Task 2 state: reader-writer protected "master logbook"
// ---------------------------------------------------------------------
sem_t logbook_sem;               // writer-exclusive lock
pthread_mutex_t read_count_mtx;  // protects read_count
int read_count = 0;
int completed_operations = 0;    // the shared variable the logbook tracks

// ---------------------------------------------------------------------
// Misc shared state
// ---------------------------------------------------------------------
pthread_mutex_t print_mtx;
chrono::high_resolution_clock::time_point start_time;
volatile bool simulation_done = false;

// =======================================================================
// Helpers
// =======================================================================

// Elapsed time since the simulation started, expressed in "relative
// time units" (matches the scale of X and Y from the input file).
long get_time_units() {
    auto now = chrono::high_resolution_clock::now();
    long ms = (long)chrono::duration_cast<chrono::milliseconds>(now - start_time).count();
    return ms / UNIT_MS;
}

// Thread-safe console output so lines never interleave.
void log_print(const string &s) {
    pthread_mutex_lock(&print_mtx);
    cout << s << endl;
    pthread_mutex_unlock(&print_mtx);
}

// Sleeps for the given number of relative time units.
void sleep_units(int units) {
    if (units > 0) usleep(units * UNIT_MS * 1000);
}

// Generates a single Poisson-distributed random delay (>=0), used for
// every random timing decision in the program (per the assignment's
// "Poisson Distribution" requirement). A fresh generator is created per
// call so this is safe to call concurrently from many threads.
int poisson_delay(double lambda) {
    random_device rd;
    mt19937 gen(rd());
    poisson_distribution<int> dist(lambda);
    int v = dist(gen);
    return v < 0 ? 0 : v;
}

// =======================================================================
// Initialization
// =======================================================================
void init_all() {
    NUM_GROUPS = N / M;

    for (int i = 0; i < NUM_STATIONS; i++)
        sem_init(&station_sem[i], 0, 1);   // 1 => station starts free

    groups.resize(NUM_GROUPS);
    for (int g = 0; g < NUM_GROUPS; g++) {
        groups[g].count = 0;
        pthread_mutex_init(&groups[g].count_mtx, NULL);
        sem_init(&groups[g].leader_sem, 0, 0); // leader starts blocked
    }

    sem_init(&logbook_sem, 0, 1);
    pthread_mutex_init(&read_count_mtx, NULL);
    pthread_mutex_init(&print_mtx, NULL);

    start_time = chrono::high_resolution_clock::now();
}

// =======================================================================
// Task 2: writer side (unit leader making a logbook entry)
// =======================================================================
void write_logbook(int unit_num) {
    sem_wait(&logbook_sem);           // exclusive access: no readers/writers active
    sleep_units(Y);                   // time taken to physically write the entry
    completed_operations++;
    log_print("Unit " + to_string(unit_num) +
              " has completed intelligence distribution at time " +
              to_string(get_time_units()));
    sem_post(&logbook_sem);           // release the logbook
}

// =======================================================================
// Operative thread: Phase 1 (typewriting station) + Phase 2 if leader
// =======================================================================
void* operative_thread(void* arg) {
    int id = *(int*)arg;

    // "Randomized Arrival": every operative is created at the same time
    // in main(), but each independently waits a Poisson-distributed
    // number of units before actually heading to its station.
    sleep_units(poisson_delay(ARRIVAL_LAMBDA));

    int station = id % NUM_STATIONS;      // (id mod 4) -> 0..3 == TS1..TS4
    sem_wait(&station_sem[station]);      // BLOCKS (no busy-wait) if occupied
    log_print("Operative " + to_string(id) +
              " has arrived at typewriting station TS" + to_string(station + 1) +
              " at time " + to_string(get_time_units()));

    sleep_units(X);                       // Document Recreation Phase
    log_print("Operative " + to_string(id) +
              " has completed document recreation at time " +
              to_string(get_time_units()));

    sem_post(&station_sem[station]);      // free the station; wakes the next
                                           // operative waiting on this exact
                                           // station (Task #1's "no indefinite
                                           // waiting" requirement).

    // ---- Report completion to this operative's unit ----
    int group_idx = (id - 1) / M;
    Group &g = groups[group_idx];

    pthread_mutex_lock(&g.count_mtx);
    g.count++;
    bool everyone_done = (g.count == M);
    pthread_mutex_unlock(&g.count_mtx);

    // Exactly one thread (whichever happens to be last) ever sees
    // everyone_done == true, so leader_sem is posted exactly once.
    if (everyone_done) {
        log_print("Unit " + to_string(group_idx + 1) +
                  " has completed document recreation phase at time " +
                  to_string(get_time_units()));
        sem_post(&g.leader_sem);
    }

    // ---- If this operative is the leader, proceed to the logbook ----
    int leader_id = (group_idx + 1) * M;   // highest id in the unit
    if (id == leader_id) {
        sem_wait(&g.leader_sem);           // waits for the last member
                                            // (which might have been itself)
        write_logbook(group_idx + 1);
    }

    return NULL;
}

// =======================================================================
// Task 2: reader side (Intelligence Staff monitoring the logbook)
// =======================================================================
void* staff_thread(void* arg) {
    int staff_id = *(int*)arg;
    double lambda = (staff_id == 1) ? STAFF1_LAMBDA : STAFF2_LAMBDA;

    while (!simulation_done) {
        sleep_units(poisson_delay(lambda));   // random inter-visit interval
        if (simulation_done) break;

        // ---------------- entry section (reader-priority) ----------------
        pthread_mutex_lock(&read_count_mtx);
        read_count++;
        if (read_count == 1)
            sem_wait(&logbook_sem);           // first reader locks out writers
        pthread_mutex_unlock(&read_count_mtx);

        log_print("Intelligence Staff " + to_string(staff_id) +
                  " began reviewing logbook at time " + to_string(get_time_units()) +
                  ". Operations completed = " + to_string(completed_operations));
        sleep_units(READ_TIME_UNITS);          // time spent reading

        // ---------------- exit section -------------------------------
        pthread_mutex_lock(&read_count_mtx);
        read_count--;
        if (read_count == 0)
            sem_post(&logbook_sem);            // last reader lets writers in
        pthread_mutex_unlock(&read_count_mtx);
    }
    return NULL;
}

// =======================================================================
// main
// =======================================================================
int main(int argc, char* argv[]) {
    if (argc != 3) {
        cout << "Usage: ./ipc.out <input_file> <output_file>" << endl;
        return 0;
    }

    ifstream fin(argv[1]);
    fin >> N >> M >> X >> Y;
    fin.close();

    ofstream fout(argv[2]);
    streambuf* coutBuf = cout.rdbuf();
    cout.rdbuf(fout.rdbuf());          // redirect all cout output to the file

    init_all();

    vector<int> ids(N + 1);
    vector<pthread_t> operative_threads(N + 1);
    for (int i = 1; i <= N; i++) ids[i] = i;

    // All operatives are "generated simultaneously": we create every
    // thread back-to-back with no artificial staggering here. The
    // randomness that produces varied arrival times lives INSIDE each
    // thread (the ARRIVAL_LAMBDA delay above), exactly as the spec asks.
    for (int i = 1; i <= N; i++)
        pthread_create(&operative_threads[i], NULL, operative_thread, &ids[i]);

    // Staff readers are started immediately too, so they are "active in
    // the system from initialization" as required.
    int staff_ids[2] = {1, 2};
    pthread_t staff_threads[2];
    for (int i = 0; i < 2; i++)
        pthread_create(&staff_threads[i], NULL, staff_thread, &staff_ids[i]);

    for (int i = 1; i <= N; i++)
        pthread_join(operative_threads[i], NULL);

    // Every unit has logged its operation now; let the staff threads
    // notice simulation_done and exit their loop gracefully.
    simulation_done = true;
    for (int i = 0; i < 2; i++)
        pthread_join(staff_threads[i], NULL);

    cout.rdbuf(coutBuf);
    return 0;
}

/*
  Prepared by: <Your Name> (<Your Student ID>)
*/