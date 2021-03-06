/***************************************************************************
 *  Title: Kernel Memory Allocator
 * -------------------------------------------------------------------------
 *    Purpose: Kernel memory allocator based on the buddy algorithm
 *    Author: Stefan Birrer
 *    Copyright: 2004 Northwestern University
 ***************************************************************************/
/***************************************************************************
 *  ChangeLog:
 * -------------------------------------------------------------------------
 *    Revision 1.2  2009/10/31 21:28:52  jot836
 *    This is the current version of KMA project 3.
 *    It includes:
 *    - the most up-to-date handout (F'09)
 *    - updated skeleton including
 *        file-driven test harness,
 *        trace generator script,
 *        support for evaluating efficiency of algorithm (wasted memory),
 *        gnuplot support for plotting allocation and waste,
 *        set of traces for all students to use (including a makefile and README of the settings),
 *    - different version of the testsuite for use on the submission site, including:
 *        scoreboard Python scripts, which posts the top 5 scores on the course webpage
 *
 *    Revision 1.1  2005/10/24 16:07:09  sbirrer
 *    - skeleton
 *
 *    Revision 1.2  2004/11/05 15:45:56  sbirrer
 *    - added size as a parameter to kma_free
 *
 *    Revision 1.3  2004/12/03 23:04:03  sbirrer
 *    - initial version for the kernel memory allocator project
 *

 ************************************************************************/
 //TO FIX:!! when we get the second page, the page can't be initialized properly.
 //TO DO	the 256 block is the existed 256 free blk, instead of the new page's 
 //			beginging 256 byte.
 /************************************************************************
 Project Group: hlv624, ywb017
 
 ***************************************************************************/

#ifdef KMA_BUD
#define __KMA_IMPL__

/************System include***********************************************/
#include <assert.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

/************Private include**********************************************/
#include "kma_page.h"
#include "kma.h"

/************Defines and Typedefs*****************************************/
/*  #defines and typedefs should have their names in all caps.
 *  Global variables begin with g. Global constants with k. Local
 *  variables should be in all lower case. When initializing
 *  structures and arrays, line everything up in neat columns.
 */

/************Global Variables*********************************************/
typedef struct freeBlock{
	struct freeBlock* nextFree ;
	int size;
}free_blk;
//Header  32,64,128,256,512,1024,2048,4096
free_blk* freeListHeader[8];
kma_page_t* g_pageList;
/************Function Prototypes******************************************/
void* kma_malloc(kma_size_t size);
void kma_free(void* ptr, kma_size_t size);
/************External Declaration*****************************************/
void addToFreeList(free_blk* ptr, int size,int index);
void* findFreeBlk(int size);
void splitMem(free_blk* father,int index);
void coalesce();
int roundSize(int size);
void setBitMap(free_blk* blkptr, int size,char num);
bool checkBuddy(void* pagePtr,int curPos,int nbrPos,int span);
void*checkNbrEmpty(void* ptr, kma_size_t size);
kma_page_t* checkPage(free_blk* blk);
void deleteFreeBlk(void* pageStart, int index);
void delFreePageBlk(kma_page_t* pageStart);
void delFromList(free_blk* ptr,int index);
void addToFreeList_init(free_blk* ptr,int size,int index);

/**************Implementation***********************************************/
void addToPageList(kma_page_t* newPage){
	kma_page_t* current;
	current = g_pageList;
	if (current==NULL){
		g_pageList = newPage;
		g_pageList->next = NULL;
		return;
	}
	while (current->next!=NULL){
		current = current->next;
	}
	newPage->next = NULL;
	current->next = newPage;
}

int findEntry(int sizeRound){
	int index;
	if (sizeRound==1<<13){
		return -2;
	}
	for(index=0; index<8;index++){
		if((2<<(index+4))==sizeRound){
			return index;
		}
	}
	return -1;
}
void initializeBitMap(kma_page_t* page){
	int i;
	char* a;
	a = (char*)(page->ptr); 
	for (i=0;i<256;i++)
	{
		*a = '0';
		a+=1;
	}
}

void setBitMap(free_blk* blkptr, int size,char num){
	int span = size/32;
	void* pageHead = BASEADDR(blkptr);
	int bitStart = ((void*)blkptr - pageHead)/32;
	memset(pageHead+bitStart,num,span);
}


void splitMem(free_blk* father,int index){
	void* temptr;
	int newspaceSize;
	temptr = (void*)father;
	father = father->nextFree;
	newspaceSize = 2<<(index+3);
	addToFreeList((free_blk*)temptr,newspaceSize,index-1);
	addToFreeList((free_blk*)(temptr + newspaceSize),newspaceSize,index-1);
}

void initializePage(kma_page_t* page){
//put a 256 bit 0 at the begining of the page
	void* temptr = page->ptr;
	addToFreeList_init((free_blk*)(temptr+256),256,3);
	addToFreeList_init((free_blk*)temptr,256,3);
	addToFreeList_init((free_blk*)(temptr+512),512,4);
	addToFreeList_init((free_blk*)(temptr+1024),1024,5);
	addToFreeList_init((free_blk*)(temptr+2048),2048,6);
	addToFreeList_init((free_blk*)(temptr+4096),4096,7);
}
void* findFreeBlk(int size){
	int index = 0;
	int sizeRound;
	void* res;
	sizeRound = roundSize(size);
	index =	findEntry(sizeRound);
	if (index == -2){
		//the size > 4096 , return a whole delFreePageBlkge without bitmap;
		kma_page_t* wholePage = get_page();
		wholePage -> num_in_use = 0;
		//Add the page to pagelist
		addToPageList(wholePage);
		return wholePage->ptr;
	}
	if(freeListHeader[index]==NULL){

		for(;index< 8;index++){
			if(freeListHeader[index]!=NULL){
				free_blk* temp = freeListHeader[index];
				freeListHeader[index] = freeListHeader[index]->nextFree;
				splitMem(temp,index);
				res = findFreeBlk(size);
				return res;
			}			
		}
		//if all the entries are NULL, get a new page
		kma_page_t* page;
		page = get_page();
		page->num_in_use=0;
		addToPageList(page);
		if (size > page->size)
		{ // requested size too large
			free_page(page);
			return NULL;
		}
		initializePage(page);
		findFreeBlk(256);  //add a flag, ptr
		setBitMap(page->ptr,8192,'0');		
		res = findFreeBlk(size);
		return res; 
	}else{
		void* ptr;
		ptr = freeListHeader[index];
		freeListHeader[index] = freeListHeader[index] -> nextFree;
		setBitMap(ptr,sizeRound,'1');
		return ptr;
	}

}

void* kma_malloc(kma_size_t size)
{
	void* res;
	res = findFreeBlk(size);
	if(res!=NULL){
		kma_page_t* currentPage;
		currentPage = checkPage(res);
		currentPage->num_in_use+=1;
	}
  return res;
}
void* checkNbrEmpty(void* ptr, kma_size_t size){
	void* pageStart = BASEADDR(ptr);
	int blkStart = (ptr-pageStart)/(32);
	int span = size/32;
	int i;
	bool prevEmpty=TRUE;
	bool nextEmpty=TRUE;
	void* prevNbr = ptr - size;
	void* nextNbr = ptr + size; 
	//check prev empty
	char* a;
	a = pageStart+blkStart-span;
	for(i=0;i<span;i++){
		if(*a == '1'){
			prevEmpty = FALSE;
			break;
		}
		a+=1;
	}
	a = pageStart+blkStart+span;
	//check next empty
	for(i=0;i<span;i++){
		if(*a == '1'){
			nextEmpty = FALSE;
			break;
		}
		a+=1;
	}
	int entry = findEntry(size);
	// check if them are buddy, return the buddy's ptr
	if(freeListHeader[entry]!=NULL){
		if(prevEmpty && nextEmpty ){
			if(checkBuddy(pageStart,blkStart,blkStart-span,span)){
				return prevNbr;
			}else if(checkBuddy(pageStart,blkStart, blkStart+span ,span)){
				return nextNbr;
			}
		}else if(prevEmpty == TRUE){
			if (checkBuddy(pageStart,blkStart, blkStart-span,span)){
				return prevNbr;
			}
		}else if(nextEmpty ==TRUE){
			if(checkBuddy(pageStart,blkStart,blkStart+span,span)){
				return nextNbr;
			}
		}
	}
	return NULL;
}

void rmPageFrPageList(kma_page_t* pageToRm){
	kma_page_t* current = g_pageList;
	if (current==NULL){
		return;
	}
	if(current == pageToRm){
		if (current->next!=NULL){
			g_pageList= g_pageList->next;
		}else{
			g_pageList=NULL;			
		}
		return;
	}
	current=current->next;
	kma_page_t* prev = g_pageList;
	while(current!=NULL){
		if(current==pageToRm){
			prev->next = current->next;
			return;
		}
		prev = current;
		current=current->next;
	}
} 
void kma_free(void* ptr, kma_size_t size)
{
	//if size > 4096,free page
	kma_page_t* freeBlkPage;
	freeBlkPage =  checkPage(ptr);
	freeBlkPage->num_in_use-=1;	
	if(freeBlkPage->num_in_use==0){
		rmPageFrPageList(freeBlkPage);
		delFreePageBlk(freeBlkPage);
		free_page(freeBlkPage);
		return;
	}
	int sizeRound;
	sizeRound = roundSize(size);
	void* ptr2;
	//return the empty buddy.
	void* iter;
	setBitMap(ptr,sizeRound,'0');
	//check for coalescing
	while(1){
		ptr2 = checkNbrEmpty(ptr,sizeRound);
		int index;
		index = findEntry(sizeRound);
		if (ptr2!=NULL){
			// have empty boddy, coalescing them
			delFromList(ptr,index);
			delFromList(ptr2,index);
			if(ptr < ptr2){
				iter=ptr; 
				memset(iter, 0 , sizeRound*2);
				addToFreeList(ptr,sizeRound*2,index+1);
				sizeRound*=2;
			}else{
				iter=ptr2;
				memset(iter, 0 , sizeRound*2);
				addToFreeList(ptr2,sizeRound*2,index+1);
				ptr = ptr2;
				sizeRound *=2;
			}
		}else if(ptr2 == NULL){
			iter = ptr;
			memset(iter, 0 , sizeRound);
			// add it to the free blk list;
			addToFreeList(ptr,sizeRound,index);
			break;
		}
	}
}
void delFromList(free_blk* ptr,int index){
	free_blk* current ;
	free_blk* prev;
	current = freeListHeader[index];
	if(current==NULL){
		return;
	}
	prev = current;
	if(current == ptr){
		freeListHeader[index]= freeListHeader[index]->nextFree;
		return;
	}
	prev = current;
	current=current->nextFree;
	while(current!=NULL){
		if(current == ptr){
			prev->nextFree = current->nextFree;
			return;
		}
		prev = current;
		current = current->nextFree;
	}
}

bool checkBuddy(void* pagePtr,int curPos,int nbrPos,int span){
	//check if ptr1 and ptr2 are buddies, if it is return true
	free_blk* nbrPtr = pagePtr + nbrPos*32;
	int size = span*32;
	if(nbrPtr->size != size){
		return FALSE;
	}
	int i = 0;
	for (i = 0 ; i < 8 ; i++){

		if( curPos - span == 1<<i){
			return TRUE;
		}else if(curPos + 2 * span == 1<<i){
			return TRUE;
		}else if(curPos - span == 0 || curPos + 2 * span == 1<<8){
			return TRUE;
		}
	}
	return FALSE;
}

kma_page_t* checkPage(free_blk* blk){
// walk through the pagelist and compare the address of blk  with the address of the adress
	if (g_pageList==NULL){
		return NULL;
	}
	int size=0;
	kma_page_t* currentPage;
	void* currentSpace;
	size = g_pageList->size;
	currentPage = g_pageList;
	currentSpace = currentPage->ptr;
	while(currentPage!=NULL){
		if( currentSpace<=((void*)blk) && ((void*)blk) < currentSpace+size){
			return currentPage;
		}
		currentPage = currentPage->next;
		if(currentPage!=NULL){
			currentSpace = currentPage->ptr;
		}
	}
	return NULL;
}
void addToFreeList_init(free_blk* ptr,int size,int index){
	free_blk* current;
	current = freeListHeader[index];
	if (current==NULL){
		freeListHeader[index] = ptr;
		freeListHeader[index]->nextFree=NULL;
		freeListHeader[index]->size = size;
		return; //(void*)freeListHeader[index];
	}
	ptr->nextFree = current;
	ptr->size = size;
	freeListHeader[index]=ptr;
}
void addToFreeList(free_blk* ptr, int size, int index){
	free_blk* current;
	free_blk* prev;
	current = freeListHeader[index];
	if (current==NULL){
		freeListHeader[index] = ptr;
		freeListHeader[index]->nextFree=NULL;
		freeListHeader[index]->size = size;
		return; 
	}
	if (ptr < current){
		freeListHeader[index] = ptr;
		freeListHeader[index]->nextFree = current;
		freeListHeader[index]->size = size;		
		return; 
	}
	prev = current;
	current = current -> nextFree;
	if(current == NULL){
		prev->nextFree = ptr;
		ptr->nextFree = NULL;
		ptr->size = size; 
	}
	while(current!=NULL){
		if( ptr< current){
			prev -> nextFree = ptr;
			ptr -> nextFree = current;
			ptr -> size = size;
		}
		prev = current;
		current = current -> nextFree;
	}
}

//delete the free blocks which belongs to empty page, then free the page
void delFreePageBlk(kma_page_t* pageStart){
	int i;
	for( i=0 ; i<8; i++ ){
		deleteFreeBlk(pageStart->ptr,i);
	}
}
void deleteFreeBlk(void* pageStart, int index){
	if (freeListHeader[index]==NULL){
		return;
	}
	int size; 
	size = ((kma_page_t*)pageStart)->size;
	free_blk* current = freeListHeader[index];
	free_blk* prev = NULL;
	int flag = 0;  // set a flag to find the first node to be freeBlkHead
	while (current!=NULL){
		if((void*)current>= pageStart && ((void*)current <= pageStart + size)){
			//if the blk locate in the page we gonna free. we delete the blk
			if(flag ==1){
				prev->nextFree = current->nextFree;
				current = prev;
				current = current->nextFree;
			}else{
				freeListHeader[index] = freeListHeader[index]->nextFree;
				current = freeListHeader[index];				
			}
		}else{
			if ( flag == 0 ){
				flag = 1;
				freeListHeader[index] = current;
				prev = current;
				current=current->nextFree;

			}else{
				current= current->nextFree;
			}
		}
	}
}
int roundSize(int size){
	int base = 32 ;
	int tempNum = base;
	if (size > base){
		while(size > tempNum){
			tempNum = tempNum * 2;
		}
		return tempNum;	
	}else{
		return base;
	}
}

#endif // KMA_BUD
