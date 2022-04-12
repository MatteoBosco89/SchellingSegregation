#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>
#include <papi.h>
#include <mpi.h>

#define ROW 10000	//numero di totale di righe della matrice per la strong, numero di righe per ogni processo per la weak
#define COL 10000	//numero di colonne della matrice
#define ITER 200    //numero di iterazioni 
#define WHITE '1'   //carattere che rappresenta la prima parte della popolazione
#define BLACK '2'   //carattere che rappresenta la seconda parte della popolazione
#define EMPTY '0'   //carattere che rappresenta gli spazi vuoti nella board
#define THRESHOLD 0.0001 //valore soglia per distinguere lo stato di felicità da quello di infelicità

int check_placement(char* board, int row, int col, int num_rows, int num_cols, double threshold, char type);
int count_unhappy(int*unhappy_spots, int num_chars);
int* check_agents(char*board,int* unhappy_spots,int rows, int cols, double threshold);
char* move_placement(char*board, int row, int col, int rows, int cols);
void print_board(char* board, int rows, int cols);



int main(int argc, char** argv){   
    //inizializzo la libreria PAPI
    if (PAPI_library_init(PAPI_VER_CURRENT) != PAPI_VER_CURRENT) {
        printf("Errore init PAPi\n");
        exit(1);
    }

    FILE *fp;
    if((fp = fopen("board.txt", "r")) == NULL){
        printf("Impossibile aprire il file");
        return -1;
    }

    int rows = ROW, cols = COL;
    int num_iterations = ITER;
    double threshold = THRESHOLD;
    int num_d = 0, num_p = 0;
    char c;
    int i, j;
    long_long papi_time_start, papi_time_stop;
    double wallClock_start, wallClock_stop;

    char* board = (char*) malloc(rows*cols * sizeof(char));

    for(i = 0; i < rows; i++){
        for(j = 0; j < cols; j++){
            fscanf(fp, "%c", &c);

            if(c == EMPTY) c = ' ';
            else if( c == WHITE) num_p++;
            else if( c == BLACK) num_d++;

            board[i*cols+j] = c;
        }
        fscanf(fp, "%c", &c);
    }
    //print_board(board, rows, cols);
    fclose(fp);
    //allochiamo un vettore per memorizzare la posizione nella board degli elementi non felici
    int* unhappy_spots = (int*) malloc((num_p + num_d)*sizeof(int));

    int current_row;
    int current_col;

    //settiamo il vettore degli unhappy a -1
    for (j=0; j < num_p + num_d; j++){
        unhappy_spots[j]=-1;
    }


    wallClock_start = MPI_Wtime();
    papi_time_start = PAPI_get_real_usec();

    // ALGORITMO
    unhappy_spots=check_agents(board, unhappy_spots, rows, cols, threshold);
    // per verificare che l'algoritmo termini verifichiamo che non ci siano unhappy all'interno del vettore unhappy_spots e che non vengano 
    // superate le iterazioni massime previste.
    // Poichè il vettore viene ad ogni iterazione resettato a -1 e ripopolato, se il primo elemento del vettore resta -1 significa
    // che non sono stati trovati elementi unhappy e quindi abbiamo raggiunto la soluzione ottima
    while(unhappy_spots[0] != -1 && num_iterations > 0){
        for(i = 0; i < num_p + num_d; i++){
            if(unhappy_spots[i]!=-1){
                current_col = unhappy_spots[i]%cols;
                current_row = (unhappy_spots[i]/cols);
                board = move_placement(board, current_row, current_col, rows, cols);
            }
        }
        // inizializzo l'array di elementi non felici a -1 (tutti felici)
        for (j=0; j < num_p + num_d; j++){
            unhappy_spots[j] = -1;
        }
        // carico l'array di elementi non felici con la loro posizione nella board
        unhappy_spots=check_agents(board,unhappy_spots,rows,cols, threshold);
        // decremento il numero di iterazione dopo un check completo
        num_iterations--;
        //if(num_iterations % 50 == 0) printf("It restanti: %d \n", num_iterations);
    }

    wallClock_stop = MPI_Wtime();
    papi_time_stop = PAPI_get_real_usec();
    
    // se unhappy_spots[0] != -1 significa che ha trovato almeno un elemento non felice
    // la soluzione quindi non risulta ottima
    if(unhappy_spots[0] != -1) printf("La soluzione trovata non e\' ottima\n\n");
    else printf("La soluzione trovata e\' ottima, iterazioni restanti = %d \n", num_iterations);
    printf("Elementi non felici: %d \n", count_unhappy(unhappy_spots, num_d + num_p));

    free(unhappy_spots);
    free(board);
    unhappy_spots=NULL;
    board = NULL;

    printf ("Tempo di esecuzione (secondi): %f\n",wallClock_stop - wallClock_start);
    printf ("Tempo di esecuzione PAPI (microsecondi): %d\n",papi_time_stop - papi_time_start);
    return 0;
}

/* Determina se l'elemento nella matrice è unhappy o happy
*
* Parametri:
*  board: la matrice di tutti gli elementi
*  row: la riga che contiene l'elemento
*  col: la colonna che contiene l'elemento
*  rows: numero di righe della board
*  cols: numero di colonne della board
*  threshold: soglia per determinare se l'elemento è happy o unhappy
*  type: il tipo di elemento preso in considerazione (WHITE, BLACK o EMPTY)
*
* Ritorna:
*  1 se l'elemento risulta felice
*  0 altrimenti
*/
int check_placement(char* board, int row, int col, int rows, int cols, double threshold, char type){
    //se il type del carattere è lo spazio vuoto allora sarà sicuramente felice
    if (type == ' '){return 1;}
    float count_same=0.0;
    float count_different=0.0;

    //inizializziamo le variabili per prendere le 8 posizioni confinanti con l'elemento considerato
    int right = row*cols+col+1;
    int left = row*cols+col-1;
    int bottom = (row+1)*cols+col;
    int top = (row-1)*cols+col;
    int top_right = (row-1)*cols+col+1;
    int top_left = (row-1)*cols+col-1;
    int bottom_right = (row+1)*cols+col+1;
    int bottom_left = (row+1)*cols+col-1;

    //Controlliamo la riga superiore
    if(row!=0){
        //elemento sopra
        if(board[top]!=' '){
            if(board[top]==type){count_same++;}
            else{count_different++;}
        }
        //controlliamo la colonna a sinistra
        if(col!=0){
        //controlliamo l'elemento in alto a sinistra
            if(board[top_left]!=' '){
                if(board[top_left]==type){count_same++;}
                else{count_different++;}
            }
        }
        //controlliamo la colonna a destra
        if(col!=cols-1){
        //controlliamo l'elemento in alto a destra
            if(board[top_right]!=' '){
                if(board[top_right]==type){count_same++;}
                else{count_different++;}
            }
        }
    }
    //controlliamo la riga sotto
    if(row!=rows-1){
    //controlliamo l'elemento sotto
        if(board[bottom]!=' '){
            if(board[bottom]==type){count_same++;}
            else{count_different++;}
        }
        //controlliamo la colonna a sinistra
        if(col!=0){
        //controlliamo l'elemento in basso a sinistra
            if(board[bottom_left]!=' '){
                if(board[bottom_left]==type){count_same++;}
                else{count_different++;}
            }
        }
        //controlliamo la colonna a destra
        if(col!=cols-1){
        //controlliamo l'elemento in basso a destra
            if(board[bottom_right]!=' '){
                if(board[bottom_right]==type){count_same++;}
                else{count_different++;}
            }
        }
    }
    //controlliamo gli elementi a destra e sinistra
    if(col!=0){
    //controlliamo a sinistra
        if(board[left]!=' '){
            if (board[left]==type){count_same++;}
            else{count_different++;}
        }
    }
    if(col!=cols-1){
    //controlliamo a destra
        if (board[right]!=' '){
            if (board[right]==type){count_same++;}
            else{count_different++;}
        }
    }
    //per verificare se un elemento è felice oppure no andiamo a contare gli elementi adiacenti che sono simili e diversi
    //e calcoliamo il rapporto (uguali/(uguali+diversi))->(count_same/(count_same+count_different))
    //tale valore vinene confrontato con il threshold. se è inferiore allora l'elemento è unhappy, altrimenti è happy
    double amt_same;
    amt_same = count_same/(count_same+count_different);
    if(count_same + count_different == 0) return 1;
    if(amt_same<threshold){return 0;}
    else{return 1;}
}

/* Conta il numero di elementi unhappy rimasti nella board
*
* Parametri:
*  unhappy_spots: array contenente le posizioni degli elementi unhappy all'interno della board
*  num_chars: dimensione del array unhappy_spots
*
*  Ritorna:
*   il numero di elementi unhappy presenti nella board
*/
int count_unhappy(int*unhappy_spots, int num_chars){
    int i;
    int j = 0;
    for(i=0;i<num_chars;i++){
        if(unhappy_spots[i] != -1){
            j++;
        }
    }
    return j;
}

/*
*Verifica se all'interno della board ci sono elementi unhappy
*
*Parametri:
* board: matrice che contiene lo stato del sistema
* rows: numero di righe della board
* cols: numero di colonne della board
*
* Ritorna:
* unhappy_spots, vettore contenente le posizioni all'interno della board degli elementi unhappy.
*/
int* check_agents(char*board, int* unhappy_spots,int rows, int cols, double threshold){
    char type;
    int index=0, row, col;
    int happiness;
    for (row=0; row<rows; row++){
        for (col=0; col<cols; col++){
            type = board[row*cols + col];
            happiness = check_placement(board, row, col, rows, cols, threshold, type);
            if (happiness == 0){
                unhappy_spots[index]=row*cols + col;
                index++;
            }
        }
    }
    return unhappy_spots;
}

/* Sposta un elemento non felice nel più vicino spazio libero 
*
*Parametri:
* board: matrice contenente lo stato del sistema
* int row: riga contenente l'elemento che si vuole spostare
* int col: colonna contenente l'elemento che si vuole spostare
* rows: numero di righe della board
* cols: numero di colonne della board
*
* Ritorna:
* la board aggiornata
*/
char* move_placement(char*board, int row, int col, int rows, int cols){
    int current_position = row*cols+col;
    //fin tanto che la posizione corrente non ha raggiunto la fine della board
    while (current_position < rows*cols-1){
        current_position++;
        if (board[current_position] == ' '){
            board[current_position]=board[row*cols+col];
            board[row*cols+col]=' ';
            return board;
        }
    }
    //se la posizione ha raggiunto la fine della board senza trovare posto, torna
    //all'inizio dell board e cerca da lì fino alla posizione originale
    current_position=0;
    while (current_position<row*cols+col){
        if (board[current_position] == ' '){
            board[current_position]=board[row*cols+col];
            board[row*cols+col]=' ';
            return board;
        }
        current_position++;
    }
    return board;
}

/* Stampa lo stato corrente della board
*
* Parametri:
*  board: la matrice contenente lo stato del sistema
*  rows: numero di righe della board
*  cols: numero di colonne della board
*/
void print_board(char* board, int rows, int cols){
    int i, j;
    for (i=0; i<rows; i++){
        for (j=0; j<cols; j++){
            printf("%c", board[i*cols + j]);
        }
        printf("\n");
    }

    return;
}
