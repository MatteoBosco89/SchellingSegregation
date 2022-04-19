#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#define THRESHOLD 0.7			//soglia di soddisfazione degli agenti
#define ITER 200			//numero massimo di giri
#define ROW 10000			//numero di totale di righe della matrice per la strong, numero di righe per ogni processo per la weak
#define COL 10000		//numero di colonne della matrice
#define OP "strong"			//inserire "strong" per strong scalability e "weak" per weak scalability

void exchangeRow(int* sendFirst, int* recvFirst, int* sendLast, int* recvLast, int rank, int numtasks, MPI_Request* recvRequest);

int* push(int* stack[], int dimensione, int* elemento, int* top);
int* pop(int* stack[], int* top);

int* pushQueue(int* queue[], int dimensione, int* elemento, int* head, int* tail, int* numElements);
int* popQueue(int* queue[], int dimensione, int* head, int* tail, int* numElements);


int main() {
	int	numtasks, rank
		, righe, resto, divisione													//variabili per la conformazione della matrice e sottomatrice quando si esegue il programma in modalit� diverse
		, righetot																	//le righe complessive della matrice, cambiano in base a weak o strong
		, tag = 1, sorgente = 0, destinatario										//variabili per le comunicazioni sulla suddivisione della matrice iniziale
		, numGiro = 1																//numGiro= numero di giro attuale
		, numVuoti = 0, numInsoddisfatti = 0										//numVuoti: contatore celle vuote e head dello stack "empty", numInsoddisfatti= contatore celle insoddisfatte di tutta la sottomatrice 
		, numVicini=0, uguali = 0, conflitti = 0, percentuale, sogliaConflitti = THRESHOLD	//per il calcolo della percentuale di ogni cella, conflitti tiene traccia dei conflitti generati in ogni cella (a differenza di numInsoddisfatti che tiene traccia del totale)
		, k = 0																		//k= head dello stack "irregular" per le celle insoddisfatte
		, head=0, tail=0															//contengono gli indici per gestire la coda degli elementi vuoti
		, termina=0, conferma=0;													//valori utili per la terminazione dei processi
	
	MPI_Init(NULL, NULL);
	MPI_Comm_size(MPI_COMM_WORLD, &numtasks);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	
	if (strcmp(OP, "strong") == 0) {
		divisione = ROW / numtasks;	//numero di righe per ogni processo
		resto = ROW % numtasks;		//conta le righe "superflue" da dividere ogni per processo
		righetot = ROW;				//nella strong le righe indicate sono quelle totali
		if (rank < resto)
			righe = divisione + 1;		//I primi "resto" processi avranno una riga in pi�
		else
			righe = divisione;
	}
	else if (strcmp(OP, "weak") == 0) {
		resto = 0;
		divisione = ROW;
		righetot = ROW * numtasks;	//nella weak le righe totali variano in base ai processi
		righe = ROW;
	}
	else {
		printf("operazione errata, termino\n");
		MPI_Finalize();
		return 0;
	}
	
	//alloco la sottomatrice per ogni processo
	int** sottomatrice = (int**) malloc(righe*sizeof(int*));
	sottomatrice[0] = (int*)malloc(righe * COL * sizeof(int));
	int i, j;
	for (i = 1; i < righe; i++)
		sottomatrice[i] = sottomatrice[0] + i * COL;
	
	//alloco la riga dei dati provenienti predecessore e dal successore
	int* rigaPred = malloc(COL * sizeof(int));
	int* rigaSucc = malloc(COL * sizeof(int));

	//stack e coda di puntatori per le celle vuote o insoddisfatte
	int dimPuntatori = (righe * COL);					//la dimensione dell'array puntatori � pari alla dimensione della sottomatrice (caso peggiore = sottomatrice di solo spazi o con tutti irregolari e senza spazi)
	int** empty = malloc(dimPuntatori * sizeof(int*));		//coda con puntatori alle celle vuote della sottomatrice
	int** irregular = malloc(dimPuntatori * sizeof(int*));	//stack con puntatori alle celle irregolari della sottomatrice
	
	//tutti i processi utilizzano interamente recvRequest per lo scambio delle righe di confine tranne lo 0 e l'ultimo
	MPI_Request recvRequest[2];								//in [0] la ricezione della riga di confine dal predecessore, da [1] la ricezione del successore
	
	//inizio la misurazione del tempo
	clock_t begin, end;
	MPI_Barrier(MPI_COMM_WORLD);
	begin = clock();

//LOGICA DI GENERAZIONE E DISTRIBUZIONE DELLA MATRICE
	if (rank == 0) {
		int buffer[COL];
		int	i, j, numMessaggi = 0; 
		//per numeri pseudocasuali differenti ad ogni esecuzione
		//time_t t;
		//srand((unsigned)time(&t));

		//numeri pseudocasuali uguali per ogni esecuzione, per i test
		srand((unsigned int)rank); 

		MPI_Request request[numtasks];
		destinatario = 0;
		printf("\nInizio la generazione e distribuzione della matrice\n");
		//Per evitare l'aggiunta di controlli ad ogni riga della matrice che viene generata:
		//utilizzo due for innestati: il primo riguarda la generazione dei dati che andranno processo 0 (senza send)
		//e l'altro con quelli che andranno anche inviati (con send al processo adatto, per ogni riga generata)
		for (i = 0; i < righe; i++) {
			for (j = 0; j < COL; j++) {
				buffer[j]= rand() % 3;
				sottomatrice[i][j] = buffer[j];
				//printf("[%d]", buffer[j]);
			}
			//printf("\n");
		}
		for (i = 0; i < righetot;i++) {
			for (j = 0; j < COL; j++) {
				buffer[j] = rand() % 3;
				//printf("[%d]", buffer[j]);
			}
			//printf("\n");
			//invia una riga in pi� al destinatario a cui spetta
			if (destinatario < resto) { 
				if (numMessaggi == divisione+1) {
					numMessaggi = 0;
					destinatario++;
				}
			}
			else {
				if (numMessaggi == divisione) {
					numMessaggi = 0;
					destinatario++;
				}
			}
			//ho provato ad effettuare il pack e l'unpack dei dati per ridurre il numero di send ma non comportavano un risparmio di operazioni...
			//siccome invece che una send e receive per ogni riga si faceva il pack e l'unpack di ogni riga + send e receive per ogni blocco di righe.
		
			MPI_Isend(&buffer, COL, MPI_INT, destinatario, tag, MPI_COMM_WORLD, &request[destinatario]);
			numMessaggi++;
		}
		printf("Generazione conclusa, procedo con la seconda fase del processo\n\n");
	}
	else {									//I processi non root ricevono le righe che gli spettano e le salvano nella sottomatrice
		MPI_Status stat;
		for (i = 0; i < righe; i++) {
			MPI_Recv(&sottomatrice[i][0], COL, MPI_INT, sorgente, tag, MPI_COMM_WORLD, &stat);
			//for (int j = 0; j < COLONNE; j++)
				//printf("{%d}[%d], ", rank, sottomatrice[i][j]);
		}
	}
	//printf("invio delle righe adiacenti\n");
	if(numtasks > 1)						//scambio delle righe di confine con i processi vicini
		exchangeRow(&sottomatrice[0][0], rigaSucc, &sottomatrice[righe - 1][0], rigaPred, rank, numtasks, recvRequest);
//	printf("Fine invio delle righe adiacenti rank %d\n", rank);
//LOGICA PER LA VERIFICA DELLA SODDISFAZIONE DEGLI AGENTI E DEI LORO SPOSTAMENTI
	//dichiaro i puntatori utilizzati per i cambi di posizione delle celle
	int* irregolare = (int*)malloc(sizeof(int));	
	int* vuoto = (int*)malloc(sizeof(int));

	while (numGiro <= ITER) {
		//scorri la sottomatrice alla ricerca di agenti insoddisfatti e tieni traccia di questi con lo stack di puntatori
		//tieni traccia con una coda circolare di puntatori anche delle caselle vuote della sottomatrice che verranno usate per eventuali spostamenti
		numInsoddisfatti = 0;		//ad ogni giro ricalcolo gli insoddisfatti della sottomatrice
		//if(numGiro % 10 == 0)printf("Rank %d, Iterazione %d\n", rank, numGiro);
		//printf("rank %d, iterazione %d\n", rank, numGiro);
		//controlla di avere i dati degli scambi degli array di confine
		if (numtasks > 1) {
			if (rank == 0)  
				MPI_Wait(&recvRequest[1], MPI_STATUS_IGNORE);

			else if (rank != numtasks - 1)
				MPI_Waitall(2, recvRequest, MPI_STATUS_IGNORE);

			else	//ultimo processo
				MPI_Wait(&recvRequest[0], MPI_STATUS_IGNORE);
		}

	//LOGICA DI CONTROLLO DEI CONFLITTI
		for(i = 0; i < righe; i++){
			for(j = 0; j < COL; j++){
				//crea lo stack di spazi vuoti al primo giro (il valore viene poi gestito con gli scambi siccome gli spazi vuoti non cambiano di numero)
				if (sottomatrice[i][j] == 0 && numGiro == 1) {	
					pushQueue(empty, dimPuntatori, &sottomatrice[i][j], &head, &tail, &numVuoti);
					//printf("spazio vuoto a [%d][%d] head(%d) tail(%d) rank{%d} vuoti[%d]\n", i, j, head, tail, rank,  numVuoti);
				}else if(sottomatrice[i][j] != 0){
					//tutti i processi tranne il rank 0 hanno una righa che gli � stata passata da qualcun'altro
					if(rank == 0 && i == 0){
					}else{
						int * top_row;
						if(i == 0 && rank != 0){
							top_row = rigaPred;
						}else{
							top_row = sottomatrice[i - 1];
						}
						//posizione in alto
						if(top_row[j] != 0){
							numVicini++;
							if(sottomatrice[i][j] != top_row[j]) conflitti++;
							else uguali++;
						}
						//se non siamo nella prima colonna
						if(j != 0){
							//posizione in alto a sinistra
							if(top_row[j - 1] != 0){
								numVicini++;
								if(sottomatrice[i][j] != top_row[j - 1]) conflitti++;
								else uguali++;
							}
						}

						if(j != COL - 1){
							//posizione in alto a destra
							if(top_row[j + 1] != 0){
								numVicini++;
								if(sottomatrice[i][j] != top_row[j + 1]) conflitti++;
								else uguali++;
							}
						}
					}
						
					if(rank == numtasks - 1 && i == righe - 1){

					}else{
						int* bottom_row;
						if(i == righe - 1 && rank != numtasks - 1){//se siamo nell'ultima riga della sottomatrice
							bottom_row = rigaSucc; //prendo la riga che mi hanno passato da sotto
						}else {
							bottom_row = sottomatrice[i + 1];
						}
						//posizione in basso
						if(bottom_row[j] != 0){
							numVicini++;
							if(sottomatrice[i][j] != bottom_row[j + 1]) conflitti++;
							else uguali++;
						}
						//posizione in basso a sinistra
						if(j != 0){
							if(bottom_row[j - 1] != 0){
								numVicini++;
								if(sottomatrice[i][j] != bottom_row[j - 1]) conflitti++;
								else uguali++;
							}
						}
						//posizione in basso a destra
						if(j != COL - 1){
							if(bottom_row[j + 1] != 0){
								numVicini++;
								if(sottomatrice[i][j] != bottom_row[j + 1]) conflitti++;
								else uguali++;
							}
						}
					}
						//posizione a sinistra
					if(j != 0){
						if(sottomatrice[i][j - 1] != 0){
							numVicini++;
							if(sottomatrice[i][j] != sottomatrice[i][j - 1]) conflitti++;
							else uguali++;
						}
					}
					//posizione a destra
					if(j != COL - 1){
						if(sottomatrice[i][j + 1] != 0){
							numVicini++;
							if(sottomatrice[i][j] != sottomatrice[i][j + 1]) conflitti++;
							else uguali++;
						}
					}
				}
				// prima di passare alla cella successiva controlla i conflitti
				if (conflitti > 0) { //si � verificato almeno un conflitto con questa cella
					percentuale = uguali/(uguali+conflitti);
					if (percentuale < sogliaConflitti) {
						//printf("\n-+{r:%d}%d[i:%d][j:%d](giro:%d)", rank, *push(irregular, dimPuntatori, &sottomatrice[i][j], &k), i, j, numGiro);
						push(irregular, dimPuntatori, &sottomatrice[i][j], &k);
						numInsoddisfatti++;
					}
					conflitti = 0;
					uguali = 0;
				}
				numVicini = 0;
			}
		}
//		printf("Sono uscito dal ciclo conflitti rank = %d\n ", rank);
	
		//dopo aver setacciato tutta la sottomatrice: se non ci sono spazi vuoti, se ci sono solo spazi o se non ci sono irregolarit�, indica la volont� di terminare
		if (numInsoddisfatti == 0 || numVuoti == dimPuntatori - 1|| numVuoti == 0 ) {
			if (numtasks == 1)
				break;
			else
				termina = 1;
		}
		else 
			termina = 0;
		//se tutti i processi vogliono terminare allora termina uscendo dal ciclo
//		printf("all reduce rank %d\n", rank);
		MPI_Allreduce(&termina, &conferma, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);
//		printf("fine all reduce rank %d\n", rank);
		if (conferma == numtasks)
			break;

	//LOGICA SOSTITUZIONI
		if (numVuoti > 0) {		//se nella sottomatrice sono presenti spazi vuoti
			while (k > 0) {		//finch� lo stack degli insoddisfatti non si svuota
			//cambia il valore indicizzato dal puntatore restituito da pop(empty) ci� che viene restituito da pop(irregular)
				irregolare = pop(irregular, &k);
				vuoto = popQueue(empty, dimPuntatori, &head, &tail, &numVuoti);
				if (irregolare != NULL && vuoto != NULL) {
					*vuoto = *irregolare;						//riempi la casella vuota
					*irregolare = 0;							//libera la casella irregolare e falla puntare come nuova casella libera
					pushQueue(empty, dimPuntatori, irregolare, &head, &tail, &numVuoti);
				}
				else
					printf("puntatori nulli\n");
			}
		}
		else // se non ci sono spazi vuoti non si possono fare scambi
			k = 0;	//per non far riempire lo stack degli insoddisfatti azzera il puntatore a head
		
		/*	test di stampa della sottomatrice al giro i-esimo
			printf("\nrank %d stampo la sottomatrice risultante al giro %d:\n", rank, numGiro);
			for (int i = 0; i < righe; i++) {
				for (int j = 0; j < dimensione; j++)
					printf("{%d}[%d] ", rank, sottomatrice[i][j]);
				printf("\n");
			}
			printf("\n");
		
		*/
		if(numtasks > 1)
			exchangeRow(&sottomatrice[0][0], rigaSucc, &sottomatrice[righe - 1][0], rigaPred, rank, numtasks, recvRequest);
//		printf("Barrier\n");	
		MPI_Barrier(MPI_COMM_WORLD);
//		printf("Fine Barrier\n");
		numGiro++;	

	}

	int soluzioneLoc = 0, soluzioneGlob = 0;
	
	if (numInsoddisfatti == 0) {
		//printf("\nprocesso %d, termino dopo aver risolto il problema\n", rank);
		soluzioneLoc = 1;
	}

	MPI_Reduce(&soluzioneLoc, &soluzioneGlob, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);
	
	if (rank == 0) {
		if (numGiro > S)
			printf("Limite di giri raggiunto. ");

		if (soluzioneGlob == numtasks)
			printf("La soluzione fornita dai processi e' ottima globalmente\n");
		else
			printf("La soluzione fornita dai processi non e' ottima globalmente\n");

		//printf("numero giri compiuti %d\n", numGiro-1);
	}
	
	MPI_Barrier(MPI_COMM_WORLD);
	end = clock();
	if (rank == 0) {
		//concludo la misurazione del tempo e stampo il risultato
		double time_spent = (double)(end - begin) / CLOCKS_PER_SEC;
		time_spent = time_spent * 1000;			//conversione in secondi
		printf("Il tempo impiegato dal programma (in millisecondi) e': %f\n\n", time_spent);
	}

	MPI_Finalize();
	return 0;
}


void exchangeRow(int* sendFirst, int* recvFirst, int* sendLast, int* recvLast, int rank, int numtasks, MPI_Request* recvRequest){
	int destinatario, sorgente, tagFirst = 2, tagLast=3;
	MPI_Request sendrequest;

	//tutti i processi (tranne lo 0) inviano al predecessore la loro prima riga e ricevono da questo la sua ultima riga
	if (rank != 0) {
		destinatario = rank - 1;		//invia la prima riga al predecessore
		MPI_Isend(sendFirst, COLONNE, MPI_INT, destinatario, tagFirst, MPI_COMM_WORLD, &sendrequest);

		sorgente = rank - 1;			//ricevi l'ultima riga del predecessore
		MPI_Irecv(recvLast, COLONNE, MPI_INT, sorgente, tagLast, MPI_COMM_WORLD, &recvRequest[0]);
	}

	//tutti i processi tranne l'ultimo inviano la propria ultima riga al successore e ricevono da quest'ultimo la sua prima riga
	if (rank != numtasks - 1) {
		destinatario = rank + 1;		//invia la sua ultima riga al successore
		MPI_Isend(sendLast, COLONNE, MPI_INT, destinatario, tagLast, MPI_COMM_WORLD, &sendrequest);

		sorgente = rank + 1;			//ricevi la prima riga del successore
		MPI_Irecv(recvFirst, COLONNE, MPI_INT, sorgente, tagFirst, MPI_COMM_WORLD, &recvRequest[1]);
	}
}

//push aggiunge un elemento allo stack e lo restituisce se completa l'operazione
//top  l'indice della prima cella libera dello stack
int* push(int* stack[], int dimensione, int* elemento, int* top){
	
	if (*top < dimensione) {
		stack[*top] = elemento;
		*top= *top+1;
		return stack[*top - 1];		//restituisci il puntatore all'elemento che ho appena aggiunto
	}
	else
		printf("Stack pieno!\n");
	return NULL;
}

//pop elimina un elemento dallo stack e lo restituisce se va a buon fine
int* pop(int* stack[], int* top){
	if (*top > 0) {
		*top = *top-1;
		return stack[*top];			//puntatore all'elemento che ho appena rimosso (top punta alla prima cella libera)
	}
	else
		printf("Lo stack e' vuoto\n");
	return NULL;
}

//pushQueue aggiunge l'elemento alla coda e restituisce un puntatore a questo se va a buon fine
int* pushQueue(int* queue[], int dimensione, int* elemento, int* head, int* tail, int* numElements){
	//head punta al primo elemento in coda, tail all'ultimo elemento inserito
	if(*numElements==dimensione-1){
		printf("la coda e' piena!\n");
		return NULL;
	}
	else{
		if (*numElements == 0) {	//se la coda � vuota: head e tail combaciano e non li sposto
			queue[*tail] = elemento;
			*numElements = *numElements + 1;
			return queue[*tail]; 
			
		}
		else{
			*tail = (*tail + 1) % dimensione;
			queue[*tail] = elemento;
			*numElements = *numElements + 1;
			
			if (*tail == 0)
				return queue[dimensione - 1];
			else
				return queue[*tail - 1];
		}
	}		
}

//popQueue preleva un elemento dalla testa della queue e lo restituisce
int* popQueue(int* queue[],int dimensione, int* head, int* tail, int* numElements){
	//head punta al primo elemento in coda, tail punta all'ultimo elemento aggiunto
	if (*numElements != 0) {
		if (*numElements != 1) {
			*head = (*head + 1) % dimensione;
			*numElements = *numElements - 1;
			if (*head == 0)
				return queue[dimensione - 1];
			else
				return queue[*head - 1];
		}
		else {		//se la coda ha un solo elemento non sposto il puntatore siccome il primo elemento da prelevare e l'ultimo inserito coincidono
			*numElements = *numElements - 1;
			return queue[*head];
		}
	}
	else {
		printf("La coda e' vuota\n");
		return NULL;
	}
	
}
