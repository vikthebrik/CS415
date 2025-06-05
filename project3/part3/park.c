#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <getopt.h>
#include <time.h>
#include <stdarg.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>
#include <errno.h>

#define MAX_PASS 100
#define MAX_CARS 10

typedef struct {
    int ticket_queue[MAX_PASS];
    int ride_queue[MAX_PASS];
    int ticket_q_len;
    int ride_q_len;
    int exploring;
    int riding;
    int done;
    struct {
        int loading;
        int running;
        int onboard;
    } cars[MAX_CARS];
} ParkState;

int num_passengers = 20;
int num_cars = 3;
int car_capacity = 5;
int car_wait_time = 10;
int ride_duration = 8;

ParkState* state;
pthread_mutex_t state_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t log_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t car_ready = PTHREAD_COND_INITIALIZER;
pthread_cond_t car_full = PTHREAD_COND_INITIALIZER;
pthread_cond_t car_empty = PTHREAD_COND_INITIALIZER;

int total_boarded = 0, total_unboarded = 0;
int next_car_to_load = -1;

void log_event(const char* fmt, ...) {
    pthread_mutex_lock(&log_lock);
    time_t now = time(NULL);
    struct tm* tinfo = localtime(&now);
    char buf[9];
    strftime(buf, sizeof(buf), "%T", tinfo);
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

    pthread_mutex_lock(&state_lock);
    state->exploring--;
    state->ticket_queue[state->ticket_q_len++] = id;
    log_event("Passenger %d finished exploring, heading to ticket booth", id);
    pthread_mutex_unlock(&state_lock);

    pthread_mutex_lock(&state_lock);
    pthread_cond_broadcast(&car_ready);

    while (next_car_to_load == -1 || !state->cars[next_car_to_load].loading)
        pthread_cond_wait(&car_ready, &state_lock);

    int cid = next_car_to_load;
    state->cars[cid].onboard++;
    state->ride_queue[state->ride_q_len++] = id;
    state->ticket_q_len--;
    log_event("Passenger %d boarded Car %d", id, cid + 1);

    if (state->cars[cid].onboard == car_capacity ||
        total_boarded + state->cars[cid].onboard == num_passengers)
        pthread_cond_signal(&car_full);

    while (state->cars[cid].loading)
        pthread_cond_wait(&car_empty, &state_lock);

    total_unboarded++;
    state->ride_q_len--;
    state->riding--;
    state->done++;
    log_event("Passenger %d unboarded Car %d", id, cid + 1);
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

        while (state->ticket_q_len == 0 &&
               total_boarded < num_passengers)
            pthread_cond_wait(&car_ready, &state_lock);

        if (total_boarded >= num_passengers && total_unboarded >= num_passengers) {
            pthread_mutex_unlock(&state_lock);
            break;
        }

        state->cars[id].loading = 1;
        state->cars[id].onboard = 0;
        next_car_to_load = id;

        log_event("Car %d invoked load()", id + 1);
        pthread_cond_broadcast(&car_ready);

        struct timespec deadline;
        clock_gettime(CLOCK_REALTIME, &deadline);
        deadline.tv_sec += car_wait_time;

        while (state->cars[id].onboard < car_capacity &&
               total_boarded + state->cars[id].onboard < num_passengers) {
            if (pthread_cond_timedwait(&car_full, &state_lock, &deadline) == ETIMEDOUT)
                break;
        }

        if (state->cars[id].onboard == 0) {
            log_event("Car %d had no passengers; skipping ride.", id + 1);
            state->cars[id].loading = 0;
            pthread_mutex_unlock(&state_lock);
            continue;
        }

        total_boarded += state->cars[id].onboard;
        state->riding += state->cars[id].onboard;
        state->cars[id].loading = 0;
        state->cars[id].running = 1;
        pthread_cond_broadcast(&car_empty);
        pthread_mutex_unlock(&state_lock);

        log_event("Car %d departed with %d passengers", id + 1, state->cars[id].onboard);
        sleep(ride_duration);

        pthread_mutex_lock(&state_lock);
        state->cars[id].running = 0;
        log_event("Car %d invoked unload()", id + 1);
        pthread_mutex_unlock(&state_lock);
    }

    return NULL;
}

void* monitor_thread(void* _) {
    while (state->done < num_passengers) {
        sleep(5);
        pthread_mutex_lock(&state_lock);

        time_t now = time(NULL);
        struct tm* t = localtime(&now);
        char buf[9];
        strftime(buf, sizeof(buf), "%T", t);
        printf("[Monitor] System State at %s\n", buf);

        printf("Ticket Queue: [");
        for (int i = 0; i < state->ticket_q_len; i++)
            printf("P%d ", state->ticket_queue[i]);
        printf("]\n");

        printf("Ride Queue: [");
        for (int i = 0; i < state->ride_q_len; i++)
            printf("P%d ", state->ride_queue[i]);
        printf("]\n");

        for (int i = 0; i < num_cars; i++) {
            printf("Car %d Status: ", i + 1);
            if (state->cars[i].running)
                printf("RUNNING (%d/%d passengers)\n", state->cars[i].onboard, car_capacity);
            else if (state->cars[i].loading)
                printf("LOADING (%d/%d passengers)\n", state->cars[i].onboard, car_capacity);
            else
                printf("WAITING (0/%d passengers)\n", car_capacity);
        }

        printf("Passengers in park: %d (%d exploring, %d in queues, %d on rides)\n\n",
               num_passengers - state->done, state->exploring,
               state->ticket_q_len + state->ride_q_len, state->riding);

        pthread_mutex_unlock(&state_lock);
    }

    printf("[Monitor] FINAL STATISTICS:\n");
    printf("Total passengers served: %d\n", num_passengers);
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
                printf(" -n INT  Number of passengers (default 20)\n");
                printf(" -c INT  Number of cars (default 3)\n");
                printf(" -p INT  Car capacity (default 5)\n");
                printf(" -w INT  Car wait time (seconds, default 10)\n");
                printf(" -r INT  Ride duration (seconds, default 8)\n");
                printf(" -h      Help\n");
                exit(0);
        }
    }
}

int main(int argc, char* argv[]) {
    srand(time(NULL));
    parse_args(argc, argv);

    printf("===== DUCK PARK SIMULATION (Part 3) =====\n");
    printf("[Monitor] Simulation started with parameters:\n");
    printf("- Number of passenger threads: %d\n", num_passengers);
    printf("- Number of cars: %d\n", num_cars);
    printf("- Capacity per car: %d\n", car_capacity);
    printf("- Park exploration time: 2-5 seconds\n");
    printf("- Car waiting period: %d seconds\n", car_wait_time);
    printf("- Ride duration: %d seconds\n", ride_duration);

    int fd = shm_open("/duck_park", O_CREAT | O_RDWR, 0666);
    ftruncate(fd, sizeof(ParkState));
    state = mmap(NULL, sizeof(ParkState), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    memset(state, 0, sizeof(ParkState));
    state->exploring = num_passengers;

    pthread_t cars[num_cars], passengers[num_passengers], monitor;
    pthread_create(&monitor, NULL, monitor_thread, NULL);

    for (int i = 0; i < num_cars; i++) {
        int* cid = malloc(sizeof(int));
        *cid = i;
        pthread_create(&cars[i], NULL, car_thread, cid);
    }

    for (int i = 0; i < num_passengers; i++) {
        int* pid = malloc(sizeof(int));
        *pid = i + 1;
        pthread_create(&passengers[i], NULL, passenger_thread, pid);
        usleep(50000);
    }

    for (int i = 0; i < num_passengers; i++) pthread_join(passengers[i], NULL);
    for (int i = 0; i < num_cars; i++) pthread_join(cars[i], NULL);
    pthread_join(monitor, NULL);

    shm_unlink("/duck_park");
    return 0;
}
