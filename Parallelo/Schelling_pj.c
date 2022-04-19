/**
 * @file Schelling_p.c
 * @author Team: Bosco, Cavoto, Ungolo
 * @brief Versione parallela modello Schelling's Segregation
 * 
 * 
 */
#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#define THRESHOLD 0.5			//soglia di soddisfazione agenti (tolleranza)
#define ITER 200				//numero massimo di iterazioni
#define ROWS 10000				//numero di totale di righe della matrice
#define COLUMNS 10000			//numero di colonne della matrice
#define WHITE '1'               //carattere che rappresenta la prima parte della popolazione
#define BLACK '2'               //carattere che rappresenta la seconda parte della popolazione
#define EMPTY '0'               //carattere che rappresenta gli spazi vuoti nella board
#define MASTER 0                // rank del MASTER


/**
 * 
 * exchangeRow: funzione scambio righe adiacenti tra processi
 * push: aggiunge un elemento allo stack e lo restituisce se completa l'operazione
 * pop: elimina un element dallo stack e lo restituisce se va a buon fine
 * pushQueue: aggiunge l'elemento alla coda e restituisce un puntatore a questo se va a buon fine
 * popQueue: preleva un elemento dalla testa della queue e lo restituisce
 * readFile: lettura matrice da file
 *
 */

void exchangeRow(int* sendFirst, int* recvFirst, int* sendLast, int* recvLast, int rank, int numtasks, MPI_Request* recvRequest);

int* push(int* stack[], int dimension, int* element, int* top);
int* pop(int* stack[], int* top);

int* pushQueue(int* queue[], int dimension, int* element, int* head, int* tail, int* numElements);
int* popQueue(int* queue[], int dimension, int* head, int* tail, int* numElements);

int readFile(char* filename);


/**
 * @param temp_matrix: matrice temporanea per lettura file 
 * 
 */
int** temp_matrix;

int main() {

	/**
	 * Variabili
	 * @param rows: numero di righe per processo
	 * @param change: resto divisione 
	 * @param division: quoziente (calcolo di righe per processo)
	 * @param rowstot: numero totale di righe
	 * @param tag: tag comunicazione Isend
	 * @param source: processo sorgente 
	 * @param destination: processo destinatario
	 * @param iteration: contatore iterazioni
	 * @param num_empty: contatore elementi vuoti
	 * @param unhappy: contatore agenti insoddisfatti
	 * @param num_near: contatore agenti vicini
	 * @param count_same: contatore agenti uguali (stessa razza/colore)
	 * @param count_diff: contatore agenti differenti
	 * @param amt_same: calcolata come count_same/(count_same + count_diff) grado di soddisfazione
	 * @param threshold: soglia di soddisfazione (superata la soglia l'agente felice)
	 * @param k: iteratore stack unhappy_spots
	 * @param head, tail: indici coda di elementi vuoti
	 * @param end, check: variabili per terminazione processi
	 * @param papi_time_start/stop: tempo PAPI
	 * @param wallCLock_start/stop: tempo wallClock
	 * 
	 */

	int	numtasks, rank, rows, change, division, rowstot, tag = 1, source = 0, destination;
	int iteration = 1, num_empty = 0, unhappy = 0, num_near=0, threshold = THRESHOLD;
	int k = 0, head=0, tail=0, end=0, check=0;
	//long_long papi_time_start, papi_time_stop;
    //double wallClock_start, wallClock_stop;
    double amt_same;
    float count_same = 0.0, count_diff = 0.0;
	
	/**
	 * @brief Inizializzazione MPI
	 * 
	 */
	MPI_Init(NULL, NULL);
	MPI_Comm_size(MPI_COMM_WORLD, &numtasks);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);

	/**
	 * @brief Inzializzazione PAPI
	 * 
	 */
    if (PAPI_library_init(PAPI_VER_CURRENT) != PAPI_VER_CURRENT) {
        printf("Errore init PAPi\n");
        exit(1);
    }

	/**
	* @brief calcolo delle righe da inviare ad ogni proesso
	*/
	division = ROWS / numtasks;		//numero di righe per ogni processo
	change = ROWS % numtasks;		//conta le righe "superflue" da dividere ogni per processo
	rowstot = ROWS;	
	if (rank < change)
		rows = division + 1;		//I primi "change" processi avranno una riga in più
	else
		rows = division;
	
	/**
	* @brief allocazione della sub_matrix per ogni processo
	*/
	int** sub_matrix = (int**) malloc(rows*sizeof(int*));
	sub_matrix[0] = (int*)malloc(rows * COLUMNS * sizeof(int));
	int i, j;
	for (i = 1; i < rows; i++)
		sub_matrix[i] = sub_matrix[0] + i * COLUMNS;
	
	/**
	* @brief alloco le righe adiacenti inferiori e superiori che vengono inviate dagli altri processi
	*/
	int* row_pred = malloc(COLUMNS * sizeof(int));
	int* row_succ = malloc(COLUMNS * sizeof(int));

	/**
	* @brief allocazione stack e coda delle celle insoddisfatte (unhappy)
	* @param dim_pointers: dimensione array puntatori
	* @param empty: coda puntatori celle vuote
	* @param unhappy_spots: stack puntatori agenti infelici
	*/
	int dim_pointers = (rows * COLUMNS);
	int** empty = malloc(dim_pointers * sizeof(int*));		
	int** unhappy_spots = malloc(dim_pointers * sizeof(int*));	
	

	MPI_Request recvRequest[2];	
	
	

	MPI_Barrier(MPI_COMM_WORLD);

	/**
	* @brief CARICAMENTO ED INVIO DELLE SOTTOMATRICI
	*/
	if (rank == MASTER) {

		int load = readFile("board.txt");

		//se non trovo il file termino
		if(load == -1) return -1;
		int	i, j, num_msg = 0; 
		MPI_Request request[numtasks];
		destination = 0;
		//printf("\nDistribuzione matrice\n");

		// creazione sottomatrice processo MASTER
		for (i = 0; i < rows; i++) {
			for (j = 0; j < COLUMNS; j++) {
				sub_matrix[i][j] = temp_matrix[i][j];
			}
		}
		// invio righe altri processi diversi da MASTER
		for (i=0; i < rowstot;i++) {
			if (destination < change) { 
				if (num_msg == division+1) {
					num_msg = 0;
					destination++;
				}
			}
			else {
				if (num_msg == division) {
					num_msg = 0;
					destination++;
				}
			}
		
			MPI_Isend(temp_matrix[i], COLUMNS, MPI_INT, destination, tag, MPI_COMM_WORLD, &request[destination]);
			num_msg++;

		}
		//printf("sottomatrici inviate\n\n");
	}else{	//I processi non MASTER ricevono le righe che gli spettano e le salvano nella sub_matrix
		MPI_Status stat;
		for (i = 0; i < rows; i++) {
			MPI_Recv(&sub_matrix[i][0], COLUMNS, MPI_INT, source, tag, MPI_COMM_WORLD, &stat);
		}
	}

	free(temp_matrix); 
	
	// i processi si scambiano le righe di confine
	if(numtasks > 1)						
		exchangeRow(&sub_matrix[0][0], row_succ, &sub_matrix[rows - 1][0], row_pred, rank, numtasks, recvRequest);

	/**
	* @brief VERIFICA DELLA UNHAPPINESS DEGLI ELEMENTI
	*/

	//puntatori utilizzati per i cambi di posizione delle celle
	int* temp_unhappy_spots = (int*)malloc(sizeof(int));	
	int* temp_empty = (int*)malloc(sizeof(int));

	// tempi
	//wallClock_start = MPI_Wtime();
    //papi_time_start = PAPI_get_real_usec();

	while (iteration < ITER) {
		//recupero agenti unhappy e inserimento nello stack
		//la coda mantiene le posizioni vuote per eventuali spostamenti
		//gli unhappy sono ricalcolati ad ogni iterazione
		unhappy = 0; 
		
		// controllo dati righe di confine
		if (numtasks > 1) {
			if (rank == MASTER)  
				MPI_Wait(&recvRequest[1], MPI_STATUS_IGNORE);

			else if (rank != numtasks - 1)
				MPI_Waitall(2, recvRequest, MPI_STATUS_IGNORE);

			else	//ultimo processo
				MPI_Wait(&recvRequest[0], MPI_STATUS_IGNORE);
		}

		/**
		* @brief Ricerca degli elementi UNHAPPY
		*/
		for(i = 0; i < rows; i++){
			for(j = 0; j < COLUMNS; j++){

				//crea lo stack di spazi vuoti al primo giro (il valore viene poi gestito con gli scambi siccome gli spazi vuoti non cambiano di numero)
				if (sub_matrix[i][j] == 0 && iteration == 0) {	
					pushQueue(empty, dim_pointers, &sub_matrix[i][j], &head, &tail, &num_empty);
					
				}else if(sub_matrix[i][j] != 0){
					//tutti i processi tranne il MASTER e l'ultimo processo hanno due righe aggiuntive inviate dai processi adiacenti
					// il processo MASTER ha solo la riga inferiore aggiuntiva (row_succ)
					// l'ultimo processo ha solo la riga superiore aggiuntiva (row_pred)
					if(rank == MASTER && i == 0){
						// NO OP
						// il processo MASTER non controlla la riga precedente 
					}else{
						int * top_row;
						//se la riga e' 0 e non MASTER controlla la riga precedente
						if(i == 0 && rank != MASTER){
							top_row = row_pred;
						}else{ //altrimenti prendiamo il processo controlla la riga della propria sub_matrix
							top_row = sub_matrix[i - 1];
						}
						//posizione in alto
						if(top_row[j] != 0){
							num_near++;
							if(sub_matrix[i][j] != top_row[j]) count_diff++;
							else count_same++;
						}
						//se non siamo nella prima colonna
						if(j != 0){
							//posizione in alto a sinistra
							if(top_row[j - 1] != 0){
								num_near++;
								if(sub_matrix[i][j] != top_row[j - 1]) count_diff++;
								else count_same++;
							}
						}
						//se non siamo nell'ultima colonna
						if(j != COLUMNS-1){
							//posizione in alto a destra
							if(top_row[j + 1] != 0){
								num_near++;
								if(sub_matrix[i][j] != top_row[j + 1]) count_diff++;
								else count_same++;
							}
						}
					}
						
					if(rank == numtasks - 1 && i == rows - 1){
						// ultimo processo non controlla riga successiva
					}else{

						int* bottom_row;
						// ultima riga sub_matrix e non ultimo processo
						if(i == rows - 1 && rank != numtasks - 1){
							bottom_row = row_succ; // riga successiva (passata da processo successivo)ì
						}else { // riga sub_matrix altrimenti
							bottom_row = sub_matrix[i + 1];
						}
						//posizione in basso
						if(bottom_row[j] != 0){
							num_near++;
							if(sub_matrix[i][j] != bottom_row[j + 1]) count_diff++;
							else count_same++;
						}
						//se non ci troviamo nella prima colonna
						if(j != 0){
							//posizione in basso a sinistra
							if(bottom_row[j - 1] != 0){
								num_near++;
								if(sub_matrix[i][j] != bottom_row[j - 1]) count_diff++;
								else count_same++;
							}
						}
						//se non ci troviamo nell'ultima colonna
						if(j != COLUMNS - 1){
							//posizione in basso a destra
							if(bottom_row[j + 1] != 0){
								num_near++;
								if(sub_matrix[i][j] != bottom_row[j + 1]) count_diff++;
								else count_same++;
							}
						}
					}
					//posizione a sinistra
					if(j != 0){
						if(sub_matrix[i][j - 1] != 0){
							num_near++;
							if(sub_matrix[i][j] != sub_matrix[i][j - 1]) count_diff++;
							else count_same++;
						}
					}
					//posizione a destra
					if(j != COLUMNS-1){
						if(sub_matrix[i][j + 1] != 0){
							num_near++;
							if(sub_matrix[i][j] != sub_matrix[i][j + 1]) count_diff++;
							else count_same++;
						}
					}
				}
				// controllo happyness
				if (num_near > 0) { // almeno un elmento adiacente, altrimenti agente sicuramente happy
					amt_same = count_same/(count_same + count_diff); // calcolo fattore di unhappyness
					if (amt_same < threshold) { // se il valore inferiore al threshold elemento non felice
						push(unhappy_spots, dim_pointers, &sub_matrix[i][j], &k);
						unhappy++;
					}
					count_diff = 0;
					count_same = 0;
				}
				num_near = 0;
			}
		}


		/**
		* @brief Condizione di terminazione dei processi
		*/
		//dopo aver setacciato tutta la sub_matrix: se non ci sono spazi vuoti, se ci sono solo spazi o se non ci sono elementi infelici, termina
		if (unhappy == 0 || num_empty == dim_pointers - 1|| num_empty == 0 ) {
			if (numtasks == 1)
				break;
			else
				end = 1;
		}
		else 
			end = 0;
		//se tutti i processi vogliono terminare allora termina uscendo dal ciclo

		MPI_Allreduce(&end, &check, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);

		if (check == numtasks)
			break;

		/**
		* @brief logica di sostituzione
		*/
		if (num_empty > 0) {//se nella sub_matrix sono presenti spazi vuoti
			while (k > 0) {	//finche' lo stack degli infelici non si svuota
				//cambia il valore indicizzato dal puntatore restituito da pop(empty) quello che viene restituito da pop(unhappy_spots)
				temp_unhappy_spots = pop(unhappy_spots, &k);
				temp_empty = popQueue(empty, dim_pointers, &head, &tail, &num_empty);
				if (temp_unhappy_spots != NULL && temp_empty != NULL) {
					*temp_empty = *temp_unhappy_spots;					//riempi la casella vuota
					*temp_unhappy_spots = 0;							//libera la casella temp_unhappy_spots e falla puntare come nuova casella libera
					pushQueue(empty, dim_pointers, temp_unhappy_spots, &head, &tail, &num_empty);
				}
				// else printf("puntatori nulli\n");
			}
		}
		else // se non ci sono spazi vuoti non si possono fare scambi
			k = 0;	//per non far riempire lo stack degli insoddisfatti azzera il puntatore a head

		//reinviamo le righe adiacenti aggiornate
		if(numtasks > 1)
			exchangeRow(&sub_matrix[0][0], row_succ, &sub_matrix[rows - 1][0], row_pred, rank, numtasks, recvRequest);
	
		MPI_Barrier(MPI_COMM_WORLD);

		iteration++;	
	}

	/**
	* @brief verifica ottenimento soluzione ottima
	*/
	int soluzioneLoc = 0, soluzioneGlob = 0;
	
	//ogni processo se non ha elementi unhappy setta la soluzioneLoc a 1
	if (unhappy == 0) {
		soluzioneLoc = 1;
	}
	//sommiamo tutti i valori di soluzioneLoc in soluzioneGlob
	MPI_Reduce(&soluzioneLoc, &soluzioneGlob, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);
	//Se il valore di soluzioneGlob == numtasks allora tutti i processi hanno ottenuto una soluzione ottima localmente e quindi abbiamo trovato la 
	//soluzione ottima globalmente
	
	
	MPI_Barrier(MPI_COMM_WORLD);
	//wallClock_stop = MPI_Wtime();
    //papi_time_stop = PAPI_get_real_usec();
	if (rank == MASTER) {
		if (soluzioneGlob == numtasks)
			printf("La soluzione trovata e\' ottima\n");
		else
			printf("La soluzione trovata non e\' ottima\n\n");
		//printf ("Tempo di esecuzione (secondi): %f\n", wallClock_stop - wallClock_start);
    	//printf ("Tempo di esecuzione PAPI (microsecondi): %d\n", papi_time_stop - papi_time_start);
	}

	free(sub_matrix);
	free(row_pred);
	free(row_succ);
	free(empty);
	free(unhappy_spots);
	free(temp_unhappy_spots);
	free(temp_empty);

	MPI_Finalize();

	return 0;
}

/**
* @brief invio righe estremi della sotto matrice ai processi adiacenti
* @param sendFirst prima riga della sottomatrice da inviare al predecessore
* @param recvFirst prima riga della sottomatrice del successore
* @param sendLast ultima riga della sottomatrice da inviare al successore
* @param recvLast ultima riga della sottomatrice del predecessore
* @param rank del processo
* @param numtasks numero di processi
* @param recvRequest
*/
void exchangeRow(int* sendFirst, int* recvFirst, int* sendLast, int* recvLast, int rank, int numtasks, MPI_Request* recvRequest){
	int destination, source, tagFirst = 2, tagLast=3;
	MPI_Request sendrequest;

	//tutti i processi (tranne lo 0) inviano al predecessore la loro prima riga e ricevono da questo la sua ultima riga
	if (rank != 0) {
		destination = rank - 1;		//invia la prima riga al predecessore
		MPI_Isend(sendFirst, COLUMNS, MPI_INT, destination, tagFirst, MPI_COMM_WORLD, &sendrequest);

		source = rank - 1;			//ricevi l'ultima riga del predecessore
		MPI_Irecv(recvLast, COLUMNS, MPI_INT, source, tagLast, MPI_COMM_WORLD, &recvRequest[0]);
	}

	//tutti i processi tranne l'ultimo inviano la propria ultima riga al successore e ricevono da quest'ultimo la sua prima riga
	if (rank != numtasks - 1) {
		destination = rank + 1;		//invia la sua ultima riga al successore
		MPI_Isend(sendLast, COLUMNS, MPI_INT, destination, tagLast, MPI_COMM_WORLD, &sendrequest);

		source = rank + 1;			//ricevi la prima riga del successore
		MPI_Irecv(recvFirst, COLUMNS, MPI_INT, source, tagFirst, MPI_COMM_WORLD, &recvRequest[1]);
	}
}

/**
* @brief push aggiunge un elemento allo stack e lo restituisce se completa l'operazione
* @param stack
* @param dimension dimensione dello stack
* @param element da aggiugere allo stack
* @param top indice della prima cella libera dello stack
*
* @return puntatore all'elemento inserito
*/
int* push(int* stack[], int dimension, int* element, int* top){
	
	if (*top < dimension) {
		stack[*top] = element;
		*top= *top+1;
		return stack[*top - 1];	//restituisci il puntatore all'elemento che ho appena aggiunto
	}
	else
		printf("Stack pieno!\n");
	return NULL;
}

/**
* @brief pop elimina un elemento dallo stack e lo restituisce se va a buon fine
* @param stack
* @param top indice della prima cella libera dello stack
*
* @return l'elemento estratto dallo stack
*/
int* pop(int* stack[], int* top){
	if (*top > 0) {
		*top = *top-1;
		return stack[*top];			//puntatore all'element che ho appena rimosso (top punta alla prima cella libera)
	}
	else
		printf("Lo stack e' temp_empty\n");
	return NULL;
}

/**
* @brief pushQueue aggiunge l'elemento alla coda e restituisce un puntatore a questo se va a buon fine
* @param queue
* @param dimension dmensione della queue
* @param element elemento da inserire nella queue
* @param head puntatore alla testa della queue
* @param tail puntatore alla coda della queue
* @param numElements numero di elementi all'interno della queue
*
* @return il puntatore all'elemento inserito
*/
int* pushQueue(int* queue[], int dimension, int* element, int* head, int* tail, int* numElements){
	//head punta al primo element in coda, tail all'ultimo elemento inserito
	if(*numElements==dimension-1){
		printf("la coda e' piena!\n");
		return NULL;
	}
	else{
		if (*numElements == 0) {	
			queue[*tail] = element;
			*numElements = *numElements + 1;
			return queue[*tail]; 
			
		}
		else{
			*tail = (*tail + 1) % dimension;
			queue[*tail] = element;
			*numElements = *numElements + 1;
			
			if (*tail == 0)
				return queue[dimension - 1];
			else
				return queue[*tail - 1];
		}
	}		
}

/**
* @brief popQueue preleva un elemento dalla testa della queue e lo restituisce
* @param queue
* @param dimension dmensione della queue
* @param head puntatore alla testa della queue
* @param tail puntatore alla coda della queue
* @param numElements numero di elementi all'interno della queue
*
* @return l'elemento estratto dalla coda
*/
int* popQueue(int* queue[],int dimension, int* head, int* tail, int* numElements){
	//head punta al primo element in coda, tail punta all'ultimo element aggiunto
	if (*numElements != 0) {
		if (*numElements != 1) {
			*head = (*head + 1) % dimension;
			*numElements = *numElements - 1;
			if (*head == 0)
				return queue[dimension - 1];
			else
				return queue[*head - 1];
		}
		else {
			*numElements = *numElements - 1;
			return queue[*head];
		}
	}
	else {
		printf("La coda e' vuota\n");
		return NULL;
	}
	
}

/**
* @brief readFile legge il file contenente la matrice di partenza e alloca la matrice
* @param filename, nome del file
* @param temp_matrix, la board di partenza
*
* @return 1 se il caricamento ha avuto buon fine, altrimenti -1
*/
int readFile(char* filename){
	int i, j;
	FILE *file;
	printf("carico file\n");

	temp_matrix = (int **)malloc(ROWS*sizeof(int*));
	temp_matrix[0] = (int *)malloc(ROWS*COLUMNS*sizeof(int));

	for(i = 1; i < ROWS; i++){
		temp_matrix[i] = temp_matrix[0] + i * COLUMNS;
	}

	file = fopen("board.txt", "r");
	if(file == NULL){
		printf("File non trovato");
		return -1;
	}
	char c;
	int temp;
	
	for(i = 0; i < ROWS; i++){
		for(j = 0; j < COLUMNS; j++){
			fscanf(file, "%c", &c);
			if(c == EMPTY) temp = 0;
			if(c == WHITE) temp = 1;
			else temp = 2;
			temp_matrix[i][j] = temp;
		}
		fscanf(file, "%c", &c);
	}
	fclose(file);
	return 1;
}