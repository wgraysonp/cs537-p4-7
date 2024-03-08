#include "types.h"
#include "stat.h"
#include "user.h"
#include "stddef.h"
#include "wmap.h"
#include "fcntl.h"

int main(void){
	printf(0, "pid: %d\n", getpid());
	char *fname = "README";
	int fd = open(fname, O_RDWR);

	printf(0, "fd: %d\n", fd);	
	uint test = wmap(0x60000000, 2287, MAP_PRIVATE, fd);
	printf(0, "test: 0x%x\n", test);
	char *arr = (char*)test;
	arr[2888] = '\0';
	printf(0, "junk: %s\n", arr);
	//int unmap = wunmap(test);
	//printf(0, "unmap : %d\n", unmap);
	exit();
}
