// Duck Park Simulation (Final Version with Enhanced Stats)
#define _GNU_SOURCE
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
    int total_passengers;
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
    int total_passengers_served;
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
int car_loading_now = 0;

// Time utils
double now_seconds() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec / 1e6;
}

void log_event(const char *fmt, ...) {
    double rel_time = now_seconds() - program_start;
    printf("[Time: %.2f] ", rel_time);
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    printf("\n");
    fflush(stdout);
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

// Monitor
void *monitor_loop(void *arg) {
    while (1) {
        sleep(5);
        pthread_mutex_lock(&mutex);

        printf("\n[Monitor] State at %.2f sec\n", now_seconds() - program_start);
        printf("Ticket Queue: [");
        for (int i = 0; i < shared_state->ticket_queue.count; ++i)
            printf("P%d ", shared_state->ticket_queue.ids[i] + 1);
        printf("]\nRide Queue: [");
        for (int i = 0; i < shared_state->ride_queue.count; ++i)
            printf("P%d ", shared_state->ride_queue.ids[i] + 1);
        printf("]\n");

        for (int i = 0; i < n_cars; ++i) {
            Car *c = &shared_state->cars[i];
            const char *status = (c->status == IDLE ? "IDLE" :
                                  c->status == LOADING ? "LOADING" :
                                  c->status == RIDING ? "RIDING" : "UNLOADING");
            printf("Car %d: %s (%d/%d)\n", i + 1, status, c->onboard_count, car_capacity);
        }

        printf("Passengers: %d exploring | %d in queues | %d riding\n\n",
               shared_state->exploring_count,
               shared_state->queueing_count,
               shared_state->riding_count);

        if (shared_state->served_once >= n_passengers)
        {
            pthread_mutex_unlock(&mutex);
            break;
        }
        pthread_mutex_unlock(&mutex);
    }
    return NULL;
}

// Passenger thread
void *passenger_thread(void *arg) {
    int id = *((int *)arg);
    free(arg);
    passengers[id].id = id;

    while (1) {
        pthread_mutex_lock(&mutex);
        shared_state->exploring_count++;
        pthread_mutex_unlock(&mutex);

        int explore_time = (rand() % (explore_max - explore_min + 1)) + explore_min;
        log_event("Passenger %d exploring for %d sec", id + 1, explore_time);
        sleep(explore_time);

        double ticket_start = now_seconds();
        pthread_mutex_lock(&mutex);
        shared_state->exploring_count--;
        enqueue(&shared_state->ticket_queue, id);
        enqueue(&shared_state->ride_queue, id);
        shared_state->queueing_count++;
        log_event("Passenger %d is getting ticket", id + 1);

        if (shared_state->ride_queue.count >= car_capacity && car_loading_now == 0) {
            car_loading_now = 1;
            pthread_cond_broadcast(&car_ready_cond);
        }
        pthread_mutex_unlock(&mutex);

        while (1) {
            int onboard = 0;
            pthread_mutex_lock(&mutex);
            for (int i = 0; i < n_cars; ++i) {
                Car *c = &shared_state->cars[i];
                for (int j = 0; j < c->onboard_count; ++j)
                    if (c->onboard[j] == id) onboard = 1;
            }
            pthread_mutex_unlock(&mutex);
            if (onboard) break;
            usleep(10000);
        }

        pthread_mutex_lock(&mutex);
        double ride_start = now_seconds();
        passengers[id].ticket_wait_time += (ride_start - ticket_start);
        pthread_mutex_unlock(&mutex);

        while (1) {
            int still_on = 0;
            pthread_mutex_lock(&mutex);
            for (int i = 0; i < n_cars; ++i) {
                Car *c = &shared_state->cars[i];
                for (int j = 0; j < c->onboard_count; ++j)
                    if (c->onboard[j] == id) still_on = 1;
            }
            pthread_mutex_unlock(&mutex);
            if (!still_on) break;
            usleep(10000);
        }

        pthread_mutex_lock(&mutex);
        double ride_end = now_seconds();
        passengers[id].ride_wait_time += (ride_end - ride_start);
        passengers[id].total_rides++;
        shared_state->riding_count--;
        shared_state->total_passengers_served++;
        if (!passengers[id].has_ridden) {
            passengers[id].has_ridden = 1;
            shared_state->served_once++;
            log_event("Passenger %d completed first ride", id + 1);
        }
        pthread_mutex_unlock(&mutex);

        if (shared_state->served_once >= n_passengers) break;
    }
    return NULL;
}

// Car thread
void *car_thread(void *arg) {
    int id = *((int *)arg);
    free(arg);
    Car *car = &shared_state->cars[id];
    car->id = id;

    while (1) {
        pthread_mutex_lock(&mutex);
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += wait_time;

        while (shared_state->ride_queue.count < car_capacity &&
               shared_state->served_once < n_passengers)
        {
            if (pthread_cond_timedwait(&car_ready_cond, &mutex, &ts) == ETIMEDOUT)
                break;
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
        }
        log_event("Car %d departing with %d passengers", id + 1, car->onboard_count);
        car->status = RIDING;
        pthread_mutex_unlock(&mutex);

        sleep(ride_duration);

        pthread_mutex_lock(&mutex);
        car->status = UNLOADING;
        car->rides_completed++;
        car->utilization_total += car->onboard_count;
        shared_state->total_rides++;
        car->onboard_count = 0;
        car->status = IDLE;
        car_loading_now = 0;
        pthread_cond_broadcast(&car_ready_cond);
        pthread_mutex_unlock(&mutex);
    }
    return NULL;
}

// Args
void print_help() {
    printf("Usage: ./park [-n passengers] [-c cars] [-p capacity] [-w wait] [-r ride] [-e min:max]\n");
}

void parse_args(int argc, char *argv[]) {
    int opt;
    while ((opt = getopt(argc, argv, "n:c:p:w:r:e:h")) != -1) {
        switch (opt) {
            case 'n': n_passengers = atoi(optarg); break;
            case 'c': n_cars = atoi(optarg); break;
            case 'p': car_capacity = atoi(optarg); break;
            case 'w': wait_time = atoi(optarg); break;
            case 'r': ride_duration = atoi(optarg); break;
            case 'e': sscanf(optarg, "%d:%d", &explore_min, &explore_max); break;
            case 'h': print_help(); exit(0);
        }
    }
}

// Main
int main(int argc, char *argv[]) {
    srand(time(NULL));
    parse_args(argc, argv);

    printf("===== DUCK PARK SIMULATION =====\n[Monitor] Simulation started with parameters:\n");
    printf("- Number of passenger threads: %d\n- Number of cars: %d\n- Capacity per car: %d\n", n_passengers, n_cars, car_capacity);
    printf("- Park exploration time: %d–%d seconds\n- Car waiting period: %d seconds\n- Ride duration: %d seconds\n", explore_min, explore_max, wait_time, ride_duration);

    int fd = shm_open("/duckpark", O_CREAT | O_RDWR, 0666);
    ftruncate(fd, sizeof(SharedState));
    shared_state = mmap(NULL, sizeof(SharedState), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    memset(shared_state, 0, sizeof(SharedState));
    shared_state->total_passengers = n_passengers;

    pthread_t pt[MAX_PASSENGERS], ct[MAX_CARS], mon;
    program_start = now_seconds();
    pthread_create(&mon, NULL, monitor_loop, NULL);

    for (int i = 0; i < n_cars; ++i) {
        int *id = malloc(sizeof(int)); *id = i;
        pthread_create(&ct[i], NULL, car_thread, id);
    }

    for (int i = 0; i < n_passengers; ++i) {
        int *id = malloc(sizeof(int)); *id = i;
        pthread_create(&pt[i], NULL, passenger_thread, id);
        sleep(2);
    }

    for (int i = 0; i < n_passengers; ++i) pthread_join(pt[i], NULL);
    for (int i = 0; i < n_cars; ++i) pthread_join(ct[i], NULL);
    pthread_join(mon, NULL);

    // Final stats
    double elapsed = now_seconds() - program_start;
    double ticket_sum = 0, ride_sum = 0;
    for (int i = 0; i < n_passengers; ++i) {
        ticket_sum += passengers[i].ticket_wait_time;
        ride_sum += passengers[i].ride_wait_time;
    }

    // Calculate correct stats
    double avg_passengers_per_ride = (double)shared_state->total_passengers_served / shared_state->total_rides;
    double utilization_percentage = (avg_passengers_per_ride / car_capacity) * 100;

    printf("\n[Monitor] FINAL STATISTICS:\n");
    printf("Total simulation time: %.2f sec\n", elapsed);  
    printf("Total passengers served: %d\n", shared_state->total_passengers_served);
    printf("Total rides completed: %d\n", shared_state->total_rides);
    printf("Average wait time in ticket queue: %.2f seconds\n", ticket_sum / n_passengers);
    printf("Average wait time in ride queue: %.2f seconds\n", ride_sum / n_passengers);
    printf("Average car utilization: %.2f%% (%.2f/%d passengers per ride)\n",
       utilization_percentage, avg_passengers_per_ride, car_capacity);


    shm_unlink("/duckpark");
    return 0;
}
