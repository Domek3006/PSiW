#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>

pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t c = PTHREAD_COND_INITIALIZER;

int doki;
int holowniki;

#define moc 10                                  //Ile ton może ciągnąć jeden holwnik
#define MAX 100                                 //Maksymalna liczba statków

void* statek(void *arg){
    int masa = *((int*)arg);
    pthread_mutex_lock(&m);
    while (!((doki > 0) && (((masa-1) / moc)+1 <= holowniki))){              //Sprawdź czy dostępne jest jedno miejsce
    //printf("Statek o masie %d czeka na zasoby\n", masa);                   //oraz wystarczająca liczba holowników
        pthread_cond_wait(&c, &m);                                           //jeśli nie, czekaj
    }
    //printf("Statek o masie %d znalazl zasoby\n", masa);
    doki--;                                                                 //Zajmij miejsce oraz holowniki              
    holowniki -= ((masa-1) / moc)+1;
    pthread_mutex_unlock(&m);
    srand(masa);
    sleep(rand() % 11);                                                     //Spędź jakiś czas w porcie
    //printf("Statek o masie %d chce odplynac\n", masa);
    pthread_mutex_lock(&m);
    doki++;                                                                 //Zwróć zasoby
    holowniki += ((masa-1) / moc)+1;
    pthread_mutex_unlock(&m);
    //printf("Statek o masie %d zwolnil zasoby\n", masa);
    pthread_cond_broadcast(&c);                                             //Przekaż wszystkim czekającym, że zostały zwolnione zasoby
}

int main(int argc, char* argv[]){
    if (argc < 4){
        printf("Za malo argumentow\n");
        exit(1);
    }
    doki = atoi(argv[1]);
    holowniki = atoi(argv[2]);
    int statki = atoi(argv[3]);
    pthread_t lista_statkow[MAX];
    if (statki > MAX){
        printf("Zbyt duzo statkow\n");
        exit(1);
    }
    srand(statki);
    for (int i=0; i<statki; i++){
        int *masa = malloc(sizeof(*masa));
        if (masa == NULL){
            perror("Blad tworzenia masy");
            exit(1);
        }
        *masa = rand() % ((holowniki * moc) + 1);
        if (*masa == 0){
            *masa++;
        }
        if (pthread_create(&lista_statkow[i], NULL, statek, masa) == -1){   //Stwórz statki o losowej masie
            perror("Tworzenie statku");
            exit(1);
        }
    }
    for (int i=0; i<statki; i++){
        if (pthread_join(lista_statkow[i], NULL) != 0){                    //Poczekaj aż wszystkie statki zostaną obsłużone
            perror("Doloczenie watku");
            exit(1);
        }
        printf("Statek %d odplynal poprawnie\n", i+1);
    }
}