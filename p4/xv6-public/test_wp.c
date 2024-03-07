#include "types.h"
#include "stat.h"
#include "user.h"
#include "stddef.h"
#include "wmap.h"

int main(void){
	printf(0, "pid: %d\n", getpid());
	uint test = wmap(0x60000000, 4096, MAP_SHARED | MAP_FIXED | MAP_ANONYMOUS, 0);
	char *arr = (char*)test;
	arr[1] = 'p';
	int umap = wunmap(0x60000000);
	printf(0,"ummap res: %d\n", umap);
	uint test2 = 0x60000004;
	char *arr2 = (char*)test2;
	arr2[1] = 'p';
	printf(0, "arr2[1]: %c\n", arr2[1]);
	exit();
}
