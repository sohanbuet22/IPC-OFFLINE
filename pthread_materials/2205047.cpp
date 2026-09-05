#include <iostream>
#include <fstream>
#include <semaphore.h>
#include <pthread.h>
#include <vector>
#include <chrono>
#include <random>
#include <unistd.h>
using namespace std;

// tunable constants
#define NUM_STATIONS 4
#define UNIT_MS 100
#define READ_TIME_UNITS 1
#define ARRIVAL_LAMBDA 5.0
#define STAFF1_LAMBDA 3.0
#define STAFF2_LAMBDA 5.0

// problem parameters read from the input file
int N, M, X, Y, NUM_GROUPS;

// task1 semaphores
sem_t station_sem[NUM_STATIONS];

struct Group
{
    int count;
    pthread_mutex_t count_mtx;
    sem_t leader_sem;
};

vector<Group> groups;

// task2 state

sem_t logbook_sem;
pthread_mutex_t read_count_mtx;
int read_count = 0;
int completed_operations = 0;

// misc state
pthread_mutex_t print_mtx;
chrono::high_resolution_clock::time_point start_time;
volatile bool simulation_done = false;

// helper function
long get_time_units()
{
    auto now = chrono::high_resolution_clock::now();
    long ms = (long)chrono::duration_cast<chrono::milliseconds>(now - start_time).count();
    return ms / UNIT_MS;
}

void sleep_units(int units)
{
    if (units > 0)
        usleep(units * UNIT_MS * 1000);
}

void log_print(const string &s)
{
    pthread_mutex_lock(&print_mtx);
    cout << s << endl;
    pthread_mutex_unlock(&print_mtx);
}

int poisson_delay(double lambda) {
    random_device rd;
    mt19937 gen(rd());
    poisson_distribution<int> dist(lambda);
    int v = dist(gen);
    return v < 0 ? 0 : v;
}


void init_all()
{
    NUM_GROUPS = N / M;

    for (int i = 0; i < NUM_STATIONS; i++)
    {
        sem_init(&station_sem[i], 0, 1);
    }

    groups.resize(NUM_GROUPS);
    for (int g = 0; g < NUM_GROUPS; g++)
    {
        groups[g].count = 0;
        pthread_mutex_init(&groups[g].count_mtx, NULL);
        sem_init(&groups[g].leader_sem, 0, 0);
    }
    sem_init(&logbook_sem, 0, 1);
    pthread_mutex_init(&read_count_mtx, NULL);
    pthread_mutex_init(&print_mtx, NULL);

    start_time = chrono::high_resolution_clock::now();
}

void write_logbook(int group_num)
{
    sem_wait(&logbook_sem);
    sleep_units(Y);
    completed_operations++;
    log_print("Unit " + to_string(group_num) + " has completed intelligence distribution at time " + to_string(get_time_units()));
    sem_post(&logbook_sem);
}

void *operative_thread(void *arg)
{
    int id = *(int *)arg;

    sleep_units(poisson_delay(ARRIVAL_LAMBDA));

    int station_number = id % NUM_STATIONS;

    sem_wait(&station_sem[station_number]);
    log_print("Operative " + to_string(id) +
              " has arrived at typewriting station TS" + to_string(station_number + 1) +
              "at time " + to_string(get_time_units()));

    sleep_units(X);

    log_print("Operative " + to_string(id) +
              " has completed document recreation at time " +
              to_string(get_time_units()));

    sem_post(&station_sem[station_number]);

    int group_num = (id - 1) / M;
    Group &g = groups[group_num];

    pthread_mutex_lock(&g.count_mtx);
    g.count++;
    bool everyone_done = (g.count == M);
    pthread_mutex_unlock(&g.count_mtx);

    if (everyone_done)
    {
        log_print("Unit " + to_string(group_num + 1) +
                  " has completed document recreation phase at time " +
                  to_string(get_time_units()));
        sem_post(&g.leader_sem);
    }

    int leader_id = (group_num + 1) * M;
    if (id == leader_id)
    {
        sem_wait(&g.leader_sem);

        write_logbook(group_num + 1);
    }

    return NULL;
}

// task 2
void *staff_thread(void *arg)
{
    int staff_id = *(int *)arg;
    double lambda = (staff_id == 1) ? STAFF1_LAMBDA : STAFF2_LAMBDA;

    while (!simulation_done)
    {
        sleep_units(poisson_delay(lambda));
        if (simulation_done)
            break;

        pthread_mutex_lock(&read_count_mtx);
        read_count++;
        if (read_count == 1)
            sem_wait(&logbook_sem);
        pthread_mutex_unlock(&read_count_mtx);

        log_print("Intelligence Staff " + to_string(staff_id) +
                  " began reviewing logbook at time " + to_string(get_time_units()) +
                  ". Operations completed = " + to_string(completed_operations));
        sleep_units(READ_TIME_UNITS);

        pthread_mutex_lock(&read_count_mtx);
        read_count--;
        if (read_count == 0)
            sem_post(&logbook_sem);
        pthread_mutex_unlock(&read_count_mtx);
    }
    return NULL;
}

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        cout << "./2205047.out <input_file> <output_file>" << endl;
        return 0;
    }

    ifstream fin(argv[1]);
    fin >> N >> M >> X >> Y;
    fin.close();

    ofstream fout(argv[2]);
    streambuf *coutBuf = cout.rdbuf();
    cout.rdbuf(fout.rdbuf());

    init_all();

    vector<int> ids(N + 1);
    vector<pthread_t> operative_threads(N + 1);
    for (int i = 1; i <= N; i++)
        ids[i] = i;

    for (int i = 1; i <= N; i++)
    {
        pthread_create(&operative_threads[i], NULL, operative_thread, &ids[i]);
    }

    int staff_ids[2] = {1, 2};
    pthread_t staff_threads[2];
    for (int i = 0; i < 2; i++)
    {
        pthread_create(&staff_threads[i], NULL, staff_thread, &staff_ids[i]);
    }

    for (int i = 1; i <= N; i++)
    {
        pthread_join(operative_threads[i], NULL);
    }

    simulation_done = true;
    for (int i = 0; i < 2; i++)
    {
        pthread_join(staff_threads[i], NULL);
    }

    cout.rdbuf(coutBuf);
    return 0;
}