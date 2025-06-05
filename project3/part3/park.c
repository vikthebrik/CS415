// park.c - Duck Park Simulation (Part 3, Final Implementation)
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <time.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>
#include <stdarg.h>

#define MAX_PASSENGERS 100
#define MAX_CARS 10
#define MAX_QUEUE 100

typedef enum { IDLE, LOADING, RIDING, UNLOADING } CarStatus;

typedef struct {
    int id;
    CarStatus status;
    int onboard[MAX_PASSENGERS];
    int onboard_count;
} Car;

typedef struct {
    int id;
    int has_ticket;
} Passenger;

typedef struct {
    int ids[MAX_QUEUE];
    int count;
} Queue;

typedef struct {
    Queue ticket_queue;
    Queue ride_queue;
    Car cars[MAX_CARS];
    int total_passengers;
    int exploring_count;
    int queueing_count;
    int riding_count;
    int passengers_unboarded;
} SharedState;

// CLI flags with defaults
int n_passengers = 20;
int n_cars = 3;
int car_capacity = 5;
int wait_time = 10;
int ride_duration = 8;
int help = 0;

// Globals
Passenger passengers[MAX_PASSENGERS];
SharedState *shared_state;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t car_ready_cond = PTHREAD_COND_INITIALIZER;
int car_loading_now = 0;
int passengers_finished = 0;

// Utils
char *timestamp() {
    static char buf[32];
    time_t now = time(NULL);
    strftime(buf, sizeof(buf), "%H:%M:%S", localtime(&now));
    return buf;
}

void log_event(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    printf("[Time: %s] ", timestamp());
    vprintf(fmt, args);
    printf("\n");
    fflush(stdout);
    va_end(args);
}

// Queue ops
void enqueue(Queue *q, int id) {
    if (q->count < MAX_QUEUE) q->ids[q->count++] = id;
}

int dequeue(Queue *q) {
    if (q->count == 0) return -1;
    int id = q->ids[0];
    for (int i = 1; i < q->count; ++i) q->ids[i - 1] = q->ids[i];
    q->count--;
    return id;
}

// Monitor (runs every 5 sec)
void *monitor_loop(void *_) {
    while (1) {
        sleep(5);
        pthread_mutex_lock(&mutex);
        if (shared_state->passengers_unboarded >= shared_state->total_passengers) {
            pthread_mutex_unlock(&mutex);
            break;
        }
        printf("\n[Monitor] System State at %s\n", timestamp());
        printf("Ticket Queue: [");
        for (int i = 0; i < shared_state->ticket_queue.count; ++i)
            printf("P%d ", shared_state->ticket_queue.ids[i]);
        printf("]\nRide Queue: [");
        for (int i = 0; i < shared_state->ride_queue.count; ++i)
            printf("P%d ", shared_state->ride_queue.ids[i]);
        printf("]\n");
        for (int i = 0; i < n_cars; ++i) {
            Car *c = &shared_state->cars[i];
            const char *status = (c->status == IDLE) ? "IDLE" :
                                 (c->status == LOADING) ? "LOADING" :
                                 (c->status == RIDING) ? "RIDING" : "UNLOADING";
            printf("Car %d Status: %s (%d/%d passengers)\n", i + 1,
                   status, c->onboard_count, car_capacity);
        }
        printf("Passengers in park: %d (%d exploring, %d in queues, %d on rides)\n",
               shared_state->total_passengers,
               shared_state->exploring_count,
               shared_state->queueing_count,
               shared_state->riding_count);
        pthread_mutex_unlock(&mutex);
    }
    return NULL;
}

// Passenger thread
void *passenger_thread(void *arg) {
    int id = *((int *)arg); free(arg);
    log_event("Passenger %d entered the park", id);

    pthread_mutex_lock(&mutex);
    shared_state->exploring_count++;
    pthread_mutex_unlock(&mutex);

    int explore = (rand() % 10) + 1;
    log_event("Passenger %d is exploring for %d seconds", id, explore);
    sleep(explore);

    pthread_mutex_lock(&mutex);
    shared_state->exploring_count--;
    shared_state->queueing_count++;
    enqueue(&shared_state->ticket_queue, id);
    enqueue(&shared_state->ride_queue, id);
    log_event("Passenger %d is getting a ticket", id);
    if (!car_loading_now && shared_state->ride_queue.count >= 1) {
        car_loading_now = 1;
        pthread_cond_broadcast(&car_ready_cond);
    }
    pthread_mutex_unlock(&mutex);

    return NULL;
}

// Car thread
void *car_thread(void *arg) {
    int cid = *((int *)arg); free(arg);
    Car *car = &shared_state->cars[cid];
    car->id = cid;
    car->status = IDLE;

    while (1) {
        pthread_mutex_lock(&mutex);
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += wait_time;

        while (shared_state->ride_queue.count < car_capacity &&
               shared_state->passengers_unboarded + shared_state->ride_queue.count < n_passengers) {
            if (pthread_cond_timedwait(&car_ready_cond, &mutex, &ts) == ETIMEDOUT) break;
        }

        if (shared_state->ride_queue.count == 0 &&
            shared_state->passengers_unboarded >= n_passengers) {
            pthread_mutex_unlock(&mutex);
            break;
        }

        car->status = LOADING;
        car->onboard_count = 0;
        int loading = (shared_state->ride_queue.count < car_capacity)
                      ? shared_state->ride_queue.count : car_capacity;

        for (int i = 0; i < loading; ++i) {
            int pid = dequeue(&shared_state->ride_queue);
            dequeue(&shared_state->ticket_queue);
            car->onboard[car->onboard_count++] = pid;
            shared_state->queueing_count--;
            shared_state->riding_count++;
        }

        log_event("Car %d is departing with %d passengers", cid + 1, car->onboard_count);
        car->status = RIDING;
        pthread_mutex_unlock(&mutex);

        sleep(ride_duration);

        pthread_mutex_lock(&mutex);
        car->status = UNLOADING;
        shared_state->riding_count -= car->onboard_count;
        shared_state->passengers_unboarded += car->onboard_count;
        car->onboard_count = 0;
        car->status = IDLE;
        car_loading_now = 0;
        pthread_cond_broadcast(&car_ready_cond);
        pthread_mutex_unlock(&mutex);
    }

    return NULL;
}

// Arg parsing
void print_help() {
    printf("Usage: ./park [-n passengers] [-c cars] [-p capacity] [-w wait] [-r ride] [-h]\n");
}

void parse_args(int argc, char *argv[]) {
    int opt;
    while ((opt = getopt(argc, argv, "n:c:p:w:r:h")) != -1) {
        switch (opt) {
            case 'n': n_passengers = atoi(optarg); break;
            case 'c': n_cars = atoi(optarg); break;
            case 'p': car_capacity = atoi(optarg); break;
            case 'w': wait_time = atoi(optarg); break;
            case 'r': ride_duration = atoi(optarg); break;
            case 'h': help = 1; break;
        }
    }
}

int main(int argc, char *argv[]) {
    srand(time(NULL));
    parse_args(argc, argv);
    if (help) { print_help(); return 0; }

    printf("===== DUCK PARK SIMULATION (Part 3) =====\n");
    printf("Simulation started with parameters:\n"
           "- Number of passengers: %d\n"
           "- Number of cars: %d\n"
           "- Car capacity: %d\n"
           "- Car wait time: %d seconds\n"
           "- Ride duration: %d seconds\n",
           n_passengers, n_cars, car_capacity, wait_time, ride_duration);

    int fd = shm_open("/duckpark", O_CREAT | O_RDWR, 0666);
    ftruncate(fd, sizeof(SharedState));
    shared_state = mmap(NULL, sizeof(SharedState),
                        PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    memset(shared_state, 0, sizeof(SharedState));
    shared_state->total_passengers = n_passengers;

    pthread_t ptids[MAX_PASSENGERS], ctids[MAX_CARS], monitor;
    pthread_create(&monitor, NULL, monitor_loop, NULL);

    for (int i = 0; i < n_cars; ++i) {
        int *cid = malloc(sizeof(int));
        *cid = i;
        pthread_create(&ctids[i], NULL, car_thread, cid);
    }

    for (int i = 0; i < n_passengers; ++i) {
        int *pid = malloc(sizeof(int));
        *pid = i + 1;
        pthread_create(&ptids[i], NULL, passenger_thread, pid);
    }

    for (int i = 0; i < n_passengers; ++i) pthread_join(ptids[i], NULL);
    for (int i = 0; i < n_cars; ++i) pthread_join(ctids[i], NULL);
    pthread_join(monitor, NULL);

    printf("\nSimulation complete. All passengers served and unboarded.\n");
    shm_unlink("/duckpark");
    return 0;
}
