#include "types.h"
#include "stat.h"
#include "user.h"
#include "stddef.h"
#include "wmap.h"

int main(void){
	int test = wmap(0x0016, 4096, MAP_SHARED | MAP_ANONYMOUS, 0);
	printf(0, "test: %d", test);
	test = wmap(0x0016, 4096, MAP_SHARED | MAP_ANONYMOUS, 0);
	printf(0, "test 2: %d", test);
	exit();
}
