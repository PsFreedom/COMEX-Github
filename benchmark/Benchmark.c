#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]){	
	int i, *Mem_Area;
	unsigned long N_Pages,totalInt;
	
	N_Pages = strtol(argv[1], NULL, 10);
	totalInt = N_Pages*1024;
	
	Mem_Area = (int*)malloc(sizeof(int)*totalInt);
	
	while(1){
		for(i=0; i<totalInt; i++){
			Mem_Area[i] = totalInt - i;
		}
		sleep(60);
	}
	
	return 0;
}
