#include <signal.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <stdio.h>
#include <stdbool.h>

struct buf_elem{
    long mtype;
    int mvalue;
};
struct portfel{
    int piec;
    int dwa;
    int jeden;
};
#define WOLNE 1
#define ZAJETE 2

static struct sembuf sbuf;

void podnies(int semid, int semnum){
    sbuf.sem_num = semnum;
    sbuf.sem_op = 1;
    sbuf.sem_flg = 0;
    if (semop(semid, &sbuf, 1) == -1){
        perror("Podnoszenie semafora");
        exit(1);
    }
}

void opusc(int semid, int semnum){
    sbuf.sem_num = semnum;
    sbuf.sem_op = -1;
    sbuf.sem_flg = 0;
    if (semop(semid, &sbuf, 1) == -1){
        perror("Opuszczanie semafora");
        exit(1);
    }
}

void term(int sig){                                                 /*Funkcja terminująca wszystkie procesy fryzjerów i klientów*/
    signal(SIGQUIT, SIG_IGN);                                       /*oraz usuwająca zasoby IPC*/
    kill(-getpid(), SIGQUIT);
    system("ipcrm -M 145289 -Q 145289 -Q 145290 -S 145289");
    exit(0);
}

#define cena 25                                                     //Cena usługi

int main(int argc, char* argv[]){
    if (argc < 4){
        printf("Za malo argumentow\n");
        exit(1);
    }
    int shmid, semid, msgid, msgidr, i, zarobek;
    struct buf_elem poczekalnia, reszta;
    struct portfel ptf;
    int *buf;
    int pid, tmp_reszta;
    bool wydane;
    int F = atoi(argv[1]);
    int N = atoi(argv[2]);
    int P = atoi(argv[3]);
    msgid = msgget(145289, IPC_CREAT|0600);
    if (msgid == -1){
        perror("Tworzenie kolejki komunikatow");
        exit(1);
    }
    msgidr = msgget(145290, IPC_CREAT|0600);
    if (msgidr == -1){
        perror("Tworzenie kolejki komunikatow");
        exit(1);
    }
    shmid = shmget(145289, 7*sizeof(int), IPC_CREAT|0600);
    if (shmid == -1){
        perror("Tworzenie pamieci");
        exit(1);
    }
    buf = (int*)shmat(shmid, NULL, 0);
    if (buf == NULL){
        perror("Przylaczenie pamieci");
        exit(1);
    }

    semid = semget(145289, 7, IPC_CREAT|0600);
    if (semid == -1){
        perror("Tworzenie semaforow");
        exit(1);
    }

    if (semctl(semid, 0, SETVAL, (int)N) == -1){
        perror("Inicjalizacja semafora 0");
        exit(1);
    }
    if (semctl(semid, 1, SETVAL, (int)1) == -1){
        perror("Inicjalizacja semafora 1");
        exit(1);
    }
    if (semctl(semid, 2, SETVAL, (int)0) == -1){
        perror("Inicjalizacja semafora 2");
        exit(1);
    }
    if (semctl(semid, 3, SETVAL, (int)1) == -1){
        perror("Inicjalizacja semafora 3");
        exit(1);
    }
    if (semctl(semid, 4, SETVAL, (int)0) == -1){
        perror("Inicjalizacja semafora 4");
        exit(1);
    }
    if (semctl(semid, 5, SETVAL, (int)0) == -1){
        perror("Inicjalizacja semafora 5");
        exit(1);
    }
    if (semctl(semid, 6, SETVAL, (int)1) == -1){
        perror("Inicjalizacja semafora 6");
        exit(1);
    }

    buf[0] = 0;
    buf[1] = 0;
    buf[2] = 0;

    poczekalnia.mtype = WOLNE;
    for (i=0; i<P; i++){
        if (msgsnd(msgid, &poczekalnia, sizeof(poczekalnia.mvalue), 0) == -1){
            perror("Inicjalizacja poczekalni");
            exit(1);
        }
    }

    for (i=0; i<F; i++){
        switch(fork()){
            case -1:
                perror("Tworzenie fryzjera");
                exit(1);
            case 0:
                ptf.piec = 0;
                ptf.dwa = 0;
                ptf.jeden = 0; 
                while (true){
                    if (msgrcv(msgid, &poczekalnia, sizeof(poczekalnia.mvalue), ZAJETE, 0) == -1){  //Oczekiwanie na klienta
                        perror("Odebranie komunikatu z poczekalni");
                        exit(1);
                    }
                    //printf("Fryzjer %d ma klienta\n", getpid());
                    pid = poczekalnia.mvalue;
                    opusc(semid, 0);                                                                //Zajęcie foltela
                    poczekalnia.mtype = WOLNE;
                    if (msgsnd(msgid, &poczekalnia, sizeof(poczekalnia.mvalue), 0) == -1){          //Zwolnienie miejsca w poczekalni
                        perror("Opuszczanie poczekalni");
                        exit(1);
                    }
                    reszta.mtype = pid;
                    opusc(semid, 3);                                                                //Początek sekcji rozliczenia
                    //printf("Fryzjer %d czeka na zaplate\n", getpid());
                    if (msgsnd(msgidr, &reszta, sizeof(reszta.mvalue), 0) == -1){                   //Wezwanie klienta do zapłaty
                        perror("Wyslanie komunikatu uslugi");
                        exit(1);
                    }
                    opusc(semid, 2);                                                                //Oczekiwanie na informację o kwocie
                    zarobek = buf[6];                                                               //Odczytanie wpłaconej kwoty
                    //printf("Fryzjer %d odczytal zaplate\n", getpid());
                    podnies(semid, 3);                                                              //Koniec sekcji rozliczenia
                    sleep(10);                                                                      //Wykonywanie usługi
                    if (msgsnd(msgidr, &reszta, sizeof(reszta.mvalue), 0) == -1){                   //Poinformowanie klienta o zakończeniu usługi
                        perror("Wyslanie komunikatu uslugi");
                        exit(1);
                    }
                    //printf("Fryzjer %d wykonal prace\n", getpid());
                    podnies(semid, 0);                                                              //Zwolnienie fotela
                    zarobek -= cena;                                                                //Początek sekcji wydawania reszty
                    tmp_reszta = zarobek;
                    wydane = false;
                    if (zarobek > 0){
                        while (true){
                            //printf("Fryzjer %d probuje wydac\n", getpid());
                            zarobek = tmp_reszta;
                            opusc(semid, 1);                                                        //Zablokowanie kasy
                            i = 0;
                            while (i <= buf[0] && zarobek-5 >= 0 && !wydane){
                                zarobek -= 5;
                                i++;
                                if (zarobek == 0){
                                    wydane = true;
                                }
                            }
                            ptf.piec = i;
                            i = 0;
                            while (i <= buf[1] && zarobek-2 >= 0 && !wydane){
                                zarobek -= 2;
                                i++;
                                if (zarobek == 0){
                                    wydane = true;
                                }
                            }
                            ptf.dwa = i;
                            i = 0;
                            while (i <= buf[2] && zarobek-1 >= 0 && !wydane){
                                zarobek -= 1;
                                i++;
                                if (zarobek == 0){
                                    wydane = true;
                                }
                            }
                            ptf.jeden = i;
                            if (wydane){                                                            //Można wydać resztę
                                buf[0] -= ptf.piec;
                                buf[1] -= ptf.dwa;
                                buf[2] -= ptf.jeden;
                                podnies(semid, 1);                                                  //Odblokowanie kasy
                                //printf("Fryzjer %d wydal reszte\n", getpid());
                                break;
                            }
                            else{                                                                   //Nie można wydać reszty
                                podnies(semid, 1);                                                  //Odblokowanie kasy
                                //printf("Fryzjer %d nie moze wydac\n", getpid());
                                opusc(semid, 4);                                                    //Czekaj aż dokonana zostanie wpłata
                            }
                        }
                    }                                                                               
                    opusc(semid, 6);                                                                //Umieszczenie reszty w pamięci
                    buf[3] = ptf.piec;
                    buf[4] = ptf.dwa;
                    buf[5] = ptf.jeden;
                    if (msgsnd(msgidr, &reszta, sizeof(reszta.mvalue), 0) == -1){                   //Poinformowanie klienta o wydaniu reszty
                        perror("Wyslanie komunikatu reszty");
                        exit(1);
                    }
                    ptf.piec = 0;
                    ptf.dwa = 0;
                    ptf.jeden = 0;                                                                               //Koniec sekcji wydawania reszty
                    //printf("Fryzjer %d wyszedl z salonu\n", getpid());
                }
                
        }
    }
    for (i=0; i<(P*2); i++){
        switch(fork()){
            case -1:
                perror("Tworzenie klienta");
                exit(1);
            case 0:
                ptf.piec = 0;
                ptf.dwa = 0;
                ptf.jeden = 0;
                srand(i);
                while (true){
                    //printf("%d %d %d %d\n", getpid(), ptf.piec, ptf.dwa, ptf.jeden);
                    zarobek = 0;                                                                    //Początek sekcji zarabiania
                    while (zarobek < cena){
                        switch(rand() % 3){
                            case 0:
                                ptf.piec++;
                                zarobek += 5;
                                break;
                            case 1:
                                ptf.dwa++;
                                zarobek += 2;
                                break;
                            case 2:
                                ptf.jeden++;
                                zarobek += 1;
                                break;
                        }
                    }                                                                              //Koniec sekcji zarabiania
                    //printf("Klient %d zarobil\n", getpid());
                    //printf("%d %d %d %d\n", getpid(), ptf.piec, ptf.dwa, ptf.jeden);
                    if (msgrcv(msgid, &poczekalnia, sizeof(poczekalnia.mvalue), WOLNE, IPC_NOWAIT) > -1){   //Sprawdź czy jest miejsce w poczekalni
                        poczekalnia.mtype = ZAJETE;
                        poczekalnia.mvalue = getpid();
                        if (msgsnd(msgid, &poczekalnia, sizeof(poczekalnia.mvalue), 0) == -1){              //Zajmij miejsce w poczekalni
                            perror("Wysylanie do poczekalni");
                            exit(1);
                        }
                        //printf("Klient %d w poczekalni\n", getpid());
                        if (msgrcv(msgidr, &reszta, sizeof(reszta.mvalue), getpid(), 0) == -1){             //Czekaj, aż fryzjer wywoła do zapłaty
                            perror("Odebranie komunikatu uslugi");
                            exit(1);
                        }
                        opusc(semid, 1);                                                                    //Zablokuj kasę
                        buf[0] += ptf.piec;
                        buf[1] += ptf.dwa;
                        buf[2] += ptf.jeden;    
                        podnies(semid, 1);                                                                  //Odblokuj kasę
                        podnies(semid, 4);                                                                  //Informacja, że do kasy coś wpłynęło
                        //printf("Klient %d wplacil do kasy\n", getpid());
                        zarobek = (ptf.piec * 5) + (ptf.dwa * 2) + (ptf.jeden);                             //Oblicz ile wpłaciłeś
                        ptf.piec = 0;
                        ptf.dwa = 0;
                        ptf.jeden = 0;
                        buf[6] = zarobek;                                                                   //Umieść kwotę w pamięci
                        podnies(semid, 2);                                                                  //Poinforuj fryzjera o możliwościu odczytania kwoty
                        //printf("Klient %d czeka na koniec uslugi\n", getpid());
                        if (msgrcv(msgidr, &reszta, sizeof(reszta.mvalue), getpid(), 0) == -1){             //Czekaj na koniec usługi
                            perror("Odebranie komunikatu uslugi");
                            exit(1);
                        }
                        //printf("Klient %d czeka na reszte\n", getpid());
                        if (msgrcv(msgidr, &reszta, sizeof(reszta.mvalue), getpid(), 0) == -1){             //Czekaj na resztę
                            perror("Odebranie komunikatu reszty");
                            exit(1);
                        }
                        ptf.piec = buf[3];                                                                  //Odbierz resztę
                        ptf.dwa = buf[4];
                        ptf.jeden = buf[5];
                        podnies(semid, 6);                                                                  //Zwolnij sekcję zapisu i odczytu reszty
                        //printf("Klient %d opuscil salon\n", getpid());
                    }
                    //printf("Klient %d poza salonem\n", getpid());
                    sleep(rand() % 20);                                                                     //Idź do pracy
                }
        }
    }
    signal(SIGINT, term);
    pause();
}