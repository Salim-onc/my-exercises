
/* C Program to create two threads: one responsible for printing even numbers and the other for odd numbers
 * alternating yto print both numbers until they reach a limit.
 */

#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>

#define MAX 10

int count = 1;
sem_t odd_sem, even_sem;  // Create 2 semphores for even and odd

void* print_odd(void* arg) {
    while (count <= MAX) {
        // Wait if it's not odd's turn
        sem_wait(&odd_sem);

        if (count <= MAX) {
            printf("%d ", count);
            count++;
        }

        sem_post(&even_sem);
    }
    return NULL;
}

void* print_even(void* arg) {
    while (count <= MAX) {
        // Wait if it's not even's turn
        sem_wait(&even_sem);

        if (count <= MAX) {
            printf("%d ", count);
            count++;
        }

        sem_post(&odd_sem);
    }
    return NULL;
}

int main() {
    pthread_t t1, t2;

    sem_init(&odd_sem, 0, 1);  // Odd starts first
    sem_init(&even_sem, 0, 0); // Even waits

    pthread_create(&t1, NULL, print_odd, NULL); //create 2 threads
    pthread_create(&t2, NULL, print_even, NULL);

    pthread_join(t1, NULL);
    pthread_join(t2, NULL);

    sem_destroy(&odd_sem);
    sem_destroy(&even_sem);

    return 0;
}
