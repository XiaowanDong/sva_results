#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>


#define ITER 20000 //20000 

union element {
	int next;
	char filler[64];
};


static inline uint64_t sva_read_tsc (void)
{
	uint64_t hi, lo;
	__asm__ __volatile__ ("rdtsc\n" : "=a"(lo), "=d"(hi));
	return lo | (hi << 32);
}

void shuffle(int *array, size_t n)
{
	if (n > 1) 
	{
		size_t i;
		for (i = 0; i < n - 1; i++) 
		{
			size_t j = i + rand() / (RAND_MAX / (n - i) + 1);
			int t = array[j];
			array[j] = array[i];
			array[i] = t;
		}
		
	}
}

/*
 * Function: secmemalloc()
 *
 * Description:
 *  Ask the SVA VM to allocate some ghost memory.
 */
static inline void *
secmemalloc(size_t size)
{
	void * ptr;

	if (getenv("GHOSTING")) {
		__asm__ __volatile__ ("int $0x7f\n" : "=a" (ptr) : "D" (size));
	} else {
		ptr = malloc(size);
	}
	return ptr;
}

/*
 * Function: secmemfree()
 *
 * Description:
 *  Return ghost memory back to the SVA VM.
 */
static inline void
secmemfree(void * ptr, size_t size)
{
	if (getenv("GHOSTING")) {
		__asm__ __volatile__ ("int $0x7e\n" : : "D" (ptr), "S" (size));
	} else {
		free(ptr);
	}
}

int main(int argc, char * argv[])
{
	if(argc != 2)
	{
		fprintf(stdout, "[usage] ./gmem_test_random SIZE(in KB)\n");
		return 0;
	}
	int n = atoi(argv[1]);
	size_t size = (1024 * n)/ sizeof(union element); //4MB, half of LLC
	int i;
	uint64_t start[3], end[3];

	start[0] = sva_read_tsc();
	union element * array = (union element *) secmemalloc(size * sizeof(union element));
	end[0] = sva_read_tsc();

	int * indices = (int *) secmemalloc(size * sizeof(int));
	for(i = 0; i < size; i++)
		indices[i] = i;
	int p = indices[size - 1];
	for(i = 0; i < size; i++)
	{
		array[p].next = indices[i];
		p = indices[i];
	}

	shuffle(indices, size);
	p = indices[size - 1];
	
	start[1] = sva_read_tsc();
	for(i = 0; i < size; i++)
	{
		array[p].next = indices[i];
		p = indices[i];
	}
	end[1] = sva_read_tsc();

	start[2] = sva_read_tsc();
	for(int iter = 0; iter < ITER; iter ++)
	{
		for(i = 0; i < size; i++)
		{
			array[p].next = indices[i];
			p = indices[i];
		}
	}
	end[2] = sva_read_tsc();
	fprintf(stderr, "size = %dKB latency = %lu %lu %lu\n", n, (end[0] - start[0]), (end[1] - start[1]), (end[2] - start[2])/ITER);
	secmemfree(indices, size * sizeof(int));
	secmemfree(array, size * sizeof(union element));
	return 0;
}
