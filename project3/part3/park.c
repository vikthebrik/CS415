// park.c - Final Duck Park Simulation (Part 3) with all fixes
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>
#include <stdarg.h>
#include <time.h>

#define MAX_PASSENGERS 100
#define MAX_CARS 10
#define MAX_QUEUE 100

typedef enum { IDLE, LOADING, RIDING, UNLOADING } CarStatus;

typedef struct {
    int id;
    CarStatus status;
    int onboard[MAX_PASSENGERS];
    int onboard_count;
    int rides_completed;
    double utilization_total;
    int total_boarded;
} Car;

typedef struct {
    int id;
    int has_ridden;
    double ticket_wait_time;
    double ride_wait_time;
    int total_rides;
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
    int total_rides;
    int served_once;
    int total_boarded_passengers;
} SharedState;

// Globals
int n_passengers = 20;
int n_cars = 3;
int car_capacity = 5;
int wait_time = 10;
int ride_duration = 8;
int explore_min = 2;
int explore_max = 5;

Passenger passengers[MAX_PASSENGERS];
SharedState *shared_state;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t car_ready_cond = PTHREAD_COND_INITIALIZER;

double program_start;

// Time utility
double now_seconds() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec / 1e6 - program_start;
}

void log_event(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    printf("[Time: %.2f] ", now_seconds());
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
    for (int i = 1; i < q->count; ++i)
        q->ids[i - 1] = q->ids[i];
    q->count--;
    return id;
}

// Monitor thread
void *monitor_thread(void *arg) {
    while (1) {
        sleep(5);
        pthread_mutex_lock(&mutex);
        printf("\n[Monitor] System State at %.2f seconds\n", now_seconds());
        printf("Ticket Queue: [");
        for (int i = 0; i < shared_state->ticket_queue.count; ++i)
            printf("P%d ", shared_state->ticket_queue.ids[i] + 1);
        printf("]\n");

        printf("Ride Queue: [");
        for (int i = 0; i < shared_state->ride_queue.count; ++i)
            printf("P%d ", shared_state->ride_queue.ids[i] + 1);
        printf("]\n");

        for (int i = 0; i < n_cars; ++i) {
            Car *c = &shared_state->cars[i];
            const char *status =
                (c->status == IDLE) ? "IDLE" :
                (c->status == LOADING) ? "LOADING" :
                (c->status == RIDING) ? "RIDING" : "UNLOADING";
            printf("Car %d: %s (%d/%d)\n", i + 1, status, c->onboard_count, car_capacity);
        }

        printf("Passengers: %d exploring | %d in queues | %d riding\n",
               shared_state->exploring_count,
               shared_state->queueing_count,
               shared_state->riding_count);

        if (shared_state->served_once >= n_passengers) {
            pthread_mutex_unlock(&mutex);
            break;
        }

        pthread_mutex_unlock(&mutex);
    }
    return NULL;
}

// Passenger thread
void *passenger_thread(void *arg) {
    int id = *(int *)arg;
    free(arg);
    passengers[id].id = id;

    while (1) {
        int explore_time = (rand() % (explore_max - explore_min + 1)) + explore_min;
        pthread_mutex_lock(&mutex);
        shared_state->exploring_count++;
        pthread_mutex_unlock(&mutex);

        log_event("Passenger %d exploring for %d sec", id + 1, explore_time);
        sleep(explore_time);

        double ticket_start = now_seconds();

        pthread_mutex_lock(&mutex);
        shared_state->exploring_count--;
        enqueue(&shared_state->ticket_queue, id);
        enqueue(&shared_state->ride_queue, id);
        shared_state->queueing_count++;
        pthread_cond_signal(&car_ready_cond);
        pthread_mutex_unlock(&mutex);

        // Wait until onboard
        int onboarded = 0;
        while (!onboarded) {
            pthread_mutex_lock(&mutex);
            for (int i = 0; i < n_cars; ++i) {
                for (int j = 0; j < shared_state->cars[i].onboard_count; ++j) {
                    if (shared_state->cars[i].onboard[j] == id) {
                        onboarded = 1;
                        break;
                    }
                }
                if (onboarded) break;
            }
            pthread_mutex_unlock(&mutex);
            usleep(10000);
        }

        double ride_start = now_seconds();
        passengers[id].ticket_wait_time += (ride_start - ticket_start);

        // Wait until unboarded
        int riding = 1;
        while (riding) {
            pthread_mutex_lock(&mutex);
            riding = 0;
            for (int i = 0; i < n_cars; ++i) {
                for (int j = 0; j < shared_state->cars[i].onboard_count; ++j) {
                    if (shared_state->cars[i].onboard[j] == id) {
                        riding = 1;
                        break;
                    }
                }
                if (riding) break;
            }
            pthread_mutex_unlock(&mutex);
            usleep(10000);
        }

        double ride_end = now_seconds();
        passengers[id].ride_wait_time += (ride_end - ride_start);
        passengers[id].total_rides++;

        pthread_mutex_lock(&mutex);
        shared_state->riding_count--;
        shared_state->total_boarded_passengers++;
        if (!passengers[id].has_ridden) {
            passengers[id].has_ridden = 1;
            shared_state->served_once++;
            log_event("Passenger %d completed first ride", id + 1);
        }
        pthread_mutex_unlock(&mutex);

        if (shared_state->served_once >= n_passengers)
            break;
    }
    return NULL;
}

// Car thread
void *car_thread(void *arg) {
    int id = *(int *)arg;
    free(arg);
    Car *car = &shared_state->cars[id];
    car->id = id;

    while (1) {
        pthread_mutex_lock(&mutex);
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += wait_time;

        car->status = IDLE;
        while (shared_state->ride_queue.count < car_capacity && shared_state->ride_queue.count == 0) {
            if (pthread_cond_timedwait(&car_ready_cond, &mutex, &ts) == ETIMEDOUT) break;
        }

        if (shared_state->ride_queue.count == 0 &&
            shared_state->served_once >= n_passengers) {
            pthread_mutex_unlock(&mutex);
            break;
        }

        car->status = LOADING;
        car->onboard_count = 0;
        for (int i = 0; i < car_capacity && shared_state->ride_queue.count > 0; ++i) {
            int pid = dequeue(&shared_state->ride_queue);
            dequeue(&shared_state->ticket_queue);
            car->onboard[car->onboard_count++] = pid;
            shared_state->queueing_count--;
            shared_state->riding_count++;
            car->total_boarded++;
        }

        if (car->onboard_count > 0) {
            car->status = RIDING;
            log_event("Car %d departing with %d passengers", id + 1, car->onboard_count);
            pthread_mutex_unlock(&mutex);

            sleep(ride_duration);

            pthread_mutex_lock(&mutex);
            car->status = UNLOADING;
            car->rides_completed++;
            car->utilization_total += car->onboard_count;
            car->onboard_count = 0;
            shared_state->total_rides++;
        }

        pthread_mutex_unlock(&mutex);
    }
    return NULL;
}

// Arg parsing
void parse_args(int argc, char *argv[]) {
    int opt;
    while ((opt = getopt(argc, argv, "n:c:p:w:r:x:y:")) != -1) {
        switch (opt) {
            case 'n': n_passengers = atoi(optarg); break;
            case 'c': n_cars = atoi(optarg); break;
            case 'p': car_capacity = atoi(optarg); break;
            case 'w': wait_time = atoi(optarg); break;
            case 'r': ride_duration = atoi(optarg); break;
            case 'x': explore_min = atoi(optarg); break;
            case 'y': explore_max = atoi(optarg); break;
        }
    }
}

int main(int argc, char *argv[]) {
    srand(time(NULL));
    parse_args(argc, argv);

    printf("===== DUCK PARK SIMULATION =====\n[Monitor] Simulation started with parameters:\n");
    printf("- Number of passenger threads: %d\n", n_passengers);
    printf("- Number of cars: %d\n", n_cars);
    printf("- Capacity per car: %d\n", car_capacity);
    printf("- Park exploration time: %d-%d seconds\n", explore_min, explore_max);
    printf("- Car waiting period: %d seconds\n", wait_time);
    printf("- Ride duration: %d seconds\n", ride_duration);

    int fd = shm_open("/duckpark", O_CREAT | O_RDWR, 0666);
    ftruncate(fd, sizeof(SharedState));
    shared_state = mmap(NULL, sizeof(SharedState), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    memset(shared_state, 0, sizeof(SharedState));
    shared_state->total_passengers = n_passengers;

    program_start = now_seconds() + program_start;

    pthread_t car_threads[MAX_CARS], passenger_threads[MAX_PASSENGERS], monitor;
    pthread_create(&monitor, NULL, monitor_thread, NULL);

    for (int i = 0; i < n_cars; ++i) {
        int *cid = malloc(sizeof(int)); *cid = i;
        pthread_create(&car_threads[i], NULL, car_thread, cid);
    }

    for (int i = 0; i < n_passengers; ++i) {
        int *pid = malloc(sizeof(int)); *pid = i;
        pthread_create(&passenger_threads[i], NULL, passenger_thread, pid);
        sleep(2); // staggered entry
    }

    for (int i = 0; i < n_passengers; ++i) pthread_join(passenger_threads[i], NULL);
    for (int i = 0; i < n_cars; ++i) pthread_join(car_threads[i], NULL);
    pthread_join(monitor, NULL);

    double total_time = now_seconds();
    double total_ticket = 0, total_ride = 0;
    for (int i = 0; i < n_passengers; ++i) {
        total_ticket += passengers[i].ticket_wait_time;
        total_ride += passengers[i].ride_wait_time;
    }

    double avg_ticket = total_ticket / n_passengers;
    double avg_ride = total_ride / n_passengers;

    double total_util = 0;
    for (int i = 0; i < n_cars; ++i) {
        if (shared_state->cars[i].rides_completed > 0)
            total_util += shared_state->cars[i].utilization_total /
                          shared_state->cars[i].rides_completed;
    }

    double avg_util = total_util / n_cars;
    double util_percent = (avg_util / car_capacity) * 100;

    printf("\n[Monitor] FINAL STATISTICS:\n");
    printf("Total simulation time: %.2f seconds\n", total_time);
    printf("Total passengers served: %d (includes repeats)\n", shared_state->total_boarded_passengers);
    printf("Total rides completed: %d\n", shared_state->total_rides);
    printf("Average wait time in ticket queue: %.2f seconds\n", avg_ticket);
    printf("Average wait time in ride queue: %.2f seconds\n", avg_ride);
    printf("Average car utilization: %.2f%% (%.2f/%d passengers per ride)\n",
           util_percent, avg_util, car_capacity);

    shm_unlink("/duckpark");
    return 0;
}
