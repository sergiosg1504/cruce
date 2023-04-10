#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/wait.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <unistd.h>
#include <string.h>
#include <pwd.h>
#include <fcntl.h>
#include "cruce.h"

//  ROBERTO MERCHAN GONZALEZ robertomergon@usal.es
//  SERGIO SANCHEZ GARCIA    sergiosg@usal.es

void liberar();  // Manejadora de SIGINT
void error(int);
void ciclo_sem();
void opSem(int, int);
void esCoche(pid_t);
void esPeaton(pid_t);
void recibirMSG(struct posiciOn);
void enviarMSG(struct posiciOn);
void ponSem(int, int);
void despedida();

typedef struct datos{
	int semaforos;
	int buzon;
	int memoria;
} datos;

union semun{
         int val;
         struct semid_ds *buf;
         ushort_t *array;
}semf;

typedef struct mensaje{
	long tipo;
} mensaje;

pid_t PIDPADRE;
datos d;

int main(int argc, char *argv[]){
	// Variables
	pid_t pidSem;
	int creacion, i, x, y;
	mensaje men;
	// Comprobacion de linea de ordenes
	if(argc != 3){
		fprintf(stderr,"Error en el numero de parametros por la linea de ordenes\n");
		exit(1);
	}
	if(atoi(argv[1]) < 3 || atoi(argv[1]) > 49 || atoi(argv[2]) < 0){
		fprintf(stderr,"Error en los valores del parametro\n");
		exit(1);
	}
	PIDPADRE = getpid();
	// Manejo de SIGINT
	struct sigaction sigInt;
	sigInt.sa_handler = liberar;
	sigfillset(&sigInt.sa_mask);
	sigdelset(&sigInt.sa_mask, SIGINT);
	sigInt.sa_flags = 0;
	sigaction(SIGINT,&sigInt,NULL);
	sigInt.sa_handler = SIG_IGN;
	sigdelset(&sigInt.sa_mask, SIGCLD);
	sigaction(SIGCLD,&sigInt,NULL);
	// Reserva de semaforos, buzon y memoria compartida
	if((d.semaforos = semget(IPC_PRIVATE,9,IPC_CREAT | 0600)) == -1){
        	pon_error("Error en la creacion del los semaforos");
        	kill(PIDPADRE, SIGINT);
	}
	if((d.buzon = msgget(IPC_PRIVATE,IPC_CREAT | 0600)) == -1){
		pon_error("Error en la creacion del buzon");
		kill(PIDPADRE,SIGINT);
	}
	if((d.memoria = shmget(IPC_PRIVATE,256, IPC_CREAT | 0600)) == -1){
        	pon_error("Error en la creacion de la memoria compartida");
        	kill(PIDPADRE, SIGINT);
	}
	// Inicio de CRUCE
	if(CRUCE_inicio(atoi(argv[2]), atoi(argv[1]), d.semaforos, shmat(d.memoria, 0, 0)) == -1){
		pon_error("Error en la funcion CRUCE_inicio");
		kill(PIDPADRE, SIGINT);
	}
	// Inicio de semaforos
	ponSem(1, 1);
	ponSem(2, 0);
	ponSem(3, 0);
	ponSem(4, 1);
	ponSem(5, atoi(argv[1])-2);
	ponSem(6, 0);
	ponSem(7, 1);
	ponSem(8, 0);
	// Envio de mensajes cruce vertical y horizontal para coches
	for(i = 7; i <= 11; i++){
		men.tipo = 100 + i;
		if(msgsnd(d.buzon, &men, sizeof(mensaje)-sizeof(long), 0) == -1)
			error(7);
	}
	for(i = 5; i <= 19; i++){
		men.tipo = 200 + i;
		if(msgsnd(d.buzon, &men, sizeof(mensaje)-sizeof(long), 0) == -1)
			error(7);
	}
	// Envio de mensajes peatones
	for(x = 90000; x <= 95000; x += 100){
		for(y = 0; y <= 16; y++){
			men.tipo = x + y;
			if(msgsnd(d.buzon, &men, sizeof(mensaje)-sizeof(long), 0) == -1)
				error(7);
		}
	}
	switch(pidSem = fork()){
		case -1:
			pon_error("Error en la creacion de hijos");
        		kill(PIDPADRE, SIGINT);
        		exit(1);
			break;
		case 0:
			if(CRUCE_pon_semAforo(SEM_C1, VERDE) == -1)
				error(2);
			if(CRUCE_pon_semAforo(SEM_C2, ROJO) == -1)
				error(2);
			if(CRUCE_pon_semAforo(SEM_P1, ROJO) == -1)
				error(2);
			if(CRUCE_pon_semAforo(SEM_P2, VERDE) == -1)
				error(2);				
			for(;;)  // Proceso que controla el ciclo semaforico
				ciclo_sem();
		default: 
			for(;;){
				if((creacion = CRUCE_nuevo_proceso()) == -1){
					pon_error("Error en la funcion CRUCE_nuevo_proceso");
					kill(PIDPADRE, SIGINT);
					kill(pidSem, SIGINT);
				}
				else if(creacion == COCHE)  //Es un coche
					esCoche(pidSem);
				else  //Es un peaton
					esPeaton(pidSem);
			}
	}
}

void liberar(){
	if(PIDPADRE == getpid()){
		if(CRUCE_fin() == -1){
			pon_error("Error en la función CRUCE_fin");
			exit(5);
		}
		if((semctl(d.semaforos,0,IPC_RMID,NULL) == -1)){
       	 	pon_error("Error al liberar los semaforos");
        		exit(5); 
		}
		if((msgctl(d.buzon,IPC_RMID,NULL)) == -1){
			pon_error("Error al liberar el buzon");
			exit(5);
		}
		if((shmctl(d.memoria, IPC_RMID,NULL)==-1)){
			pon_error("Error al liberar la memoria compartida");
			exit(5);
		}
		despedida();
	}
	exit(0);
}

void despedida(){
	system("clear");
	printf("\n\n\n");
	printf("\t+--------------------------------------------------+");
	printf("\n\t|               TRABAJO REALIZADO POR              |\n");
	printf("\t+--------------------------------------------------+");
	printf("\n\t|        ROBERTO MERCHAN GONZALEZ (i0909939)       |\n");
	printf("\t|        SERGIO  SANCHEZ GARCIA   (i0961594)       |\n");
	printf("\t+--------------------------------------------------+");
	printf("\n\n\n");
}

void error(int error){
	if(error == 1)
		pon_error("Error en la funcion semctl");
	else if(error == 2)
		pon_error("Error en la funcion CRUCE_pon_semAforo");
	else if(error == 3)
		pon_error("Error en semop");
	else if(error == 4)
		pon_error("Error en la funcion pausa");
	else if(error == 5)
		pon_error("Error en la funcion pausa_coche");
	else if(error == 6)
		pon_error("Error en la funcion msgrcv");
	else if(error == 7)
		pon_error("Error en la funcion msgsnd");
	kill(PIDPADRE, SIGINT);
	kill(getpid(), SIGINT);
}

void ponSem(int sem, int val){
	semf.val = val;
	if(semctl(d.semaforos, sem, SETVAL, semf) == -1)
		error(1);
}

void opSem(int sem, int op){
	struct sembuf sops;
	sops.sem_flg = 0;

	if(op == 1){
		sops.sem_num = sem;
		sops.sem_op = 1;
		if(semop(d.semaforos,&sops,1) == -1)
			error(3);
	}
	else if (op == -1){
		sops.sem_num = sem;
		sops.sem_op = -1;
		if(semop(d.semaforos,&sops,1) == -1)
			error(3);
	}
	else if(op == 0){
		sops.sem_num = sem;
		sops.sem_op = 0;
		if(semop(d.semaforos,&sops,1) == -1)
			error(3);
	}
	else
		pon_error("Error en funcion opSem");
}

void recibirMSG(struct posiciOn pos){
	mensaje mens;
	mens.tipo = 90000 +  pos.y + 100*pos.x;
	if(msgrcv(d.buzon,&mens,sizeof(mensaje)-sizeof(long),mens.tipo,MSG_NOERROR) == -1)
		error(6);
}

void enviarMSG(struct posiciOn pos){
	mensaje mens;
	mens.tipo = 90000 +  pos.y + 100*pos.x;
	if(msgsnd(d.buzon, &mens, sizeof(mensaje)-sizeof(long), 0) == -1)
		error(7);
}

void ciclo_sem(){
	// Variables
	int i;
	struct sembuf sops;
	
	// Amarillo C1
	opSem(1,-1);
	if(CRUCE_pon_semAforo(SEM_C1, AMARILLO) == -1)
		error(2);
	opSem(6,0);
	for(i = 0; i < 2; i++){
		if(pausa() == -1)
			error(4);
	}
	//SEGUNDA FASE
	if(CRUCE_pon_semAforo(SEM_C1, ROJO) == -1)
		error(2);
	opSem(4,-1);
	if(CRUCE_pon_semAforo(SEM_P2, ROJO) == -1)
		error(2);
	if(CRUCE_pon_semAforo(SEM_C2, VERDE) == -1)
		error(2);
	opSem(2,1);
	for(i = 0; i < 9; i++){
		if(pausa() == -1)
			error(4);
	}
	// Amarillo C2
	opSem(2,-1);
	if(CRUCE_pon_semAforo(SEM_C2, AMARILLO) == -1)
		error(2);
	opSem(6,0);
	for(i = 0; i < 2; i++){
		if(pausa() == -1)
			error(4);
	}
	//TERCERA FASE
	if(CRUCE_pon_semAforo(SEM_C2, ROJO) == -1)
		error(2);
	if(CRUCE_pon_semAforo(SEM_P1, VERDE) == -1)
		error(2);
	opSem(3,1);
	for(i = 0; i < 12; i++){
		if(pausa() == -1)
			error(4);
	}
	//PRIMERA FASE
	opSem(3,-1);
	if(CRUCE_pon_semAforo(SEM_P1, ROJO) == -1)
		error(2);
	if(CRUCE_pon_semAforo(SEM_C1, VERDE) == -1)
		error(2);
	opSem(1,1);
	if(CRUCE_pon_semAforo(SEM_P2, VERDE) == -1)
		error(2);
	opSem(4,1);
	for(i = 0; i < 6; i++){
		if(pausa() == -1)
			error(4);
	}
}

void esCoche(pid_t pidSem){
	struct posiciOn pos;
	struct sembuf sops;
	mensaje mens;
	int nacimientoV = 0, rojo = 0, entraCruce = 0;

	opSem(5,-1);  // Decrementamos S5 al crear un coche
	switch(fork()){
		case -1:
			pon_error("Error en la creacion de hijos");
        		kill(PIDPADRE, SIGINT);
        		exit(1);
			break;
		case 0:
			pos = CRUCE_inicio_coche();
			if(pos.x == 33 && pos.y == 1)
				nacimientoV = 1;
			for(;;){
				if(pos.x == 33 && pos.y == 6){ // C1 rojo
					opSem(1,-1);
					rojo = 1;
					opSem(6,1);
					opSem(8, 0);
					entraCruce = 1;
				}
				if(pos.x == 13 && pos.y == 10){ // C2 rojo
					opSem(2,-1);
					rojo = 2;
					opSem(6,1);
					opSem(8, 0);
					entraCruce = 1;
				}
				if(nacimientoV && !entraCruce){
					mens.tipo = 100 + pos.y + 6;
					if(msgrcv(d.buzon,&mens,sizeof(mensaje)-sizeof(long),mens.tipo,MSG_NOERROR) == -1)
						error(6);
				}
				if(!nacimientoV && !entraCruce){
					mens.tipo = 200 + pos.x + 8;
					if(msgrcv(d.buzon,&mens,sizeof(mensaje)-sizeof(long),mens.tipo,MSG_NOERROR) == -1)
						error(6);
				}
				pos = CRUCE_avanzar_coche(pos);
				if(pausa_coche() == -1)
					error(5);
				if(nacimientoV && pos.y > 7 && pos.y <= 12){
					mens.tipo = 100 + pos.y - 1;
					if(msgsnd(d.buzon, &mens, sizeof(mensaje)-sizeof(long), 0) == -1)
						error(7);
				}
				if(!nacimientoV && pos.x > 5 && pos.x <= 21){
					mens.tipo = 200 + pos.x - 2;
					if(msgsnd(d.buzon, &mens, sizeof(mensaje)-sizeof(long), 0) == -1)
						error(7);
				}
				if(pos.y < 0) // El coche termina
					break;
			}
			if(CRUCE_fin_coche() == -1){
				pon_error("Error en la funcion CRUCE_fin_coche");
				kill(PIDPADRE, SIGINT);
				kill(pidSem, SIGINT);
			}
			if(rojo == 1)
					opSem(1,1);
			if(rojo == 2)
					opSem(2,1);
			opSem(6,-1);  // Decrementamos S6 al salir del cruce
			opSem(5,1);  // Incrementamos S5 al salir del mapa
			kill(getpid(),SIGINT);
	}
}

void esPeaton(pid_t pidSem){
	struct posiciOn pos, pos_ac, pos_ant;
	struct sembuf sops;
	mensaje mens;
	int entraCruce1 = 0, entraCruce2 = 0, flagSem = 1;

	opSem(5,-1);  // Decrementamos S5 al crear un peaton
	switch(fork()){
		case -1:
			pon_error("Error en la creacion de hijos");
        	kill(PIDPADRE, SIGINT);
        	exit(1);
			break;
		case 0:
			opSem(7,-1);
			pos = CRUCE_inicio_peatOn_ext(&pos_ac);
			recibirMSG(pos_ac);
			for(;;){
				if(pos.x == 30 && (pos.y <= 15 && pos.y >= 13)){ // P1 rojo
					opSem(3,-1);
					entraCruce1 = 1;
					opSem(8, 1);
				}
				if(pos.y == 11 && (pos.x <= 28 && pos.x >= 22)){ // P2 rojo
					opSem(4,-1);
					entraCruce2 = 1;
					opSem(8, 1);
				}
				recibirMSG(pos);
				pos_ant = pos_ac;
				pos_ac = pos;
				pos = CRUCE_avanzar_peatOn(pos);
				if(!((pos_ac.x==0 && pos_ac.y>10) || (pos_ac.x<40 && pos_ac.y==16)) && flagSem){
					opSem(7,1);
					flagSem = 0;
				}
				enviarMSG(pos_ant);
				if(pausa() == -1)
					error(4);
				if(entraCruce1 && pos.x == 38){ // Vertical 
					opSem(8, -1);
					opSem(3,1);
					entraCruce1 = 0;
				}
				if(entraCruce2 && pos.y ==6){ // Horizontal 
					opSem(8, -1);
					opSem(4,1);
					entraCruce2 = 0;
				}
				if(pos.y < 0){ // El peaton termina
					enviarMSG(pos_ac);
					break;
				}
			}
			if(CRUCE_fin_peatOn() == -1){
				pon_error("Error en la funcion CRUCE_fin_peatOn");
				kill(PIDPADRE, SIGINT);
				kill(pidSem, SIGINT);
			}
			opSem(5,1);
			kill(getpid(),SIGINT);
	}
}
