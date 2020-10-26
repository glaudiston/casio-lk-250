#include <stdint.h>

// https://tools.ietf.org/html/rfc3174
// constant explanations ref:
//   https://crypto.stackexchange.com/questions/10829/why-initialize-sha1-with-specific-buffer?newreg=e95094e68b48405b9d467c14741b91eb
//
// 0111 0110 0101 0100 0011 0010 0001 0000 = BE 0x76543210 = LE 0x67452301
// 1111 1110 1101 1100 1011 1010 1001 1000 = BE 0xFEDCBA98 = LE 0xEFCDAB89
// 1000 1001 1010 1011 1100 1101 1110 1111 = BE 0x89ABCDEF = LE 0x98BADCFE
// 0000 0001 0010 0011 0100 0101 0110 0111 = BE 0x01234567 = LE 0x10325476
//
uint32_t H[5] =
{
	0x67452301,
	0xEFCDAB89,
	0x98BADCFE,
	0x10325476,
	0xC3D2E1F0
};

uint32_t rotl( uint32_t x, int shift )
{
	return ( x << shift ) | (x >> (sizeof(x)*8 - shift));
};

uint32_t round_func( uint32_t b, uint32_t c, uint32_t d, int round_num )
{
	if ( round_num <= 19 )
	{
		return (b & c) | ((~b) & d);
	}
	else if (round_num <= 39 )
	{
		return ( b ^ c ^ d );
	}
	else if ( round_num <= 59 )
	{
		return ( b & c ) | ( b & d ) | ( c & d );
	}
	else
	{
		return ( b ^ c ^ d );
	}
}

uint32_t k_for_round( int round_num )
{
	if ( round_num <= 19 )
	{
		return 0x5a827999;
	}
	else if ( round_num <= 39 )
	{
		return 0x6ed9eba1;
	}
	else if ( round_num <= 59 )
	{
		return 0x8f1bbcdc;
	}
	else
	{
		return 0xca62c1d6;
	}
}

int pad(uint8_t * block, uint8_t * extra_block, int blocksize, int filesize)
{
	int two_blocks = 0;
	// l is block size in bits
	uint64_t l = (uint64_t) filesize * 8;
	if ( blocksize <= 55 )
	{
		block[blocksize] = 0x80;
		int i;
		for ( i = 0; i < 8; i++ )
		{
			block[56 + i] = (l >> (56 - (8 * i)));
		}
	}
	else
	{
		two_blocks = 1;
		if (blocksize < 63)
			block[blocksize] = 0x80;
		else
			extra_block[0] = 0x80;

		int i;
		for ( i = 0; i < 8; i++ )
		{
			extra_block[56+i] = (l >> (56 - (8 * i)));
		}
	}
	return two_blocks;
}

void calc_block_sha1(uint8_t *block)
{
	static uint32_t w[80] = {0x00000000};
	int i;
	for ( i=0; i < 16; i++ )
	{
		int offset = (i * 4);
		w[i] = 	block[offset] 		<< 24 |
			block[offset + 1] 	<< 16 |
			block[offset + 2]	<< 8  |
			block[offset + 3];
	}

	for ( i = 16; i < 80; i++)
	{
		uint32_t tmp = (w[i-3] ^w[i-8] ^ w[i-14] ^ w[i-16]);
		w[i] = rotl(tmp, 1);
	}

	uint32_t a = H[0];
	uint32_t b = H[1];
	uint32_t c = H[2];
	uint32_t d = H[3];
	uint32_t e = H[4];

	for ( i = 0; i < 80; i++ )
	{
		uint32_t tmp = rotl(a, 5) + round_func(b,c,d,i) + e + w[i] + k_for_round(i);
		e = d;
		d = c;
		c = rotl(b,30);
		b = a;
		a = tmp;
	}

	H[0] = H[0] + a;
	H[1] = H[1] + b;
	H[2] = H[2] + c;
	H[3] = H[3] + d;
	H[4] = H[4] + e;
}

/**
 * This function calculate message digest sha-1 from source_data argument
 * and put result hex string in sha1sum argument
 * */
int sha1(char * source_data, int source_data_len, char * sha1sum[40]){
	int errcode = 0;
	// read each 512 bits (64 bytes) and do calc block
	char block[64];
	int i,bs;
	for ( i=0; i < source_data_len; i=i+64 ) {
		bs=64-(source_data_len-i > 64 ? 64 - (source_data_len-i) : 0);
		memcpy(block, source_data[i], bs);
		calc_block_sha1(block);
	}
	return errcode;
}
