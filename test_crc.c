#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

#include "crc16.h"

int main(void)
{
	__u16 c;

	c = crc16_ansi(0, "\x00\x01\x7f\xfa", 4);

	printf("crc=%04X\n", c);
	return 0;
}
