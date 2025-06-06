// part1/park.c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>
#include <stdarg.h>

sem_t ticket_booth;
sem_t board_sem;
sem_t unboard_sem;
sem_t ready_to_load;
pthread_mutex_t log_lock = PTHREAD_MUTEX_INITIALIZER;

// Timestamped log printing
void log_event(const char* format, ...) {
    pthread_mutex_lock(&log_lock);
    time_t t = time(NULL);
    struct tm* tm_info = localtime(&t);
    char buf[9];
    strftime(buf, 9, "%T", tm_info);
    printf("[Time: %s] ", buf);

    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);

    printf("\n");
    fflush(stdout);
    pthread_mutex_unlock(&log_lock);
}

void* passenger(void* arg) {
    log_event("Passenger 1 entered the park");

    log_event("Passenger 1 is exploring the park...");
    sleep(2);

    log_event("Passenger 1 heading to ticket booth...");
    sem_wait(&ticket_booth);
    log_event("Passenger 1 acquired a ticket");
    sem_post(&ticket_booth);

    log_event("Passenger 1 joined the ride queue");
    sem_post(&ready_to_load);  // Let car know passenger is ready

    sem_wait(&board_sem);
    log_event("Passenger 1 boarded Car 1");

    sem_wait(&unboard_sem);
    log_event("Passenger 1 unboarded Car 1");
    return NULL;
}

void* car(void* arg) {
    sem_wait(&ready_to_load);  // Wait until passenger has ticket and is queued

    log_event("Car 1 invoked load()");
    sem_post(&board_sem);  // Allow passenger to board
    sleep(1);

    log_event("Car 1 departed for its run");
    sleep(3);

    log_event("Car 1 invoked unload()");
    sem_post(&unboard_sem);
    return NULL;
}

int main() {
    printf("===== DUCK PARK SIMULATION (Part 1) =====\n");

    pthread_t t_passenger, t_car;
    sem_init(&ticket_booth, 0, 1);
    sem_init(&board_sem, 0, 0);
    sem_init(&unboard_sem, 0, 0);
    sem_init(&ready_to_load, 0, 0);

    pthread_create(&t_car, NULL, car, NULL);
    pthread_create(&t_passenger, NULL, passenger, NULL);

    pthread_join(t_passenger, NULL);
    pthread_join(t_car, NULL);

    return 0;
}
