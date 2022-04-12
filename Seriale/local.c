/**
 * @file local.c
 * @author Team
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

#define ROW 10000		
#define COL 10000	
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
    fclose(fp);
    int* unhappy_spots = (int*) malloc((num_p + num_d)*sizeof(int));
    int current_row;
    int current_col;
    for (j=0; j < num_p + num_d; j++){
        unhappy_spots[j]=-1;
    }

    /**
     * @brief Inizio Algoritmo
     * 
     */
    unhappy_spots=check_agents(board, unhappy_spots, rows, cols, threshold);
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
        if(num_iterations % 50 == 0) printf("Iterazioni restanti: %d \n", num_iterations);
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

/**
 * @brief Controlla le otto posizioni contigue di ogni elemento per calcolare la felicita'
 * 
 * @param board 
 * @param row 
 * @param col 
 * @param rows 
 * @param cols 
 * @param threshold 
 * @param type 
 * @return int 
 */
int check_placement(char* board, int row, int col, int rows, int cols, double threshold, char type){
    if (type == ' '){return 1;}
    float count_same=0.0;
    float count_different=0.0;
    int right = row*cols+col+1;
    int left = row*cols+col-1;
    int bottom = (row+1)*cols+col;
    int top = (row-1)*cols+col;
    int top_right = (row-1)*cols+col+1;
    int top_left = (row-1)*cols+col-1;
    int bottom_right = (row+1)*cols+col+1;
    int bottom_left = (row+1)*cols+col-1;

    if(row!=0){
        if(board[top]!=' '){
            if(board[top]==type){count_same++;}
            else{count_different++;}
        }
        if(col!=0){
            if(board[top_left]!=' '){
                if(board[top_left]==type){count_same++;}
                else{count_different++;}
            }
        }
        if(col!=cols-1){
            if(board[top_right]!=' '){
                if(board[top_right]==type){count_same++;}
                else{count_different++;}
            }
        }
    }
    if(row!=rows-1){
        if(board[bottom]!=' '){
            if(board[bottom]==type){count_same++;}
            else{count_different++;}
        }
        if(col!=0){
            if(board[bottom_left]!=' '){
                if(board[bottom_left]==type){count_same++;}
                else{count_different++;}
            }
        }
        if(col!=cols-1){
            if(board[bottom_right]!=' '){
                if(board[bottom_right]==type){count_same++;}
                else{count_different++;}
            }
        }
    }
    if(col!=0){
        if(board[left]!=' '){
            if (board[left]==type){count_same++;}
            else{count_different++;}
        }
    }
    if(col!=cols-1){
        if (board[right]!=' '){
            if (board[right]==type){count_same++;}
            else{count_different++;}
        }
    }
    double amt_same;
    amt_same = count_same/(count_same+count_different);
    if(count_same + count_different == 0) return 1;
    if(amt_same<threshold){return 0;}
    else{return 1;}
}

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

char* move_placement(char*board, int row, int col, int rows, int cols){
    int current_position = row*cols+col;
    while (current_position < rows*cols-1){
        current_position++;
        if (board[current_position] == ' '){
            board[current_position]=board[row*cols+col];
            board[row*cols+col]=' ';
            return board;
        }
    }
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
