#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <getopt.h>
#include <time.h>
#include <stdarg.h>
#include <errno.h>

#define MAX_PASS 100
#define MAX_CARS 10

int num_passengers = 10;
int num_cars = 2;
int car_capacity = 3;
int car_wait_time = 5;
int ride_duration = 5;

pthread_mutex_t log_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t state_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t car_ready = PTHREAD_COND_INITIALIZER;
pthread_cond_t car_full = PTHREAD_COND_INITIALIZER;
pthread_cond_t car_empty = PTHREAD_COND_INITIALIZER;

typedef struct {
    int id;
    int onboard;
    int is_loading;
    int running;
} Car;

Car cars[MAX_CARS];

int passengers_waiting = 0;
int total_boarded = 0;
int total_unboarded = 0;
int next_car_to_load = -1;

void log_event(const char* fmt, ...) {
    pthread_mutex_lock(&log_lock);
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    char buf[9];
    strftime(buf, sizeof(buf), "%T", tm_info);
    printf("[Time: %s] ", buf);

    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    printf("\n");
    pthread_mutex_unlock(&log_lock);
}

void* passenger_thread(void* arg) {
    int id = *(int*)arg;
    free(arg);

    log_event("Passenger %d entered the park", id);

    int explore_time = rand() % 4 + 2;
    log_event("Passenger %d is exploring the park for %ds...", id, explore_time);
    sleep(explore_time);

    log_event("Passenger %d acquired a ticket", id);

    pthread_mutex_lock(&state_lock);
    passengers_waiting++;
    pthread_cond_broadcast(&car_ready);

    while (next_car_to_load == -1 || !cars[next_car_to_load].is_loading)
        pthread_cond_wait(&car_ready, &state_lock);

    int cid = next_car_to_load;
    cars[cid].onboard++;
    passengers_waiting--;
    log_event("Passenger %d boarded Car %d", id, cid + 1);

    if (cars[cid].onboard == car_capacity || total_boarded + passengers_waiting == num_passengers)
        pthread_cond_signal(&car_full);

    while (cars[cid].is_loading)
        pthread_cond_wait(&car_empty, &state_lock);

    total_unboarded++;
    log_event("Passenger %d unboarded Car %d", id, cid + 1);

    // Wake sleeping car threads in case they’re stuck waiting
    pthread_cond_broadcast(&car_ready);
    pthread_mutex_unlock(&state_lock);
    return NULL;
}

void* car_thread(void* arg) {
    int id = *(int*)arg;
    free(arg);

    while (1) {
        pthread_mutex_lock(&state_lock);

        if (total_boarded >= num_passengers && total_unboarded >= num_passengers) {
            pthread_mutex_unlock(&state_lock);
            break;
        }

        while (passengers_waiting == 0 && total_boarded < num_passengers)
            pthread_cond_wait(&car_ready, &state_lock);

        if (total_boarded >= num_passengers && total_unboarded >= num_passengers) {
            pthread_mutex_unlock(&state_lock);
            break;
        }

        cars[id].onboard = 0;
        cars[id].is_loading = 1;
        next_car_to_load = id;

        log_event("Car %d invoked load()", id + 1);
        pthread_cond_broadcast(&car_ready);

        struct timespec deadline;
        clock_gettime(CLOCK_REALTIME, &deadline);
        deadline.tv_sec += car_wait_time;

        while (cars[id].onboard < car_capacity &&
               total_boarded + cars[id].onboard < num_passengers) {
            if (pthread_cond_timedwait(&car_full, &state_lock, &deadline) == ETIMEDOUT)
                break;
        }

        if (cars[id].onboard == 0) {
            log_event("Car %d had no passengers; skipping ride.", id + 1);
            cars[id].is_loading = 0;
            pthread_mutex_unlock(&state_lock);
            continue;
        }

        total_boarded += cars[id].onboard;
        cars[id].is_loading = 0;
        pthread_cond_broadcast(&car_empty);
        pthread_mutex_unlock(&state_lock);

        log_event("Car %d departed with %d passengers", id + 1, cars[id].onboard);
        sleep(ride_duration);

        pthread_mutex_lock(&state_lock);
        log_event("Car %d invoked unload()", id + 1);
        pthread_mutex_unlock(&state_lock);
    }

    return NULL;
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
            case 'h':
            default:
                printf("Usage: ./park [OPTIONS]\n");
                printf("Options:\n");
                printf("  -n <num>  Number of passengers (default 10)\n");
                printf("  -c <num>  Number of cars (default 2)\n");
                printf("  -p <num>  Capacity per car (default 3)\n");
                printf("  -w <num>  Car wait time in seconds (default 5)\n");
                printf("  -r <num>  Ride duration in seconds (default 5)\n");
                printf("  -h        Display this help message\n");
                exit(0);
        }
    }
}

int main(int argc, char* argv[]) {
    srand(time(NULL));
    parse_args(argc, argv);

    printf("===== DUCK PARK SIMULATION (Part 2) =====\n");
    printf("Passengers: %d | Cars: %d | Capacity: %d | Wait: %d | Ride: %d\n",
           num_passengers, num_cars, car_capacity, car_wait_time, ride_duration);

    pthread_t cars_t[num_cars], passengers_t[num_passengers];

    for (int i = 0; i < num_cars; i++) {
        cars[i].id = i;
        cars[i].onboard = 0;
        cars[i].is_loading = 0;
        cars[i].running = 0;
        int* cid = malloc(sizeof(int));
        *cid = i;
        pthread_create(&cars_t[i], NULL, car_thread, cid);
    }

    for (int i = 0; i < num_passengers; i++) {
        int* pid = malloc(sizeof(int));
        *pid = i + 1;
        pthread_create(&passengers_t[i], NULL, passenger_thread, pid);
        usleep(50000);  // Stagger entry
    }

    for (int i = 0; i < num_passengers; i++)
        pthread_join(passengers_t[i], NULL);
    for (int i = 0; i < num_cars; i++)
        pthread_join(cars_t[i], NULL);

    log_event("Simulation complete.");
    return 0;
}
