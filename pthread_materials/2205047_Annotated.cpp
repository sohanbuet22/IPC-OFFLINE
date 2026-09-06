#include <iostream>
#include <fstream>
#include <semaphore.h>
#include <pthread.h>
#include <vector>
#include <chrono>
#include <random>
#include <unistd.h>
using namespace std;

// ============================================================
// tunable constants — eigulo change korle simulation-er "feel"
// (koto druto ghotona ghote) change hobe, kintu correctness na
// ============================================================
#define NUM_STATIONS 4        // Task1: mot koyta typewriting station ache (TS1..TS4)
#define UNIT_MS 100            // 1 "time unit" = 100 millisecond real somoy
#define READ_TIME_UNITS 1      // staff ekbar read korte koto time-unit lagabe
#define ARRIVAL_LAMBDA 5.0     // operative-der arrival delay-er Poisson mean
#define STAFF1_LAMBDA 3.0      // staff1 koto ghono ghono read korte ashbe (mean)
#define STAFF2_LAMBDA 5.0      // staff2-er jonno alada mean (assignment-e bola "different interval")

// ============================================================
// input file theke asha problem parameters
// ============================================================
int N, M, X, Y, NUM_GROUPS;
// N = mot operative shonkha, M = protita group-e koyjon
// X = document recreation-er relative time, Y = logbook entry-er relative time
// NUM_GROUPS = N/M -> koyta group/unit ache

// ============================================================
// Task 1 er jonno semaphore: 4 ta typewriting station -> 4 ta semaphore
// protita semaphore ekta MUTEX hisebe kaj korche (init value = 1),
// mane ekbare EKJON-i sheita station use korte parbe
// ============================================================
sem_t station_sem[NUM_STATIONS];

// ekta "Group" (unit) er jonno দরকারি shared state
struct Group
{
    int count;                  // ei group-er koyjon Phase-1 (document recreation) shesh korese
    pthread_mutex_t count_mtx;  // count variable-ke race condition theke bachate mutex
    sem_t leader_sem;           // leader ei semaphore-e wait kore, jokhon shobai (M jon) shesh
                                 // kore fele tokhon eita post kora hoy -> leader jege othe
};

vector<Group> groups;   // protita group-er jonno ekta Group struct, index 0..NUM_GROUPS-1

// ============================================================
// Task 2 (Reader-Writer / Logbook) er shared state
// ============================================================
sem_t logbook_sem;              // logbook-e write korar "lock" (binary semaphore)
pthread_mutex_t read_count_mtx; // read_count variable protect korar jonno mutex
int read_count = 0;             // ekhon koyjon reader (staff) logbook porche
int completed_operations = 0;   // mot koyta unit ekhon porjonto logbook-e entry likhe fele6e

// ============================================================
// mixed/misc shared state
// ============================================================
pthread_mutex_t print_mtx;                          // cout-e ekshathe dui thread lekha thekate mutex
chrono::high_resolution_clock::time_point start_time; // simulation shuru howar somoy (reference point)
volatile bool simulation_done = false;               // shob operative shesh hoye gele true hoy,
                                                      // eita diye staff thread-der loop bondho kora hoy

// ============================================================
// Helper Functions
// ============================================================

// simulation shuru howar por theke koto "time unit" pero geche, seta hishab kore
long get_time_units()
{
    auto now = chrono::high_resolution_clock::now();
    long ms = (long)chrono::duration_cast<chrono::milliseconds>(now - start_time).count();
    return ms / UNIT_MS;   // real millisecond -> amader nijoshsho "time unit"-e convert
}

// dewa "units" shonkhok time-unit somoy ghumiye thake (usleep die)
void sleep_units(int units)
{
    if (units > 0)
        usleep(units * UNIT_MS * 1000);   // units -> milliseconds -> microseconds
}

// THREAD-SAFE print: kono duita thread jate ekshathe cout-e likhe
// output "mixed up" na kore fele, tai print_mtx die protect kora
void log_print(const string &s)
{
    pthread_mutex_lock(&print_mtx);
    cout << s << endl;
    pthread_mutex_unlock(&print_mtx);
}

// Poisson distribution theke ekta random delay (non-negative integer) generate kore
// "Randomized Arrival" + "Poisson Distribution" requirement pূরণ korার jonno eita
int poisson_delay(double lambda) {
    random_device rd;              // OS theke true-random seed nay
    mt19937 gen(rd());             // random number generator, ei seed die initialize
    poisson_distribution<int> dist(lambda);
    int v = dist(gen);
    return v < 0 ? 0 : v;          // safety: kokhono negative delay na hoy
}


// ============================================================
// Shob global state ke initialize kora (program shuru howar age)
// ============================================================
void init_all()
{
    NUM_GROUPS = N / M;   // koyta group/unit ache, seta hishab kora

    // protita typewriting station-er semaphore = 1 (mane free/available)
    for (int i = 0; i < NUM_STATIONS; i++)
    {
        sem_init(&station_sem[i], 0, 1);
    }

    // protita group-er state initialize kora
    groups.resize(NUM_GROUPS);
    for (int g = 0; g < NUM_GROUPS; g++)
    {
        groups[g].count = 0;
        pthread_mutex_init(&groups[g].count_mtx, NULL);
        sem_init(&groups[g].leader_sem, 0, 0);  // 0 die shuru -> leader প্রথমে blocked thakbe
    }

    sem_init(&logbook_sem, 0, 1);              // logbook lock -> shuru te free
    pthread_mutex_init(&read_count_mtx, NULL);
    pthread_mutex_init(&print_mtx, NULL);

    start_time = chrono::high_resolution_clock::now();   // simulation clock-er "t=0" mark kora
}

// ============================================================
// Phase 2: Group leader ei function call kore logbook-e entry likhte
// ============================================================
void write_logbook(int group_num)
{
    sem_wait(&logbook_sem);       // WRITER lock: kono reader/writer na thakle tobei dhoka jabe
    sleep_units(Y);               // logbook entry likhte Y time-unit lage (simulate kora)
    completed_operations++;       // shared counter update (eita WRITER er moddhei nirapod, karon
                                   // ei muhurte r kew (reader/writer) access korte parche na)
    log_print("Unit " + to_string(group_num) + " has completed intelligence distribution at time " + to_string(get_time_units()));
    sem_post(&logbook_sem);       // lock chere dilam, ekhon onno keu (reader/writer) dhukte parbe
}

// ============================================================
// TASK 1: protita OPERATIVE ei thread function-e cholche
// ============================================================
void *operative_thread(void *arg)
{
    int id = *(int *)arg;   // ei operative-er nijoshsho ID (1..N)

    // ---- Step 0: random arrival delay ----
    // shob thread ekshathe create hoy, kintu ekhane elomelo (Poisson) somoy dhore
    // ghumiye thake -> tai bastob-er moto "alada alada somoye pouchay" simulate hoy
    sleep_units(poisson_delay(ARRIVAL_LAMBDA));

    // ---- Step 1: kon typewriting station tar jonno nirdharito ----
    // assignment-er sutro: (ID mod 4) + 1 -> code-e 0-indexed rakha hoyeche
    int station_number = id % NUM_STATIONS;

    // ---- Step 2: sheita station dokhol kora (mutex-er moto ekjon-e-ekjon) ----
    sem_wait(&station_sem[station_number]);   // station bhorti thakle EKHANE BLOCK
                                               // hoye thakbe -- KONO busy-wait/loop nei
    log_print("Operative " + to_string(id) +
              " has arrived at typewriting station TS" + to_string(station_number + 1) +
              "at time " + to_string(get_time_units()));

    // ---- Step 3: document recreation korche (X time-unit lagbe) ----
    sleep_units(X);

    log_print("Operative " + to_string(id) +
              " has completed document recreation at time " +
              to_string(get_time_units()));

    // ---- Step 4: station chere deya -> pore ke wait korche take jagabe ----
    sem_post(&station_sem[station_number]);

    // ---- Step 5: nijer group ber kora, tarpor "ami shesh korlam" bole count barano ----
    int group_num = (id - 1) / M;   // 0-indexed group number
    Group &g = groups[group_num];

    pthread_mutex_lock(&g.count_mtx);
    g.count++;
    bool everyone_done = (g.count == M);   // ei increment-tai ki group-er SHESH jon chilo?
    pthread_mutex_unlock(&g.count_mtx);

    // jodi AMII shesh jon hoi (mane count exactly M-e pouchese amar karone),
    // tahole leader-ke জাগানোর জন্য post kori -- ei "if" ONLY EKবার true hobe
    // (karon count-er increment mutex die protected, tai duijon EKSATHE
    //  "everyone_done == true" dekhte parbe na)
    if (everyone_done)
    {
        log_print("Unit " + to_string(group_num + 1) +
                  " has completed document recreation phase at time " +
                  to_string(get_time_units()));
        sem_post(&g.leader_sem);   // leader-ke jagiye dilam
    }

    // ---- Step 6: ami ki nijei ei group-er leader? ----
    // group-er shobcheye boro ID-i leader (assignment-er niyom onujayi)
    int leader_id = (group_num + 1) * M;
    if (id == leader_id)
    {
        // leader ekhane wait kore -- jodi ami nijei shesh jon hoye thaki tahole
        // upore-i sem_post hoye geche, tai ekhane sathe sathei pass kore jabo.
        // Ar jodi ami shesh na hoi (onno keu pore shesh korbe), tahole ekhane
        // BLOCK hoye thakbo, jotokhon na sheijon amake sem_post kore jagay.
        sem_wait(&g.leader_sem);

        write_logbook(group_num + 1);   // Phase 2 shuru: logbook-e entry
    }

    return NULL;
}

// ============================================================
// TASK 2: duijon INTELLIGENCE STAFF ei thread function-e cholche
// (classic READERS-WRITERS problem, READER-der priority beshi)
// ============================================================
void *staff_thread(void *arg)
{
    int staff_id = *(int *)arg;
    // dujoner jonno alada Poisson mean -> "different random intervals" requirement
    double lambda = (staff_id == 1) ? STAFF1_LAMBDA : STAFF2_LAMBDA;

    while (!simulation_done)   // shob operative kaj shesh na kora porjonto cholte thakbe
    {
        // ---- pore koto khon por abar poRte ashbe, seta elomelo (Poisson) ----
        sleep_units(poisson_delay(lambda));
        if (simulation_done)   // ghum theke uthei abar check -- shesh hoye gele break
            break;

        // ---- READER ENTRY PROTOCOL (classic readers-writers) ----
        pthread_mutex_lock(&read_count_mtx);
        read_count++;
        if (read_count == 1)
            sem_wait(&logbook_sem);   // PROTHOM reader hole, WRITER-ke lock kore dao
                                      // (jate kono writer madhye dhukte na pare)
        pthread_mutex_unlock(&read_count_mtx);

        // ---- ekhon nirapode PORTE parbo (kono writer nei) ----
        log_print("Intelligence Staff " + to_string(staff_id) +
                  " began reviewing logbook at time " + to_string(get_time_units()) +
                  ". Operations completed = " + to_string(completed_operations));
        sleep_units(READ_TIME_UNITS);

        // ---- READER EXIT PROTOCOL ----
        pthread_mutex_lock(&read_count_mtx);
        read_count--;
        if (read_count == 0)
            sem_post(&logbook_sem);   // SHESH reader hole, WRITER-ke abar chere dao
        pthread_mutex_unlock(&read_count_mtx);
    }
    return NULL;
}

// ============================================================
// MAIN: input poRa, shob thread create kora, output likhe close kora
// ============================================================
int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        cout << "./2205047.out <input_file> <output_file>" << endl;
        return 0;
    }

    // ---- Input file theke N, M, X, Y poRa ----
    ifstream fin(argv[1]);
    fin >> N >> M >> X >> Y;
    fin.close();

    // ---- cout-ke output file-e "redirect" kora ----
    // (eivabe pura code-e "cout <<" likhleo ashole output file-e likha hoy)
    ofstream fout(argv[2]);
    streambuf *coutBuf = cout.rdbuf();      // original cout buffer mone rakha (pore ferot dite hobe)
    cout.rdbuf(fout.rdbuf());               // cout-er "target"-ke file-e set kore dewa

    init_all();   // shob semaphore/mutex/clock initialize

    // ---- shob operative thread create kora (1-indexed rakha hoyeche shurbidha-r jonno) ----
    vector<int> ids(N + 1);
    vector<pthread_t> operative_threads(N + 1);
    for (int i = 1; i <= N; i++)
        ids[i] = i;

    for (int i = 1; i <= N; i++)
    {
        // shob N ta thread EKSHATHE (tight loop-e) create kora hocche ->
        // "generate all operatives simultaneously" requirement pূরণ
        pthread_create(&operative_threads[i], NULL, operative_thread, &ids[i]);
    }

    // ---- duijon staff thread create kora ----
    int staff_ids[2] = {1, 2};
    pthread_t staff_threads[2];
    for (int i = 0; i < 2; i++)
    {
        pthread_create(&staff_threads[i], NULL, staff_thread, &staff_ids[i]);
    }

    // ---- shob operative thread shesh howar jonno wait kora ----
    for (int i = 1; i <= N; i++)
    {
        pthread_join(operative_threads[i], NULL);
    }

    // ---- ekhon shob operative-i shesh, tai staff thread-der bolte hobe "ar dorkar nei" ----
    simulation_done = true;
    for (int i = 0; i < 2; i++)
    {
        pthread_join(staff_threads[i], NULL);   // staff thread-o (loop theke ber hoye) shesh hobe
    }

    // ---- cout-ke abar tar original (console) buffer-e ferot deya ----
    cout.rdbuf(coutBuf);
    return 0;
}