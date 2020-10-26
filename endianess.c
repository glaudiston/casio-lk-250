#include <stdio.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>

// compile with:
// gcc endianness.c -o endianess -lm
// https://en.wikipedia.org/wiki/Endianness
// https://stackoverflow.com/questions/1001307/detecting-endianness-programmatically-in-a-c-program#1001373
// https://www.boost.org/doc/libs/1_67_0/doc/html/predef/reference.html#predef.reference.other_macros
// pow(2, 17)
# define MAX_VALUE 131072
# define BITS 16
int main(int argc, char *argv[])
{
        uint32_t v = 0x1234;
	if ( argc <  2 ) {
		printf("Using default value: %i\n", v);
	} else {
		v = atoi(argv[1]);
		if ( v >= MAX_VALUE ) {
			printf("The maximun allowed value in this test is %i\n", MAX_VALUE);
			return 1;
		}
	}



	char bin[MAX_VALUE][BITS];
	int i;
	for ( i=0; i<MAX_VALUE; i++ ){
		int j;
		for (j=0;j<BITS; j++){
			char c;
			c = i & (int) pow(2,j) ? '1' : '0';
			bin[i][BITS-1-j]=c;
		};
	};

	union {
		uint32_t i;
		unsigned char c[32];
	} t;

	t.i = v;
	char s[33];
	sprintf(s,"%.16s", bin[v]);
	char intv[21];
	intv[0]=0;
	printf("int: 0x%04X: ", t.i);
	for ( i=0; i<BITS/4; i++ ){
		sprintf(intv, "%.*s%.4s ", strlen(intv), intv, (char*) &s[i*4]);
	}
	printf("%s\n", intv);
	sprintf(s,"%.8s%.8s"
			, &(bin[t.c[0]])[8]
			, &(bin[t.c[1]])[8]);
	printf("BIN: 0x%02X%02X: ", t.c[0], t.c[1]);
	for ( i=0; i<BITS/4; i++ ){
		printf("%.4s ", (char*) &s[i*4]);
	}

	return 0;
}
