#include <ucontext.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/mman.h>
#include <time.h>

/*
* Here you have 3 contexts - one for each coroutine. First
* belongs to main coroutine - the one, who starts all others.
*/
ucontext_t uctx_main;
ucontext_t* uctx_func;
int num_coros;

#define handle_error(msg) do { perror(msg); exit(EXIT_FAILURE); } while (0)

#define stack_size 1024 * 1024

static void *
allocate_stack_sig()
{
	void *stack = malloc(stack_size);
	stack_t ss;
	ss.ss_sp = stack;
	ss.ss_size = stack_size;
	ss.ss_flags = 0;
	sigaltstack(&ss, NULL);
	return stack;
}

/* How to swap context */

int* ready_coro;
// High resolution timers
long int* worktime;
int* num_swaps;
struct timespec start;
struct timespec end;
// Available time in microseconds of working for each coroutine
long int target_latency, tv;

// id = 0 - main context, id > 0 = coroutine context 
void swap(int id)
{
	clock_gettime(CLOCK_REALTIME, &end);
	
	if(id == -1) {
		for(int j=0; j<num_coros; j++) {
			if (ready_coro[j]) {
				clock_gettime(CLOCK_REALTIME, &start);
				if (swapcontext(&uctx_main, uctx_func+j) == -1)
	        			handle_error("swapcontext");
				break;
			}	
		}
		return;
	}

	tv = (end.tv_sec - start.tv_sec)*1000000 + (end.tv_nsec - start.tv_nsec)/1000;
	if(tv < target_latency)
		return;
	worktime[id] += tv; 
	for(int j=1; j<num_coros; j++)
	{
		if (ready_coro[(id+j)%num_coros]) {
			num_swaps[id] ++;
			clock_gettime(CLOCK_REALTIME, &start);
			if (swapcontext(uctx_func+id, uctx_func+(id+j)%num_coros) == -1)
	        		handle_error("swapcontext");
			break;
		}	
	}
}

int* merge(int* arr1, int* arr2, int size1, int size2, int id)
{
	int* merged_arr = (int*)malloc(sizeof(int)*(size1 + size2));
	swap(id);
	int i1 = 0, i2 = 0;
	swap(id);
	for(int i = 0; i < size1 + size2; i++)
	{
		if(i1 < size1 && i2 < size2)
		{
			if(arr1[i1] < arr2[i2])
			{
				swap(id);
				merged_arr[i] = arr1[i1];
				swap(id);
				i1++;
				swap(id);
			}
			else
			{
				swap(id);
				merged_arr[i] = arr2[i2];
				swap(id);
				i2++;
				swap(id);
			}
		}
		else if(i1 < size1)
		{
			swap(id);
			merged_arr[i] = arr1[i1];
			swap(id);
			i1++;
			swap(id);
		}
		else
		{
			swap(id);
			merged_arr[i] = arr2[i2];
			swap(id);
			i2++;
			swap(id);
		}
	}
	return merged_arr;
}

int* merge_sort(int* arr, int first, int last, int id)
{
	if(first == last)
		return &arr[first];
	swap(id);
	int middle = (first + last)/2;
	swap(id);
	int* left_arr = merge_sort(arr, first, middle, id);
	swap(id);
	int* right_arr = merge_sort(arr, middle + 1, last, id);
	swap(id);
	return merge(left_arr, right_arr, middle - first + 1, last - middle, id);
}

/* Coroutine body */
static void
my_coroutine(int id, char* filename, int** arr_sorted, int* pnum_el)
{
	clock_gettime(CLOCK_REALTIME, &start);
	printf("coro%d: started\n", id);
	swap(id);
	int num_el = 0;
	swap(id);
	int buf;
	swap(id);
	FILE* f;
	swap(id);

	f = fopen(filename, "r");
	swap(id);
	while(1)
	{
		if(fscanf(f,"%d",&buf) == EOF)
			break;
		swap(id);
		num_el ++;
		swap(id);
	}
	
	fclose(f);
	swap(id);

	int* arr = (int*)malloc(sizeof(int)*num_el);
	swap(id);
	f = fopen(filename, "r");
	swap(id);
	for(int i=0; i<num_el; i++)
		if(fscanf(f,"%d",&buf)) {
			swap(id);
			arr[i] = buf;
			swap(id);
	}

	arr_sorted[id] = merge_sort(arr, 0, num_el-1, id);
	swap(id);
	*pnum_el = num_el;
	swap(id);

	printf("coro%d: returning\n", id);
	swap(id);
	ready_coro[id] = 0;
	clock_gettime(CLOCK_REALTIME, &end);		
	worktime[id] += (end.tv_sec - start.tv_sec)*1000000 + (end.tv_nsec - start.tv_nsec)/1000;
}


int main (int argc, char *argv[])
{
	struct timespec start_time;
	struct timespec end_time;
	clock_gettime(CLOCK_REALTIME, &start_time);
	
	num_coros = argc - 2;
	printf("Number of coros: %d\n", num_coros);
	char** str = argv + 2;
	uctx_func = (ucontext_t*)malloc(sizeof(ucontext_t)*num_coros);
	ready_coro = (int*)malloc(sizeof(int)*num_coros);
	target_latency = atoi(argv[1]);
	printf("Target latency: %ld\n", target_latency);
	worktime = (long int*)malloc(sizeof(long int)*num_coros);
	num_swaps = (int*)malloc(sizeof(int)*num_coros);
	int** arr_sorted = (int**)malloc(sizeof(int*)*num_coros);
	int* num_el = (int*)malloc(sizeof(int)*num_coros); 
	int* arr_final = arr_sorted[0];
	int num_el_total = 0;

	/* Initialization of coroutine structures.*/
	for(int i=0; i<num_coros; i++)
	{
		ready_coro[i] = 1;
		worktime[i] = 0;
		num_swaps[i] = 0;
		if (getcontext(uctx_func+i) == -1)
			handle_error("getcontext");
		uctx_func[i].uc_stack.ss_sp = allocate_stack_sig();
		uctx_func[i].uc_stack.ss_size = stack_size;
	
		/* Here you specify, to which context to
		 * switch after this coroutine is finished */
		uctx_func[i].uc_link = &uctx_main;
		makecontext(uctx_func+i, (void(*)(void))my_coroutine, 4, i, str[i], arr_sorted, num_el+i);
	}
	
	/* Here coroutines start */
	printf("main: start\n");
	if (swapcontext(&uctx_main, uctx_func) == -1)
		handle_error("swapcontext");
	for(int i=0; i<num_coros; i++)
		swap(-1);

	// Merging all files into one file and sorting it
	num_el_total = num_el[0];
	arr_final = arr_sorted[0];
	for(int i=1; i<num_coros; i++)
	{
		arr_final = merge(arr_final, arr_sorted[i], num_el_total, num_el[i], 0);
		num_el_total += num_el[i];
	}
	FILE* f;
	f = fopen("output.txt","w");
	for(int i=0; i<num_el_total; i++)
		fprintf(f, "%d ", arr_final[i]);
	fclose(f);
	
	printf("main: exiting\n");
	
	// Work time calculations
	clock_gettime(CLOCK_REALTIME, &end_time);
	printf("Programm execution time: %ld misrosec\n", (end_time.tv_sec - start_time.tv_sec)*1000000 + (end_time.tv_nsec - start_time.tv_nsec)/1000);
	printf("Coroutines execution time and number of swaps:\n");
	for(int i=0; i<num_coros; i++)
		printf("\tcoro%d: %ld microsec, %d swaps\n", i, worktime[i], num_swaps[i]);
	return 0;
}


