/* A simulation of the schelling segregation model
 *
 * Authors: Sydney Finkelstein, Annabel Winters-McCabe
 *
*/

#include <stdio.h>
#include<stdlib.h>
#include<math.h>
#include<unistd.h>

#define ROW 10000		//numero di totale di righe della matrice per la strong, numero di righe per ogni processo per la weak
#define COL 10000	//numero di colonne della matrice
#define ITER 200
#define WHITE '1'
#define BLACK '2'
#define EMPTY '0'
#define THRESHOLD 0.0001

int check_placement(char* board, int row, int col, int num_rows, int num_cols, double threshold, char type);
int count_unhappy(int*unhappy_spots, int num_chars);
int* check_agents(char*board,int* unhappy_spots,int rows, int cols, double threshold);
char* move_placement(char*board, int row, int col, int rows, int cols);
void print_board(char* board, int rows, int cols);



int main(int argc, char** argv){

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
    //allocate space for an array containing which spots in the board are
    //unhappy
    int* unhappy_spots = (int*) malloc((num_p + num_d)*sizeof(int));

    int current_row;
    int current_col;

    //fill board with -1's before checking which are happy and unhappy
    for (j=0; j < num_p + num_d; j++){
        unhappy_spots[j]=-1;
    }


    unhappy_spots=check_agents(board, unhappy_spots, rows, cols, threshold);
    // ALGORITMO
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
        if(num_iterations % 50 == 0) printf("It restanti: %d \n", num_iterations);
    }

    
    // se unhappy_spots[0] != -1 significa che ha trovato almeno un elemento non felice
    // la soluzione quindi non risulta ottima
    if(unhappy_spots[0] != -1) printf("La soluzione trovata non e\' ottima\n\n");
    else printf("La soluzione trovata e\' ottima, iterazioni restanti = %d \n", num_iterations);
    printf("Elementi non felici: %d", count_unhappy(unhappy_spots, num_d + num_p));
    free(unhappy_spots);
    free(board);
    unhappy_spots=NULL;
    board = NULL;
    return 0;
}

/* Determines whether the character at the given spot in the board is happy or
* unhappy.
*
* Parameters:
*  board: the allocated array representing the
*      board full of chars and empty spaces
*  row: integer representing the row of the given character
*  col: integer representing the column of the given character
*  rows: integer representing the total number of rows in the board
*  cols: integer representing the total number of columns in the board
*  threshold: floating point number representing the threshold to determine
*      whether the given spot can be considered unhappy
*  type: character that is the character at the given spot
*
* Returns:
*  returns 1 if the character in the given spot in the board is happy,
*    and 0 if it is unhappy
*/
int check_placement(char* board, int row, int col, int rows, int cols, double threshold, char type){
    //if the character at that spot is empty, the spot is considered happy
    if (type == ' '){return 1;}
    float count_same=0.0;
    float count_different=0.0;

    //initialize variables that represent the given spot's neighbors
    int right = row*cols+col+1;
    int left = row*cols+col-1;
    int bottom = (row+1)*cols+col;
    int top = (row-1)*cols+col;
    int top_right = (row-1)*cols+col+1;
    int top_left = (row-1)*cols+col-1;
    int bottom_right = (row+1)*cols+col+1;
    int bottom_left = (row+1)*cols+col-1;

    //if not the top row
    if(row!=0){
        //check above
        if(board[top]!=' '){
            if(board[top]==type){count_same++;}
            else{count_different++;}
        }
        //if not left column
        if(col!=0){
        //check top left
            if(board[top_left]!=' '){
                if(board[top_left]==type){count_same++;}
                else{count_different++;}
            }
        }
        //if not right column
        if(col!=cols-1){
        //check top right
            if(board[top_right]!=' '){
                if(board[top_right]==type){count_same++;}
                else{count_different++;}
            }
        }
    }
    //if not bottom row
    if(row!=rows-1){
    //check below
        if(board[bottom]!=' '){
            if(board[bottom]==type){count_same++;}
            else{count_different++;}
        }
        //if not left column
        if(col!=0){
        //check bottom left
            if(board[bottom_left]!=' '){
                if(board[bottom_left]==type){count_same++;}
                else{count_different++;}
            }
        }
        //if not right column
        if(col!=cols-1){
        //check bottom right
            if(board[bottom_right]!=' '){
                if(board[bottom_right]==type){count_same++;}
                else{count_different++;}
            }
        }
    }
    //if not left column
    if(col!=0){
    //check left
        if(board[left]!=' '){
            if (board[left]==type){count_same++;}
            else{count_different++;}
        }
    }
    //if not right column
    if(col!=cols-1){
    //check right
        if (board[right]!=' '){
            if (board[right]==type){count_same++;}
            else{count_different++;}
        }
    }

    //divide the number of the spot's neighbors that are the same as the given
    //spot by the total number of neighbors that are characters and compare to
    //threshold
    double amt_same;
    amt_same = count_same/(count_same+count_different);
    if(count_same + count_different == 0) return 1;
    if(amt_same<threshold){return 0;}
    else{return 1;}
}

/* Checks whether the board still has unhappy characters, or whether all unhappy
* characters from the round have been moved
*
* Parameters:
*  unhappy_spots: dynamically allocated int array representing which spots in
*    the board are unhappy and which are happy. Unhappy_spots contains a
*    placeholder of -1 where the spots are happy
*  num_chars: integer representing the total number of characters in the board
*
*  Return:
*  1 if there is at least one dissatisifed agent left on the board, 0 if there
*  are none.
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
*Checks the board for dissatisfied agents and returns an array of their indices
*
*Parameters:
* board: dynamically allocated 2D array representing the state of the simulation
* unhappy_spots:dynamically allocated int array representing which spots in
*    the board are unhappy and which are happy
* rows: integer representing the # of rows in the board.
* cols: integer representing the # of columns in the board.
*
* Return:
* unhappy_spots
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

/* Moves a dissatisfied agent to the nearest open spot, wrapping around to the
* beginning index of the board if necessary.
*
*Parameters:
* board: dynamically allocated 2D array representing the state of the simulation
* unhappy_spots:dynamically allocated int array representing which spots in
*    the board are unhappy and which are happy
* int row: current x coordinate of the agent to be moved
* int col: current y coordinate of the agent to be moved
* rows: integer representing the # of rows in the board.
* cols: integer representing the # of columns in the board.
*
* Return:
* an updated board
*/
char* move_placement(char*board, int row, int col, int rows, int cols){
    int current_position = row*cols+col;
    //while the current_position has not yet reached the end of the board
    while (current_position < rows*cols-1){
        current_position++;
        if (board[current_position] == ' '){
            board[current_position]=board[row*cols+col];
            board[row*cols+col]=' ';
            return board;
        }
    }
    //if the position has reached the end of the board without finding an open
    //spot, wrap around to the beginning of the board and look from there until
    //the original position
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

/* Prints the current state of the board
*
* Parameters:
*  board: dynamically allocated char array representing the current state of
*    the board
*  rows: integer representing the total number of rows
*  cols: integer representing the total number of columns
*
* Return: none
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
