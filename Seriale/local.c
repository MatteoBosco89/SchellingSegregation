/**
 * @file local.c
 * @author Team: Bosco, Cavoto, Ungolo
 * @brief Versione locale (testing)
 * @version 0.1
 * @date 2022-04-12
 *
 * @copyright Copyright (c) 2022
 *
 */

#include <stdio.h>
#include<stdlib.h>
#include<math.h>
#include<unistd.h>

#define ROW 10000	//numero di totale di righe della matrice per la strong, numero di righe per ogni processo per la weak
#define COL 10000	//numero di colonne della matrice
#define ITER 200    //numero di iterazioni
#define WHITE '1'   //carattere che rappresenta la prima parte della popolazione
#define BLACK '2'   //carattere che rappresenta la seconda parte della popolazione
#define EMPTY '0'   //carattere che rappresenta gli spazi vuoti nella board
#define THRESHOLD 0.3 //valore soglia per distinguere lo stato di felicità da quello di infelicità

int check_placement(char* board, int row, int col, int num_rows, int num_cols, double threshold, char type);
int count_unhappy(int*unhappy_spots, int num_chars);
int* check_agents(char*board,int* unhappy_spots,int rows, int cols, double threshold);
char* move_placement(char*board, int row, int col, int rows, int cols);
void print_board(char* board, int rows, int cols);

int* push(int* stack[], int dimension, int* element, int* top);
int* pop(int* stack[], int* top);

int* pushQueue(int* queue[], int dimension, int* element, int* head, int* tail, int* numElements);
int* popQueue(int* queue[], int dimension, int* head, int* tail, int* numElements);


int main(int argc, char** argv){

    FILE *fp;
    if((fp = fopen("board.txt", "r")) == NULL){
        printf("Impossibile aprire il file");
        return -1;
    }

    int** empty;		//coda con puntatori alle celle vuote della sub_matrix
    int** irregular;
    int head, tail, k;
    int dim_pointers;
    int num_empty;

    dim_pointers = ROW*COL;
    empty = malloc(dim_pointers * sizeof(int*));		//coda con puntatori alle celle vuote della sub_matrix
    irregular = malloc(dim_pointers * sizeof(int*));
    head = 0; tail = 0; k = 0;
    num_empty = 0;

    int rows = ROW, cols = COL;
    int num_iterations = 0;
    double threshold = THRESHOLD;


    char c;
    int temp;
    int i, j;

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

    //settiamo il vettore degli unhappy a -1

    /**
     * @brief Inizio Algoritmo
     *
     */

    int unhappy = 0;
    int optimal = 0;
    int* temp_irregular = (int*)malloc(sizeof(int));
    int* temp_empty = (int*)malloc(sizeof(int));

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
                    // prima di passare alla cella successiva controlla i conflitti
                    if (num_near > 0) {
                        //si é verificato almeno un conflitto con questa cella?
                        amt_same = num_equals/(num_equals+num_diff);
                        //calcolo fattore di unhappyness
                        //printf("amt_same %f\n", amt_same );
                        if (amt_same < threshold) {
                            //se il valore è inferiore al threshold l'elemento non è felice
                            push(irregular, dim_pointers, &board[i][j], &k);
                            unhappy++;

                        }

                    }
                    num_near = 0;
                    num_diff = 0;
                    num_equals = 0;
                }
            }
        }
        //printf("unhappy %d num_empty %d dim_pointers %d\n", unhappy, num_empty, dim_pointers-1);
        if (unhappy == 0 || num_empty == dim_pointers - 1 || num_empty == 0 ) {
            optimal = 1;
			break;
		}

        if (num_empty > 0) {//se nella board sono presenti spazi vuoti
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
        }else{
            k = 0;
        }

        num_iterations++;
        printf("%d\n", unhappy);
        //if(num_iterations % 50 == 0) printf("Iterazioni restanti: %d \n", num_iterations);
    }

    if(optimal != 1) printf("La soluzione trovata non e\' ottima\n\n");
    else printf("La soluzione trovata e\' ottima in %d iterazioni \n", num_iterations);

    free(irregular);
    free(empty);
    free(board);

    return 0;
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
