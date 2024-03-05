#include "types.h"
#include "stat.h"
#include "user.h"
#include "stddef.h"
#include "wmap.h"

int main(void){
	uint addr = wmap(0x0006, 4096, MAP_SHARED | MAP_ANONYMOUS, 0);
	printf(0, "addr: %d", addr);
	exit();
}
