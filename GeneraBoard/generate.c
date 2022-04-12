#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>
#define ROW 10000		
#define COL 10000
#define N1 5000000
#define N2 5000000

void populate();
void mixup();
int **GRID;
int main(int argc, char* argv){
    int i, j, temp;
    FILE *fp;
    GRID = (int **) malloc ((ROW)*sizeof(int *));
	for (i=0; i<ROW+2; i++) {
		GRID[i] = (int *) malloc ((COL)*sizeof(int));
	}
    populate();
    mixup();
    printf("\nmixed");
    fp = fopen("board.txt", "w");

    for (i = 0; i < ROW; i++){
        for (j = 0; j < COL; j++){
            fprintf(fp, "%d", GRID[i][j]);
        }
        fprintf(fp, "\n");
    }
    fclose(fp);
    free(GRID);
    return 0;
}

void populate() {
	int i, j, temp;
    int n1 = N1;
    int n2 = N2;
	for (i=0; i<ROW; i++) {
		for (j=0; j<COL; j++) {
            /*
            temp = rand()%4;
            if(temp == 0 && n1 != 0){
                GRID[i][j] = 1;
                n1--;
            }else if(temp == 1 && n2 != 0){
                GRID[i][j] = 2;
                n2--;
            }else{
                GRID[i][j] = 0;
            }*/
            if (n1 != 0) {
				GRID[i][j] = 1;
				n1--;
			} else if (n2 !=0) {
				GRID[i][j] = 2;
				n2--;
			} else {
				GRID[i][j] = 0;
			}
		}	
	}
    printf("n1, n2 = %d, %d", n1, n2);
}

void mixup() {
	int i, j, k, z, s, temp;
	for (k=0; k<20; k++) {
		for (i=0; i<ROW; i++) {
			for (j=0; j<COL; j++) {
				z = (int) (rand()%ROW);
				s = (int) (rand()%COL);
				temp = GRID[i][j];
				GRID[i][j] = GRID[z][s];
				GRID[z][s] = temp;
			}
		}
        printf("passo %d \n", k);
	}
}
