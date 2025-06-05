#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <stdarg.h>
#include <errno.h>
#include <getopt.h>

#define MAX_PASS 200
#define MAX_CARS 10

int num_passengers = 20;
int num_cars = 3;
int car_capacity = 5;
int car_wait_time = 10;
int ride_duration = 8;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t log_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t load_cond = PTHREAD_COND_INITIALIZER;
pthread_cond_t ready_cond = PTHREAD_COND_INITIALIZER;

int passenger_queue[MAX_PASS];
int queue_front = 0, queue_back = 0;
int total_boarded = 0, total_completed = 0;

int car_index = 0;

void log_event(const char* fmt, ...) {
    pthread_mutex_lock(&log_lock);
    time_t now = time(NULL);
    struct tm* t = localtime(&now);
    char buf[9];
    strftime(buf, sizeof(buf), "%T", t);
    printf("[Time: %s] ", buf);
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    printf("\n");
    pthread_mutex_unlock(&log_lock);
}

void enqueue_passenger(int id) {
    passenger_queue[queue_back++] = id;
}

int dequeue_passenger() {
    if (queue_front == queue_back) return -1;
    return passenger_queue[queue_front++];
}

void* passenger_thread(void* arg) {
    int id = *(int*)arg;
    free(arg);

    log_event("Passenger %d entered the park", id);
    int explore_time = (rand() % 10) + 1;
    log_event("Passenger %d is exploring for %d seconds", id, explore_time);
    sleep(explore_time);

    log_event("Passenger %d is getting a ticket", id);

    pthread_mutex_lock(&mutex);
    enqueue_passenger(id);
    pthread_cond_signal(&load_cond);
    pthread_mutex_unlock(&mutex);

    return NULL;
}

void* car_thread(void* arg) {
    int id = *(int*)arg;
    free(arg);

    while (1) {
        pthread_mutex_lock(&mutex);
        while (car_index != id && total_completed < num_passengers)
            pthread_cond_wait(&ready_cond, &mutex);

        if (total_completed >= num_passengers) {
            pthread_mutex_unlock(&mutex);
            break;
        }

        log_event("Car %d invoked load()", id + 1);
        int boarded = 0;
        struct timespec deadline;
        clock_gettime(CLOCK_REALTIME, &deadline);
        deadline.tv_sec += car_wait_time;

        while (boarded < car_capacity && total_boarded + boarded < num_passengers) {
            while (queue_front == queue_back) {
                if (pthread_cond_timedwait(&load_cond, &mutex, &deadline) == ETIMEDOUT) break;
            }

            if (queue_front == queue_back) break;

            int pid = dequeue_passenger();
            log_event("Passenger %d boarded Car %d", pid, id + 1);
            boarded++;
        }

        if (boarded == 0) {
            log_event("Car %d skipped ride due to no passengers", id + 1);
            car_index = (car_index + 1) % num_cars;
            pthread_cond_broadcast(&ready_cond);
            pthread_mutex_unlock(&mutex);
            continue;
        }

        total_boarded += boarded;
        log_event("Car %d departed with %d passengers", id + 1, boarded);
        pthread_mutex_unlock(&mutex);

        sleep(ride_duration);

        pthread_mutex_lock(&mutex);
        log_event("Car %d invoked unload()", id + 1);
        total_completed += boarded;
        car_index = (car_index + 1) % num_cars;
        pthread_cond_broadcast(&ready_cond);
        pthread_mutex_unlock(&mutex);
    }

    return NULL;
}

void usage() {
    printf("Duck Park Simulation (Part 2)\n");
    printf("Usage: ./park [OPTIONS]\n");
    printf(" -n INT  Number of passengers\n");
    printf(" -c INT  Number of cars\n");
    printf(" -p INT  Car capacity\n");
    printf(" -w INT  Car wait time (seconds)\n");
    printf(" -r INT  Ride duration (seconds)\n");
    printf(" -h      Show this help message\n");
}

void parse_args(int argc, char* argv[]) {
    int opt;
    while ((opt = getopt(argc, argv, "n:c:p:w:r:h")) != -1) {
        switch (opt) {
            case 'n': num_passengers = atoi(optarg); break;
            case 'c': num_cars = atoi(optarg); break;
            case 'p': car_capacity = atoi(optarg); break;
            case 'w': car_wait_time = atoi(optarg); break;
            case 'r': ride_duration = atoi(optarg); break;
            case 'h': usage(); exit(0);
            default: usage(); exit(1);
        }
    }
}

int main(int argc, char* argv[]) {
    srand(time(NULL));
    parse_args(argc, argv);

    printf("===== DUCK PARK SIMULATION (Part 2) =====\n");
    printf("Simulation started with parameters:\n");
    printf("- Number of passengers: %d\n", num_passengers);
    printf("- Number of cars: %d\n", num_cars);
    printf("- Car capacity: %d\n", car_capacity);
    printf("- Car wait time: %d seconds\n", car_wait_time);
    printf("- Ride duration: %d seconds\n", ride_duration);

    pthread_t car_threads[num_cars];
    pthread_t passenger_threads[num_passengers];

    for (int i = 0; i < num_cars; i++) {
        int* cid = malloc(sizeof(int)); *cid = i;
        pthread_create(&car_threads[i], NULL, car_thread, cid);
    }

    for (int i = 0; i < num_passengers; i++) {
        int* pid = malloc(sizeof(int)); *pid = i + 1;
        pthread_create(&passenger_threads[i], NULL, passenger_thread, pid);
    }

    for (int i = 0; i < num_passengers; i++)
        pthread_join(passenger_threads[i], NULL);

    pthread_mutex_lock(&mutex);
    pthread_cond_broadcast(&ready_cond);
    pthread_mutex_unlock(&mutex);

    for (int i = 0; i < num_cars; i++)
        pthread_join(car_threads[i], NULL);

    printf("Simulation complete. Total passengers served: %d\n", total_completed);
    return 0;
}
