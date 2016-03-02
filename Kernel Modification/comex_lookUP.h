
int powOrder(int Order){
	int result = 1;
	
	while(Order > 0){
		result = result*2;
		Order--;
	}	
	return result;
}

pte_t pageWalk_getPTE(unsigned long Addr){

	pgd_t *pgd;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *ptep, pte;
	spinlock_t *ptl;
	
	pgd = pgd_offset(COMEX_mm, Addr);
	if (pgd_none(*pgd) || pgd_bad(*pgd)){
		printk(KERN_INFO "%s: PGD bug\n", __FUNCTION__);
	}	
	pud = pud_offset(pgd, Addr);
	if (pud_none(*pud) || pud_bad(*pud)){
		printk(KERN_INFO "%s: PUD bug\n", __FUNCTION__);
	}
	pmd = pmd_offset(pud, Addr);
	if (pmd_none(*pmd) || pmd_bad(*pmd)){
		printk(KERN_INFO "%s: PMD bug\n", __FUNCTION__);
	}	
	
	ptep = pte_offset_map_lock(COMEX_mm, pmd, Addr, &ptl);
	pte = *ptep;
	pte_unmap_unlock(ptep, ptl);
	
	return pte;
}

unsigned long getPhyAddrLookUP(unsigned long entry){
	return entry >> 28;
}

unsigned int getPageNumber(unsigned long entry){
	return (unsigned int)(entry & 0xFFFFFF0) >> 4;
}

unsigned int getSizeOrder(unsigned long entry){
	return (unsigned int)(entry & 0xF); 
}

void myQuickSort(unsigned long arr[], int left, int right) {
      int i = left, j = right;
      unsigned long tmp;
      unsigned long pivot = arr[(left + right) / 2];
 
      /* partition */
      while (i <= j) {
            while (arr[i] < pivot)
                  i++;
            while (arr[j] > pivot)
                  j--;
            if (i <= j) {
                  tmp = arr[i];
                  arr[i] = arr[j];
                  arr[j] = tmp;
                  i++;
                  j--;
            }
      };
 
      /* recursion */
      if (left < j)
            myQuickSort(arr, left, j);
      if (i < right)
            myQuickSort(arr, i, right);
}

int binSearchCOMEXLookUP(unsigned long value){
	int head, middle, tail;
	unsigned long firstVal, lastVal;
	
	head = 0;
	tail = totalLookUPEntry-1;
//	printk(KERN_INFO "%s , Search Addr %lX", __FUNCTION__, value);	
	while(tail >= head){
		middle = (head+tail)/2;
		firstVal = getPhyAddrLookUP(comexLookUP[middle]);
		lastVal = getPhyAddrLookUP(comexLookUP[middle]) + (powOrder(getSizeOrder(comexLookUP[middle]))-1);	
		if(value >= firstVal && value <= lastVal){
			return getPageNumber(comexLookUP[middle]) + (value - firstVal);
		}
		else if(lastVal < value){
			head = middle+1;
		}
		else{
			tail = middle-1;
		}
	}
	return -1;
}
