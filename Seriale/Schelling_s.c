/**
 * @file Schelling_s.c
 * @author Team: Bosco, Cavoto, Ungolo
 * @brief Versione seriale modello Schelling's Segregation
 * 
 * 
 */

#include <stdio.h>
#include<stdlib.h>
#include<math.h>
#include<unistd.h>
#include <papi.h>
#include <mpi.h>

#define THRESHOLD 0.5			//soglia di soddisfazione agenti (tolleranza)
#define ITER 200				//numero massimo di iterazioni
#define ROW 10000				//numero di totale di righe della matrice
#define COL 10000			    //numero di colonne della matrice
#define WHITE '1'               //carattere che rappresenta la prima parte della popolazione
#define BLACK '2'               //carattere che rappresenta la seconda parte della popolazione
#define EMPTY '0'               //carattere che rappresenta gli spazi vuoti nella board


/**
 * 
 * push: aggiunge un elemento allo stack e lo restituisce se completa l'operazione
 * pop: elimina un element dallo stack e lo restituisce se va a buon fine
 * pushQueue: aggiunge l'elemento alla coda e restituisce un puntatore a questo se va a buon fine
 * popQueue: preleva un elemento dalla testa della queue e lo restituisce
 *
 */

int* push(int* stack[], int dimension, int* element, int* top);
int* pop(int* stack[], int* top);

int* pushQueue(int* queue[], int dimension, int* element, int* head, int* tail, int* numElements);
int* popQueue(int* queue[], int dimension, int* head, int* tail, int* numElements);


int main(int argc, char** argv){

     //inizializzo la libreria PAPI
    if (PAPI_library_init(PAPI_VER_CURRENT) != PAPI_VER_CURRENT) {
        printf("Errore init PAPi\n");
        exit(1);
    }

    // inizializzazione file matrice
    FILE *fp;
    if((fp = fopen("board.txt", "r")) == NULL){
        printf("Impossibile aprire il file");
        return -1;
    }

    /**
	 * Variabili
	 * @param rows: numero di righe per processo
	 * @param num_iteration: contatore iterazioni
	 * @param num_empty: contatore elementi vuoti
	 * @param unhappy: contatore agenti insoddisfatti
	 * @param num_near: contatore agenti vicini
	 * @param count_same: contatore agenti uguali (stessa razza/colore)
	 * @param count_diff: contatore agenti differenti
	 * @param amt_same: calcolata come count_same/(count_same + count_diff) grado di soddisfazione
	 * @param threshold: soglia di soddisfazione (superata la soglia l'agente felice)
	 * @param k: iteratore stack unhappy_spots
	 * @param head, tail: indici coda di elementi vuoti
	 * @param papi_time_start/stop: tempo PAPI
	 * @param wallCLock_start/stop: tempo wallClock
     * @param dim_pointers: dimensione array puntatori
	 * @param empty: coda puntatori celle vuote
	 * @param unhappy_spots: stack puntatori agenti infelici
     * @param optimal: flag per controllo soluzione ottima
	 * 
	 */

    int** empty;		
    int** unhappy_spots;
    int head, tail, k;
    int dim_pointers;
    int num_empty;

    dim_pointers = ROW*COL;
    empty = malloc(dim_pointers * sizeof(int*));		
    unhappy_spots = malloc(dim_pointers * sizeof(int*));
    head = 0; tail = 0; k = 0;
    num_empty = 0;

    int rows = ROW, cols = COL;
    int num_iterations = 0;
    double threshold = THRESHOLD;
    int unhappy = 0;
    int optimal = 0;
    int* temp_unhappy_spots = (int*)malloc(sizeof(int));
    int* temp_empty = (int*)malloc(sizeof(int));
    char c;
    int temp;
    int i, j;
    long_long papi_time_start, papi_time_stop;
    double wallClock_start, wallClock_stop;

    /**
     * @brief lettura e allocazione matrice
     * 
     */

    int** board = (int**) malloc(rows*sizeof(int*));
	board[0] = (int*)malloc(rows * cols * sizeof(int));
    for (i = 1; i < rows; i++)
		board[i] = board[0] + i * cols;

    for(i = 0; i < rows; i++){
        for(j = 0; j < cols; j++){
            fscanf(fp, "%c", &c);

            if(c == EMPTY) temp = 0;
            else if( c == WHITE) temp = 1;
            else if( c == BLACK) temp = 2;

            board[i][j] = temp;
        }
        fscanf(fp, "%c", &c);
    }

    fclose(fp);


    /**
     * @brief Inizio Algoritmo
     *
     */

    wallClock_start = MPI_Wtime();
    papi_time_start = PAPI_get_real_usec();

    

    float num_diff = 0.0, num_equals = 0.0;
    int num_near = 0;
    double amt_same;
    while(num_iterations < ITER){
        unhappy = 0;
        for (i = 0; i < rows; i++){
            for (j = 0; j < cols; j++){
                if(board[i][j] == 0 && num_iterations == 0)
                    pushQueue(empty, dim_pointers, &board[i][j], &head, &tail, &num_empty);

                else if(board[i][j] != 0){

                    if(i != 0){
                        int * top_row;
                        top_row = board[i - 1];
                        //posizione in alto
                        if(top_row[j] != 0){
                            num_near++;
                            if(board[i][j] != top_row[j]) num_diff++;
                            else num_equals++;
                        }
                        //se non siamo nella prima colonna
                        if(j != 0){
                            //posizione in alto a sinistra
                            if(top_row[j - 1] != 0){
                                num_near++;
                                if(board[i][j] != top_row[j - 1]) num_diff++;
                                else num_equals++;
                            }
                        }
                        //se non siamo nell'ultima colonna
                        if(j != COL-1){
                            //posizione in alto a destra
                            if(top_row[j + 1] != 0){
                                num_near++;
                                if(board[i][j] != top_row[j + 1]) num_diff++;
                                else num_equals++;
                            }
                        }
                    }

                    if(i != ROW - 1){
                        int* bottom_row;
                        bottom_row = board[i + 1];

                        //posizione in basso
                        if(bottom_row[j] != 0){
                            num_near++;
                            if(board[i][j] != bottom_row[j + 1]) num_diff++;
                            else num_equals++;
                        }
                        //se non ci troviamo nella prima colonna
                        if(j != 0){
                            //posizione in basso a sinistra
                            if(bottom_row[j - 1] != 0){
                                num_near++;
                                if(board[i][j] != bottom_row[j - 1]) num_diff++;
                                else num_equals++;
                            }
                        }
                        //se non ci troviamo nell'ultima colonna
                        if(j != COL - 1){
                            //posizione in basso a destra
                            if(bottom_row[j + 1] != 0){
                                num_near++;
                                if(board[i][j] != bottom_row[j + 1]) num_diff++;
                                else num_equals++;
                            }
                        }
                    }
                    //posizione a sinistra
                    if(j != 0){
                        if(board[i][j - 1] != 0){
                            num_near++;
                            if(board[i][j] != board[i][j - 1]) num_diff++;
                            else num_equals++;
                        }
                    }
                    //posizione a destra
                    if(j != COL-1){
                        if(board[i][j + 1] != 0){
                            num_near++;
                            if(board[i][j] != board[i][j + 1]) num_diff++;
                            else num_equals++;
                        }
                    }
                    //printf("num_near %d num_equals %d num_diff %d\n",num_near, num_equals, num_diff);
                    if (num_near > 0) { // almeno un elmento adiacente, altrimenti agente happy sicuramente happy
                        amt_same = num_equals/(num_equals+num_diff); //calcolo fattore di unhappyness
                        if (amt_same < threshold) { // se il valore inferiore al threshold elemento non felice
                            push(unhappy_spots, dim_pointers, &board[i][j], &k);
                            unhappy++;

                        }

                    }
                    num_near = 0;
                    num_diff = 0;
                    num_equals = 0;
                }
            }
        }
        // controllo elementi unhappy, se almeno un solo unhappy soluzione non ottima
        if (unhappy == 0 || num_empty == dim_pointers - 1 || num_empty == 0 ) {
            // se soluzione ottima ferma il ciclo
            optimal = 1;
			break;
		}

        if (num_empty > 0) {//se nella board sono presenti spazi vuoti
            while (k > 0) {	//finche' lo stack degli infelici non si svuota
                //cambia il valore indicizzato dal puntatore restituito da pop(empty) quello che viene restituito da pop(unhappy_spots)
                temp_unhappy_spots = pop(unhappy_spots, &k);
                temp_empty = popQueue(empty, dim_pointers, &head, &tail, &num_empty);
                if (temp_unhappy_spots != NULL && temp_empty != NULL) {
                    *temp_empty = *temp_unhappy_spots;					//riempi la casella vuota
                    *temp_unhappy_spots = 0;							//libera la casella temp_unhappy_spots e falla puntare come nuova casella libera
                    pushQueue(empty, dim_pointers, temp_unhappy_spots, &head, &tail, &num_empty);
                }
                //else printf("puntatori nulli\n");
            }
        }else{
            k = 0;
        }

        num_iterations++;
        //printf("%d\n", unhappy);
        //if(num_iterations % 50 == 0) printf("Iterazioni restanti: %d \n", num_iterations);
    }
    wallClock_stop = MPI_Wtime();
    papi_time_stop = PAPI_get_real_usec();

    if(optimal != 1) printf("La soluzione trovata non e\' ottima\n\n");
    else printf("La soluzione trovata e\' ottima\n\n");

    printf ("Tempo di esecuzione (secondi): %f\n",wallClock_stop - wallClock_start);
    printf ("Tempo di esecuzione PAPI (microsecondi): %d\n",papi_time_stop - papi_time_start);

    free(unhappy_spots);
    free(empty);
    free(board);

    return 0;
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