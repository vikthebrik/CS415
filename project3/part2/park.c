// File: part2/park.c - Multi-threaded Simulation (Fixed: final unload sync with condition variable)
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>

#define MAX_PASSENGERS 100

int n = 10, capacity = 3, waiting = 0;
int total_boarded = 0;
int total_passengers = 10;
int ride_ready = 0;
int unboarded = 0;

sem_t ticket_booth;
sem_t board_queue, unboard_queue;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t car_full = PTHREAD_COND_INITIALIZER;
pthread_cond_t car_empty = PTHREAD_COND_INITIALIZER;

void* passenger(void* arg) {
    int id = *((int*)arg);
    int explore_time = rand() % 5 + 1;
    printf("[P%d] exploring for %ds...\n", id, explore_time);
    sleep(explore_time);

    sem_wait(&ticket_booth);
    printf("[P%d] got ticket.\n", id);
    sem_post(&ticket_booth);

    sem_wait(&board_queue);
    printf("[P%d] boarded.\n", id);

    pthread_mutex_lock(&lock);
    waiting++;
    total_boarded++;
    if (waiting == ride_ready) {
        pthread_cond_signal(&car_full);
    }
    pthread_mutex_unlock(&lock);

    sem_wait(&unboard_queue);
    printf("[P%d] unboarded.\n", id);

    pthread_mutex_lock(&lock);
    unboarded++;
    if (unboarded == ride_ready) {
        pthread_cond_signal(&car_empty);
    }
    pthread_mutex_unlock(&lock);

    return NULL;
}

void* car(void* arg) {
    int id = *((int*)arg);
    while (1) {
        pthread_mutex_lock(&lock);
        int remaining = total_passengers - total_boarded;
        if (remaining == 0) {
            pthread_mutex_unlock(&lock);
            break;
        }
        ride_ready = remaining < capacity ? remaining : capacity;
        unboarded = 0;
        waiting = 0;
        pthread_mutex_unlock(&lock);

        printf("[Car %d] loading %d passengers...\n", id, ride_ready);
        for (int i = 0; i < ride_ready; i++) sem_post(&board_queue);

        pthread_mutex_lock(&lock);
        while (waiting < ride_ready) {
            pthread_cond_wait(&car_full, &lock);
        }
        pthread_mutex_unlock(&lock);

        printf("[Car %d] running.\n", id);
        sleep(2);

        printf("[Car %d] unloading.\n", id);
        for (int i = 0; i < ride_ready; i++) sem_post(&unboard_queue);

        pthread_mutex_lock(&lock);
        while (unboarded < ride_ready) {
            pthread_cond_wait(&car_empty, &lock);
        }
        pthread_mutex_unlock(&lock);
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    pthread_t pt[MAX_PASSENGERS], ct;
    int ids[MAX_PASSENGERS];
    srand(time(NULL));

    sem_init(&ticket_booth, 0, 1);
    sem_init(&board_queue, 0, 0);
    sem_init(&unboard_queue, 0, 0);

    pthread_create(&ct, NULL, car, &(int){1});

    for (int i = 0; i < n; i++) {
        ids[i] = i+1;
        pthread_create(&pt[i], NULL, passenger, &ids[i]);
        sleep(1);
    }
    for (int i = 0; i < n; i++) pthread_join(pt[i], NULL);
    pthread_join(ct, NULL);
    return 0;
}
