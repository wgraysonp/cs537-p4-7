#include "types.h"
#include "stat.h"
#include "user.h"
#include "stddef.h"
#include "wmap.h"

int main(void){
	printf(0, "pid: %d\n", getpid());
	uint test = wmap(0x80000001, 4096,  MAP_SHARED | MAP_PRIVATE | MAP_ANONYMOUS, 0);
	printf(0, "test: 0x%d\n", test);
	exit();
}
