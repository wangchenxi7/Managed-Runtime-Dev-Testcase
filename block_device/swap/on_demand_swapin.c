/**
 * Use madvice() MAD_FREE to put a specific range of memory into inactive list. 
 * These pages will be swapped out when there is a memory pressure.
 * 
 */

#include <linux/kernel.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <sys/mman.h>  
#include <sys/errno.h>
#include </usr/include/asm-generic/mman-common.h>

#include "stdint.h"
#include "stdio.h"


// Semeru
//#include <linux/swap_global_struct.h>

#define MADV_FLUSH_RANGE_TO_REMOTE 20  // Should include from linux header

#define SYS_SWAP_STAT_RESET			335
#define SYS_NUM_SWAP_OUT_PAGES	336
#define SYS_ON_DEMAND_SWAPIN		337


typedef enum {true, false} bool;

extern errno;




/**
 * Reserve memory at fixed address 
 */
static char* reserve_anon_memory(char* requested_addr, uint64_t bytes, bool fixed) {
	char * addr;
	int flags;

	flags = MAP_PRIVATE | MAP_NORESERVE | MAP_ANONYMOUS;   
	if (fixed == true) {
			printf("Request fixed addr 0x%llx ", (uint64_t)requested_addr);

		flags |= MAP_FIXED;
	}

	// Map reserved/uncommitted pages PROT_NONE so we fail early if we
	// touch an uncommitted page. Otherwise, the read/write might
	// succeed if we have enough swap space to back the physical page.
	addr = (char*)mmap(requested_addr, bytes, PROT_NONE,
											 flags, -1, 0);

	return addr == MAP_FAILED ? NULL : addr;
}



/**
 * Commit memory at reserved memory range.
 *  
 */
char* commit_anon_memory(char* start_addr, uint64_t size, bool exec) {
	int prot = (exec == true) ? PROT_READ|PROT_WRITE|PROT_EXEC : PROT_READ|PROT_WRITE;
	uint64_t res = (uint64_t)mmap(start_addr, size, prot,
																		 MAP_PRIVATE|MAP_FIXED|MAP_ANONYMOUS, -1, 0);   // MAP_FIXED will override the old mapping
	
	// commit memory successfully.
	if (res == (uint64_t) MAP_FAILED) {

		// print errno here.
		return NULL;
	}

	return start_addr;
}




int main(){
				
	int type = 0x1;
	uint64_t request_addr 	= 0x40000000; // start of RDMA meta space, 1GB not exceed the swap partitio size.
	uint64_t size  					=	0x100000;		// 1MB, for uint64_t, length is 0x20,000
	//uint64_t size  					=	0x2000000;		// 16MB, for uint64_t, length is 0x200,000
	char* user_buff;
	uint64_t i;
	uint64_t sum = 0;
	uint64_t swapped_out_pages = 0;
	int on_demand_swapin_record = 0;
	int ret;

	// 1) reserve space by mmap
	user_buff = reserve_anon_memory((char*)request_addr, size, true );
	if(user_buff == NULL){
		printf("Reserve user_buffer, 0x%llx failed. \n", (uint64_t)request_addr);
	}else{
		printf("Reserve user_buffer: 0x%llx, bytes_len: 0x%llx \n",(uint64_t)user_buff, size);
	}

	// 2) commit the space
	user_buff = commit_anon_memory((char*)request_addr, size, false);
	if(user_buff == NULL){
		printf("Commit user_buffer, 0x%llx failed. \n", (uint64_t)request_addr);
	}else{
		printf("Commit user_buffer: 0x%llx, bytes_len: 0x%llx \n",(uint64_t)user_buff, size);
	}

	printf("Phase#1, no swap out \n");
	uint64_t * buf_ptr = (uint64_t*)user_buff;
	for(i=0; i< size/sizeof(uint64_t); i++ ){
		buf_ptr[i] = i;  // the max value.
	}

	printf("	Reset the swap out statistics monitoring array\n");
	ret = syscall(SYS_SWAP_STAT_RESET, request_addr, size);
	printf("	SYS_SWAP_STAT_RESET returned %d \n", ret);

	printf("	#1 Check current swapped out pages num\n");
	swapped_out_pages = syscall(SYS_NUM_SWAP_OUT_PAGES, request_addr, size);
	on_demand_swapin_record	= syscall(SYS_ON_DEMAND_SWAPIN);
	printf("	#1 swapped out pages num 0x%llx , on demand swapin page 0x%x\n", swapped_out_pages, on_demand_swapin_record);  // should be 0.


	printf("Phase#2, invoke madvice to add the whole array into inactive list \n");
	ret = madvise(user_buff, size, MADV_FLUSH_RANGE_TO_REMOTE);
	//ret = madvise(user_buff, size, 8);
	
	printf("	#2 Check current swapped out pages num after calling madvise\n");
	swapped_out_pages = syscall(SYS_NUM_SWAP_OUT_PAGES, request_addr, size);
	printf("	#2 swapped out pages num 0x%llx \n", swapped_out_pages);  // should be size/4K
	
	if(ret !=0){
		printf("MAD_FREE failed, return value %d \n", ret);
	}

	sum =0;
	printf("Phase#3, trigger swap in.\n");
	for(i=0; i< size/sizeof(uint64_t); i+=1024 ){ // access 1 u64 per 8KB, 2pages.
		printf("buf_ptr[0x%llx] 0x%llx \n",(uint64_t)i, buf_ptr[i]);
		sum +=buf_ptr[i];  // the sum should be 0x7f0,000
	}

	printf("	#3 Check current swapped out pages num after re-access these pages\n");
	swapped_out_pages = syscall(SYS_NUM_SWAP_OUT_PAGES, request_addr, size);
	on_demand_swapin_record	= syscall(SYS_ON_DEMAND_SWAPIN);
	printf("	#3 swapped out pages num 0x%llx , on demand swapin page 0x%x \n", swapped_out_pages, on_demand_swapin_record);  // should be 0.

	printf("sum : 0x%llx \n",sum);

	return 0;
}
