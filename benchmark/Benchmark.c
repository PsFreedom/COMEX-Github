#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]){	
	char *Mem_Area;
	unsigned long i, j, N_Pages, totalChar;
	
	N_Pages = strtol(argv[1], NULL, 10);
	totalChar = N_Pages*4096;
	Mem_Area = (char *)malloc(sizeof(char)*totalChar);
	
	for(i=0, j=0; i<totalChar; i+=4096, j++){
		Mem_Area[i] = j;
	}
	while(1){
		for(i=0; i<totalChar; i+=4096){		
			Mem_Area[i]++;
		}
		sleep(20);
		printf("Mem_Area[i] %d\n", Mem_Area[4096]);
	}	
	return 0;
}
