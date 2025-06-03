// File: part1/park.c - Single Threaded Simulation
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>

pthread_mutex_t ticket_mutex = PTHREAD_MUTEX_INITIALIZER;
void* passenger_thread(void* arg) {
    int id = *((int*)arg);
    printf("Passenger %d is exploring the park...\n", id);
    sleep(2);

    pthread_mutex_lock(&ticket_mutex);
    printf("Passenger %d acquired a ticket.\n", id);
    pthread_mutex_unlock(&ticket_mutex);

    printf("Passenger %d boarded the car.\n", id);
    printf("Passenger %d is on the ride.\n", id);
    sleep(2);
    printf("Passenger %d unboarded.\n", id);
    return NULL;
}

int main() {
    pthread_t t;
    int id = 1;
    pthread_create(&t, NULL, passenger_thread, &id);
    pthread_join(t, NULL);
    return 0;
}

