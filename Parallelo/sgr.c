#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#define THRESHOLD 0.7		//soglia di soddisfazione degli elementi
#define S 100				//numero massimo di iterazioni
#define ROWS 10000			//numero di totale di righe della matrice
#define COLUMNS 10000		//numero di colonne della matrice
#define WHITE '1'
#define BLACK '2'
#define EMPTY '0'

void exchangeRow(int* sendFirst, int* recvFirst, int* sendLast, int* recvLast, int rank, int numtasks, MPI_Request* recvRequest);

int* push(int* stack[], int dimension, int* element, int* top);
int* pop(int* stack[], int* top);

int* pushQueue(int* queue[], int dimension, int* element, int* head, int* tail, int* numElements);
int* popQueue(int* queue[], int dimension, int* head, int* tail, int* numElements);

int readFile(char* filename);

int** temp_matrix;

int main() {
	int	numtasks, rank
		, rows, change, division 
		, rowstot
		, tag = 1, source = 0, destination	//variabili per le comunicazioni sulla suddivisione della matrice iniziale
		, iteration = 1	//contatore iterazioni
		, num_empty = 0, unhappy = 0//num_empty: contatore celle vuote e head dello stack "empty", unhappy = contatore celle insoddisfatte di tutta la sub_matrix 
		, num_near=0, num_equals = 0, num_diff = 0, amt_same, threshold = THRESHOLD	//variabili per il calcolo del grado di soddisfazione
		, k = 0	//k= head dello stack "irregular" per le celle insoddisfatte
		, head=0, tail=0//contengono gli indici per gestire la coda degli elementi vuoti
		, end=0, check=0;//variabili per la terminazione dei processi
	
	MPI_Init(NULL, NULL);
	MPI_Comm_size(MPI_COMM_WORLD, &numtasks);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);

	/*
	*@brief calcolo delle righe da inviare ad ogni proesso
	*/
	division = ROWS / numtasks;		//numero di righe per ogni processo
	change = ROWS % numtasks;		//conta le righe "superflue" da dividere ogni per processo
	rowstot = ROWS;	
	if (rank < change)
		rows = division + 1;		//I primi "change" processi avranno una riga in più
	else
		rows = division;
	
	/*
	*@brief allocazione della sub_matrix per ogni processo
	*/
	int** sub_matrix = (int**) malloc(rows*sizeof(int*));
	sub_matrix[0] = (int*)malloc(rows * COLUMNS * sizeof(int));
	int i, j;
	for (i = 1; i < rows; i++)
		sub_matrix[i] = sub_matrix[0] + i * COLUMNS;
	
	/*
	*@brief alloco le righe adiacenti inferiori e superiori che vengono inviate dagli altri processi
	*/
	int* row_pred = malloc(COLUMNS * sizeof(int));
	int* row_succ = malloc(COLUMNS * sizeof(int));

	/*
	*@brief allocazione stack e coda delle celle insoddisfatte (unhappy)
	*/
	int dim_pointers = (rows * COLUMNS);//la dimension dell'array puntatori è pari alla dimension della sub_matrix (caso peggiore = sub_matrix di solo spazi o con tutti irregolari e senza spazi)
	int** empty = malloc(dim_pointers * sizeof(int*));		//coda con puntatori alle celle vuote della sub_matrix
	int** irregular = malloc(dim_pointers * sizeof(int*));	//stack con puntatori alle celle irregolari della sub_matrix
	

	//tutti i processi utilizzano interamente recvRequest per lo scambio delle righe di confine tranne lo 0 e l'ultimo
	MPI_Request recvRequest[2];	//in [0] la ricezione della riga di confine dal predecessore, da [1] la ricezione del successore
	
	
	//inserire papi
	clock_t begin, end_time;

	MPI_Barrier(MPI_COMM_WORLD);

	/*
	*@brief CARICAMENTO ED INVIO DELLE SOTTOMATRICI
	*/
	if (rank == 0) {

		int load = readFile("board.txt");

		//se non trovo il file termino
		if(load == -1) return -1;

		//sostituire con papi
		begin = clock();
		int	i, j, num_msg = 0; 

		MPI_Request request[numtasks];
		destination = 0;
		printf("\nInizio la distribuzione della matrice\n");

		//creo la sub matrix per il processo rank = 0
		for (i = 0; i < rows; i++) {
			for (j = 0; j < COLUMNS; j++) {
				sub_matrix[i][j] = temp_matrix[i][j];
			}
		}
		//invio le righe ai processi con rank != 0
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
		printf("Invio concluso, procedo con la seconda fase del processo\n\n");
	}
	else {	//I processi non root ricevono le righe che gli spettano e le salvano nella sub_matrix
		MPI_Status stat;
		for (i = 0; i < rows; i++) {
			MPI_Recv(&sub_matrix[i][0], COLUMNS, MPI_INT, source, tag, MPI_COMM_WORLD, &stat);
		}
	}

	free(temp_matrix);//libero la memoria assegnata alla temp_matrix, poichè non più necessaria
	
	//scambio delle righe di confine con i processi vicini
	if(numtasks > 1)						
		exchangeRow(&sub_matrix[0][0], row_succ, &sub_matrix[rows - 1][0], row_pred, rank, numtasks, recvRequest);

	/*
	*@brief VERIFICA DELLA UNHAPPINESS DEGLI ELEMENTI
	*/

	//dichiaro i puntatori utilizzati per i cambi di posizione delle celle
	int* temp_irregular = (int*)malloc(sizeof(int));	
	int* temp_empty = (int*)malloc(sizeof(int));

	while (iteration <= S) {
		//scorri la sub_matrix alla ricerca di elementi insoddisfatti e tieni traccia di questi con lo stack di puntatori
		//tieni traccia con una coda circolare di puntatori anche delle caselle vuote della sub_matrix che verranno usate per eventuali spostamenti
		
		unhappy = 0;//ad ogni giro ricalcolo gli insoddisfatti della sub_matrix
		
		//controlla di avere i dati degli scambi delle righe di confine
		if (numtasks > 1) {
			if (rank == 0)  
				MPI_Wait(&recvRequest[1], MPI_STATUS_IGNORE);

			else if (rank != numtasks - 1)
				MPI_Waitall(2, recvRequest, MPI_STATUS_IGNORE);

			else	//ultimo processo
				MPI_Wait(&recvRequest[0], MPI_STATUS_IGNORE);
		}

		/*
		*@brief Ricerca degli elementi UNHAPPY
		*/
		for(i = 0; i < rows; i++){
			for(j = 0; j < COLUMNS; j++){

				//crea lo stack di spazi vuoti al primo giro (il valore viene poi gestito con gli scambi siccome gli spazi vuoti non cambiano di numero)
				if (sub_matrix[i][j] == 0 && iteration == 1) {	
					pushQueue(empty, dim_pointers, &sub_matrix[i][j], &head, &tail, &num_empty);
					
				}else if(sub_matrix[i][j] != 0){
					//tutti i processi tranne il rank 0 e l'ultimo processo hanno due  righe che gli sono state inviate dai processi adiacenti
					//il processo root ha solo la riga inferiore (row_succ)
					//l'ultimo processo ha solo la riga superiore (row_pred)
					if(rank == 0 && i == 0){
						//se sono il processo rank 0 e sono alla riga 0 non ho una riga superiore, quindi non posso controllare
						//le celle in alto
					}else{

						int * top_row;
						//se la riga è 0 e rank!=0 allora dobbiamo controllare la riga che ci è stata passata da un'altro processo
						if(i == 0 && rank != 0){
							top_row = row_pred;
						}else{ //altrimenti prendiamo la riga dalla nostra sub matrix
							top_row = sub_matrix[i - 1];
						}
						//posizione in alto
						if(top_row[j] != 0){
							num_near++;
							if(sub_matrix[i][j] != top_row[j]) num_diff++;
							else num_equals++;
						}
						//se non siamo nella prima colonna
						if(j != 0){
							//posizione in alto a sinistra
							if(top_row[j - 1] != 0){
								num_near++;
								if(sub_matrix[i][j] != top_row[j - 1]) num_diff++;
								else num_equals++;
							}
						}
						//se non siamo nell'ultima colonna
						if(j != COLUMNS-1){
							//posizione in alto a destra
							if(top_row[j + 1] != 0){
								num_near++;
								if(sub_matrix[i][j] != top_row[j + 1]) num_diff++;
								else num_equals++;
							}
						}
					}
						
					if(rank == numtasks - 1 && i == rows - 1){
						//se siamo l'ultimo processo e ci troviamo nella nostra ultima riga non possiamo controllare le posizioni inferiori
					}else{

						int* bottom_row;
						if(i == rows - 1 && rank != numtasks - 1){//se siamo nell'ultima riga della sub_matrix e non siamo l'ultimo processo
							bottom_row = row_succ; //prendo la riga che mi hanno passato da sotto
						}else { //altrimenti prendo la riga dalla mia sottomatrice
							bottom_row = sub_matrix[i + 1];
						}
						//posizione in basso
						if(bottom_row[j] != 0){
							num_near++;
							if(sub_matrix[i][j] != bottom_row[j + 1]) num_diff++;
							else num_equals++;
						}
						//se non ci troviamo nella prima colonna
						if(j != 0){
							//posizione in basso a sinistra
							if(bottom_row[j - 1] != 0){
								num_near++;
								if(sub_matrix[i][j] != bottom_row[j - 1]) num_diff++;
								else num_equals++;
							}
						}
						//se non ci troviamo nell'ultima colonna
						if(j != COLUMNS - 1){
							//posizione in basso a destra
							if(bottom_row[j + 1] != 0){
								num_near++;
								if(sub_matrix[i][j] != bottom_row[j + 1]) num_diff++;
								else num_equals++;
							}
						}
					}
					//posizione a sinistra
					if(j != 0){
						if(sub_matrix[i][j - 1] != 0){
							num_near++;
							if(sub_matrix[i][j] != sub_matrix[i][j - 1]) num_diff++;
							else num_equals++;
						}
					}
					//posizione a destra
					if(j != COLUMNS-1){
						if(sub_matrix[i][j + 1] != 0){
							num_near++;
							if(sub_matrix[i][j] != sub_matrix[i][j + 1]) num_diff++;
							else num_equals++;
						}
					}
				}
				// prima di passare alla cella successiva controlla i conflitti
				if (num_diff > 0) { //si é verificato almeno un conflitto con questa cella?
					amt_same = num_equals/(num_equals+num_diff); //calcolo fattore di unhappyness
					if (amt_same < threshold) {//se il valore è inferiore al threshold l'elemento non è felice
						push(irregular, dim_pointers, &sub_matrix[i][j], &k);
						unhappy++;
					}
					num_diff = 0;
					num_equals = 0;
				}
				num_near = 0;
			}
		}


		/*
		*@brief Condizione di terminazione dei processi
		*/
		//dopo aver setacciato tutta la sub_matrix: se non ci sono spazi vuoti, se ci sono solo spazi o se non ci sono elementi insoddisfatti, indica la volontà di terminare
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

		/*
		*@brief logica di sostituzione
		*/
		if (num_empty > 0) {//se nella sub_matrix sono presenti spazi vuoti
			while (k > 0) {	//finchè lo stack degli insoddisfatti non si svuota
				//cambia il valore indicizzato dal puntatore restituito da pop(empty) ciç che viene restituito da pop(irregular)
				temp_irregular = pop(irregular, &k);
				temp_empty = popQueue(empty, dim_pointers, &head, &tail, &num_empty);
				if (temp_irregular != NULL && temp_empty != NULL) {
					*temp_empty = *temp_irregular;					//riempi la casella vuota
					*temp_irregular = 0;							//libera la casella temp_irregular e falla puntare come nuova casella libera
					pushQueue(empty, dim_pointers, temp_irregular, &head, &tail, &num_empty);
				}
				else
					printf("puntatori nulli\n");
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

	/*
	*@brief verifica ottenimento soluzione ottima
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
	if (rank == 0) {
		if (iteration > S)
			printf("Limite di giri raggiunto. ");
		if (soluzioneGlob == numtasks)
			printf("La soluzione fornita dai processi e' ottima globalmente\n");
		else
			printf("La soluzione fornita dai processi non e' ottima globalmente\n");
	}
	
	MPI_Barrier(MPI_COMM_WORLD);
	//inserire papi
	end_time = clock();
	if (rank == 0) {
		//concludo la misurazione del tempo e stampo il risultato
		double time_spent = (double)(end_time - begin) / CLOCKS_PER_SEC;
		time_spent = time_spent * 1000;			//conversione in secondi
		printf("Il tempo impiegato dal programma (in millisecondi) e': %f\n\n", time_spent);
	}

	free(sub_matrix);
	free(row_pred);
	free(row_succ);
	free(empty);
	free(irregular);
	free(temp_irregular);
	free(temp_empty);

	MPI_Finalize();

	return 0;
}

/*
*@brief invio righe estremi della sotto matrice ai processi adiacenti
*@param sendFirst prima riga della sottomatrice da inviare al predecessore
*@param recvFirst prima riga della sottomatrice del successore
*@param sendLast ultima riga della sottomatrice da inviare al successore
*@param recvLast ultima riga della sottomatrice del predecessore
*@param rank del processo
*@param numtasks numero di processi
*@param recvRequest
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

/*
*@brief push aggiunge un elemento allo stack e lo restituisce se completa l'operazione
*@param stack, la pila
*@param dimension dimensione dello stack
*@param element da aggiugere allo stack
*@param top é l'indice della prima cella libera dello stack
*
*@return puntatore all'elemento inserito
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

/*
*@brief pop elimina un element dallo stack e lo restituisce se va a buon fine
*@param stack, la pila
*@param top  é l'indice della prima cella libera dello stack
*
*@return l'elemento estratto dallo stack
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

/*
*@brief pushQueue aggiunge l'elemento alla coda e restituisce un puntatore a questo se va a buon fine
*@param queue, la coda
*@param dimension dmensione della queue
*@param element elemento da inserire nella queue
*@param head puntatore alla testa della queue
*@param tail puntatore alla coda della queue
*@param numElements numero di elementi all'interno della queue
*
*@return il puntatore all'elemento inserito
*/
int* pushQueue(int* queue[], int dimension, int* element, int* head, int* tail, int* numElements){
	//head punta al primo element in coda, tail all'ultimo element inserito
	if(*numElements==dimension-1){
		printf("la coda e' piena!\n");
		return NULL;
	}
	else{
		if (*numElements == 0) {	//se la coda é vuota: head e tail combaciano e non li sposto
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

/*
*@brief popQueue preleva un element dalla testa della queue e lo restituisce
*@param queue, la coda
*@param dimension dmensione della queue
*@param head puntatore alla testa della queue
*@param tail puntatore alla coda della queue
*@param numElements numero di elementi all'interno della queue
*
*@return l'elemento estratto dalla coda
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
		else {		//se la coda ha un solo element non sposto il puntatore siccome il primo element da prelevare e l'ultimo inserito coincidono
			*numElements = *numElements - 1;
			return queue[*head];
		}
	}
	else {
		printf("La coda e' vuota\n");
		return NULL;
	}
	
}
/*
*@brief readFile legge il file contenente la matrice di partenza e alloca la matrice
*@param filename, nome del file
*@param temp_matrix, la board di partenza
*
*@return 1 se il caricamento ha avuto buon fine, altrimenti -1
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