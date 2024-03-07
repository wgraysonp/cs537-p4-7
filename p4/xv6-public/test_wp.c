#include "types.h"
#include "stat.h"
#include "user.h"
#include "stddef.h"
#include "wmap.h"

int main(void){
	printf(0, "pid: %d\n", getpid());
	uint test = wmap(0x60000000, 8192, MAP_SHARED | MAP_FIXED | MAP_ANONYMOUS, 0);
	//iprintf(0, "test: %d\n", test);
	uint test2 = 0x60000000 + 4096;
	//uint test_addr = 0x60000000;
	char *arr = (char*)test;
	arr[1] = 'p';
	printf(0, "first worked\n");
	char *arr2 = (char*)test2;
	arr2[1] = 'p';
	//uint test2  = wmap(0x70000000, 4096, MAP_SHARED | MAP_ANONYMOUS, 0);
	//printf(0, "test 2: %d\n", test);
	exit();
}
