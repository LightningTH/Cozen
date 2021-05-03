#ifndef NEKISAHLOTH_LIB_ENCRYPTION_TWOFISH
#define NEKISAHLOTH_LIB_ENCRYPTION_TWOFISH

#include <unistd.h>
#include <stdint.h>
#include "EncryptionBase.h"

//copied from the Twofish reference at https://www.schneier.com/academic/twofish/download/

/***************************************************************************
	PLATFORM.H	-- Platform-specific defines for TWOFISH code

	Submitters:
		Bruce Schneier, Counterpane Systems
		Doug Whiting,	Hi/fn
		John Kelsey,	Counterpane Systems
		Chris Hall,		Counterpane Systems
		David Wagner,	UC Berkeley
			
	Code Author:		Doug Whiting,	Hi/fn
		
	Version  1.00		April 1998
		
	Copyright 1998, Hi/fn and Counterpane Systems.  All rights reserved.
		
	Notes:
		*	Tab size is set to 4 characters in this file

***************************************************************************/

/* use intrinsic rotate if possible */
#define	ROL(x,n) (((x) << ((n) & 0x1F)) | ((x) >> (32-((n) & 0x1F))))
#define	ROR(x,n) (((x) >> ((n) & 0x1F)) | ((x) << (32-((n) & 0x1F))))

#define		LittleEndian		1		/* e.g., 1 for Pentium, 0 for 68K */
#define		ALIGN32				0		/* need dword alignment? (no for Pentium) */

#define		Bswap(x)			(x)		/* NOP for little-endian machines */
#define		ADDR_XOR			0		/* NOP for little-endian machines */

/*	Macros for extracting bytes from dwords (correct for endianness) */
#define	_b(x,N)	(((BYTE *)&x)[((N) & 3) ^ ADDR_XOR]) /* pick bytes out of a dword */

#define		b0(x)			_b(x,0)		/* extract LSB of DWORD */
#define		b1(x)			_b(x,1)
#define		b2(x)			_b(x,2)
#define		b3(x)			_b(x,3)		/* extract MSB of DWORD */

/* aes.h */

/* ---------- See examples at end of this file for typical usage -------- */

/* AES Cipher header file for ANSI C Submissions
	Lawrence E. Bassham III
	Computer Security Division
	National Institute of Standards and Technology

	This sample is to assist implementers developing to the
Cryptographic API Profile for AES Candidate Algorithm Submissions.
Please consult this document as a cross-reference.
	
	ANY CHANGES, WHERE APPROPRIATE, TO INFORMATION PROVIDED IN THIS FILE
MUST BE DOCUMENTED. CHANGES ARE ONLY APPROPRIATE WHERE SPECIFIED WITH
THE STRING "CHANGE POSSIBLE". FUNCTION CALLS AND THEIR PARAMETERS
CANNOT BE CHANGED. STRUCTURES CAN BE ALTERED TO ALLOW IMPLEMENTERS TO
INCLUDE IMPLEMENTATION SPECIFIC INFORMATION.
*/

/*	Defines:
		Add any additional defines you need
*/

#define 	DIR_ENCRYPT 	0 		/* Are we encrpyting? */
#define 	DIR_DECRYPT 	1 		/* Are we decrpyting? */
#define 	MODE_ECB 		1 		/* Are we ciphering in ECB mode? */
#define 	MODE_CBC 		2 		/* Are we ciphering in CBC mode? */
#define 	MODE_CFB1 		3 		/* Are we ciphering in 1-bit CFB mode? */

#define 	TRUE 			1
#define 	FALSE 			0

#define 	BAD_KEY_DIR 		-1	/* Key direction is invalid (unknown value) */
#define 	BAD_KEY_MAT 		-2	/* Key material not of correct length */
#define 	BAD_KEY_INSTANCE 	-3	/* Key passed is not valid */
#define 	BAD_CIPHER_MODE 	-4 	/* Params struct passed to cipherInit invalid */
#define 	BAD_CIPHER_STATE 	-5 	/* Cipher in wrong state (e.g., not initialized) */

/* CHANGE POSSIBLE: inclusion of algorithm specific defines */
/* TWOFISH specific definitions */
#define		MAX_KEY_SIZE		64	/* # of ASCII chars needed to represent a key */
#define		MAX_IV_SIZE			16	/* # of bytes needed to represent an IV */
#define		BAD_INPUT_LEN		-6	/* inputLen not a multiple of block size */
#define		BAD_PARAMS			-7	/* invalid parameters */
#define		BAD_IV_MAT			-8	/* invalid IV text */
#define		BAD_ENDIAN			-9	/* incorrect endianness define */
#define		BAD_ALIGN32			-10	/* incorrect 32-bit alignment */

#define		BLOCK_SIZE			128	/* number of bits per block */
#define		MAX_ROUNDS			 16	/* max # rounds (for allocating subkey array) */
#define		ROUNDS_128			 16	/* default number of rounds for 128-bit keys*/
#define		ROUNDS_192			 16	/* default number of rounds for 192-bit keys*/
#define		ROUNDS_256			 16	/* default number of rounds for 256-bit keys*/
#define		MAX_KEY_BITS		256	/* max number of bits of key */
#define		MIN_KEY_BITS		128	/* min number of bits of key (zero pad) */
#define		VALID_SIG	 0x48534946	/* initialization signature ('FISH') */
#define		MCT_OUTER			400	/* MCT outer loop */
#define		MCT_INNER		  10000	/* MCT inner loop */
#define		REENTRANT			  1	/* nonzero forces reentrant code (slightly slower) */

#define		INPUT_WHITEN		0	/* subkey array indices */
#define		OUTPUT_WHITEN		( INPUT_WHITEN + BLOCK_SIZE/32)
#define		ROUND_SUBKEYS		(OUTPUT_WHITEN + BLOCK_SIZE/32)	/* use 2 * (# rounds) */
#define		TOTAL_SUBKEYS		(ROUND_SUBKEYS + 2*MAX_ROUNDS)

/* Typedefs:
	Typedef'ed data storage elements. Add any algorithm specific
	parameters at the bottom of the structs as appropriate.
*/

typedef unsigned char BYTE;
typedef	unsigned int DWORD;		/* 32-bit unsigned quantity */
typedef DWORD fullSbox[4][256];

/* The structure for key information */
typedef struct 
	{
	BYTE direction;					/* Key used for encrypting or decrypting? */
#if ALIGN32
	BYTE dummyAlign[3];				/* keep 32-bit alignment */
#endif
	int  keyLen;					/* Length of the key */
	char keyMaterial[MAX_KEY_SIZE+4];/* Raw key data in ASCII */

	/* Twofish-specific parameters: */
	DWORD keySig;					/* set to VALID_SIG by makeKey() */
	int	  numRounds;				/* number of rounds in cipher */
	DWORD key32[MAX_KEY_BITS/32];	/* actual key bits, in dwords */
	DWORD sboxKeys[MAX_KEY_BITS/64];/* key bits used for S-boxes */
	DWORD subKeys[TOTAL_SUBKEYS];	/* round subkeys, input/output whitening bits */
#if REENTRANT
	fullSbox sBox8x32;				/* fully expanded S-box */
  #if defined(COMPILE_KEY) && defined(USE_ASM)
#undef	VALID_SIG
#define	VALID_SIG	 0x504D4F43		/* 'COMP':  C is compiled with -DCOMPILE_KEY */
	DWORD cSig1;					/* set after first "compile" (zero at "init") */
	void *encryptFuncPtr;			/* ptr to asm encrypt function */
	void *decryptFuncPtr;			/* ptr to asm decrypt function */
	DWORD codeSize;					/* size of compiledCode */
	DWORD cSig2;					/* set after first "compile" */
	BYTE  compiledCode[5000];		/* make room for the code itself */
  #endif
#endif
	} keyInstance;

/* The structure for cipher information */
typedef struct 
	{
	BYTE  mode;						/* MODE_ECB, MODE_CBC, or MODE_CFB1 */
#if ALIGN32
	BYTE dummyAlign[3];				/* keep 32-bit alignment */
#endif
	BYTE  IV[MAX_IV_SIZE];			/* CFB1 iv bytes  (CBC uses iv32) */

	/* Twofish-specific parameters: */
	DWORD cipherSig;				/* set to VALID_SIG by cipherInit() */
	DWORD iv32[BLOCK_SIZE/32];		/* CBC IV bytes arranged as dwords */
	} cipherInstance;

#define		CONST				/* helpful C++ syntax sugar, NOP for ANSI C */

#if BLOCK_SIZE == 128			/* optimize block copies */
#define		Copy1(d,s,N)	((DWORD *)(d))[N] = ((DWORD *)(s))[N]
#define		BlockCopy(d,s)	{ Copy1(d,s,0);Copy1(d,s,1);Copy1(d,s,2);Copy1(d,s,3); }
#else
#define		BlockCopy(d,s)	{ memcpy(d,s,BLOCK_SIZE/8); }
#endif

/***************************************************************************
	TABLE.H	-- Tables, macros, constants for Twofish S-boxes and MDS matrix

	Submitters:
		Bruce Schneier, Counterpane Systems
		Doug Whiting,	Hi/fn
		John Kelsey,	Counterpane Systems
		Chris Hall,		Counterpane Systems
		David Wagner,	UC Berkeley
			
	Code Author:		Doug Whiting,	Hi/fn
		
	Version  1.00		April 1998
		
	Copyright 1998, Hi/fn and Counterpane Systems.  All rights reserved.
		
	Notes:
		*	Tab size is set to 4 characters in this file
		*	These definitions should be used in optimized and unoptimized
			versions to insure consistency.

***************************************************************************/

/* for computing subkeys */
#define	SK_STEP			0x02020202u
#define	SK_BUMP			0x01010101u
#define	SK_ROTL			9

/* Reed-Solomon code parameters: (12,8) reversible code
	g(x) = x**4 + (a + 1/a) x**3 + a x**2 + (a + 1/a) x + 1
   where a = primitive root of field generator 0x14D */
#define	RS_GF_FDBK		0x14D		/* field generator */
#define	RS_rem(x)		\
	{ BYTE  b  = (BYTE) (x >> 24);											 \
	  DWORD g2 = ((b << 1) ^ ((b & 0x80) ? RS_GF_FDBK : 0 )) & 0xFF;		 \
	  DWORD g3 = ((b >> 1) & 0x7F) ^ ((b & 1) ? RS_GF_FDBK >> 1 : 0 ) ^ g2 ; \
	  x = (x << 8) ^ (g3 << 24) ^ (g2 << 16) ^ (g3 << 8) ^ b;				 \
	}

/*	Macros for the MDS matrix
*	The MDS matrix is (using primitive polynomial 169):
*      01  EF  5B  5B
*      5B  EF  EF  01
*      EF  5B  01  EF
*      EF  01  EF  5B
*----------------------------------------------------------------
* More statistical properties of this matrix (from MDS.EXE output):
*
* Min Hamming weight (one byte difference) =  8. Max=26.  Total =  1020.
* Prob[8]:      7    23    42    20    52    95    88    94   121   128    91
*             102    76    41    24     8     4     1     3     0     0     0
* Runs[8]:      2     4     5     6     7     8     9    11
* MSBs[8]:      1     4    15     8    18    38    40    43
* HW= 8: 05040705 0A080E0A 14101C14 28203828 50407050 01499101 A080E0A0 
* HW= 9: 04050707 080A0E0E 10141C1C 20283838 40507070 80A0E0E0 C6432020 07070504 
*        0E0E0A08 1C1C1410 38382820 70705040 E0E0A080 202043C6 05070407 0A0E080E 
*        141C101C 28382038 50704070 A0E080E0 4320C620 02924B02 089A4508 
* Min Hamming weight (two byte difference) =  3. Max=28.  Total = 390150.
* Prob[3]:      7    18    55   149   270   914  2185  5761 11363 20719 32079
*           43492 51612 53851 52098 42015 31117 20854 11538  6223  2492  1033
* MDS OK, ROR:   6+  7+  8+  9+ 10+ 11+ 12+ 13+ 14+ 15+ 16+
*               17+ 18+ 19+ 20+ 21+ 22+ 23+ 24+ 25+ 26+
*/
#define	MDS_GF_FDBK		0x169	/* primitive polynomial for GF(256)*/
#define	LFSR1(x) ( ((x) >> 1)  ^ (((x) & 0x01) ?   MDS_GF_FDBK/2 : 0))
#define	LFSR2(x) ( ((x) >> 2)  ^ (((x) & 0x02) ?   MDS_GF_FDBK/2 : 0)  \
							   ^ (((x) & 0x01) ?   MDS_GF_FDBK/4 : 0))

#define	Mx_1(x) ((DWORD)  (x))		/* force result to dword so << will work */
#define	Mx_X(x) ((DWORD) ((x) ^            LFSR2(x)))	/* 5B */
#define	Mx_Y(x) ((DWORD) ((x) ^ LFSR1(x) ^ LFSR2(x)))	/* EF */

#define	M00		Mul_1
#define	M01		Mul_Y
#define	M02		Mul_X
#define	M03		Mul_X

#define	M10		Mul_X
#define	M11		Mul_Y
#define	M12		Mul_Y
#define	M13		Mul_1

#define	M20		Mul_Y
#define	M21		Mul_X
#define	M22		Mul_1
#define	M23		Mul_Y

#define	M30		Mul_Y
#define	M31		Mul_1
#define	M32		Mul_Y
#define	M33		Mul_X

#define	Mul_1	Mx_1
#define	Mul_X	Mx_X
#define	Mul_Y	Mx_Y

/*	Define the fixed p0/p1 permutations used in keyed S-box lookup.  
	By changing the following constant definitions for P_ij, the S-boxes will
	automatically get changed in all the Twofish source code. Note that P_i0 is
	the "outermost" 8x8 permutation applied.  See the f32() function to see
	how these constants are to be  used.
*/
#define	P_00	1					/* "outermost" permutation */
#define	P_01	0
#define	P_02	0
#define	P_03	(P_01^1)			/* "extend" to larger key sizes */
#define	P_04	1

#define	P_10	0
#define	P_11	0
#define	P_12	1
#define	P_13	(P_11^1)
#define	P_14	0

#define	P_20	1
#define	P_21	1
#define	P_22	0
#define	P_23	(P_21^1)
#define	P_24	0

#define	P_30	0
#define	P_31	1
#define	P_32	1
#define	P_33	(P_31^1)
#define	P_34	1

#define	p8(N)	P8x8[P_##N]			/* some syntax shorthand */

/* fixed 8x8 permutation S-boxes */

/***********************************************************************
*  07:07:14  05/30/98  [4x4]  TestCnt=256. keySize=128. CRC=4BD14D9E.
* maxKeyed:  dpMax = 18. lpMax =100. fixPt =  8. skXor =  0. skDup =  6. 
* log2(dpMax[ 6..18])=   --- 15.42  1.33  0.89  4.05  7.98 12.05
* log2(lpMax[ 7..12])=  9.32  1.01  1.16  4.23  8.02 12.45
* log2(fixPt[ 0.. 8])=  1.44  1.44  2.44  4.06  6.01  8.21 11.07 14.09 17.00
* log2(skXor[ 0.. 0])
* log2(skDup[ 0.. 6])=   ---  2.37  0.44  3.94  8.36 13.04 17.99
***********************************************************************/
CONST BYTE P8x8[2][256]=
	{
/*  p0:   */
/*  dpMax      = 10.  lpMax      = 64.  cycleCnt=   1  1  1  0.         */
/* 817D6F320B59ECA4.ECB81235F4A6709D.BA5E6D90C8F32471.D7F4126E9B3085CA. */
/* Karnaugh maps:
*  0111 0001 0011 1010. 0001 1001 1100 1111. 1001 1110 0011 1110. 1101 0101 1111 1001. 
*  0101 1111 1100 0100. 1011 0101 0010 0000. 0101 1000 1100 0101. 1000 0111 0011 0010. 
*  0000 1001 1110 1101. 1011 1000 1010 0011. 0011 1001 0101 0000. 0100 0010 0101 1011. 
*  0111 0100 0001 0110. 1000 1011 1110 1001. 0011 0011 1001 1101. 1101 0101 0000 1100. 
*/
	{
	0xA9, 0x67, 0xB3, 0xE8, 0x04, 0xFD, 0xA3, 0x76, 
	0x9A, 0x92, 0x80, 0x78, 0xE4, 0xDD, 0xD1, 0x38, 
	0x0D, 0xC6, 0x35, 0x98, 0x18, 0xF7, 0xEC, 0x6C, 
	0x43, 0x75, 0x37, 0x26, 0xFA, 0x13, 0x94, 0x48, 
	0xF2, 0xD0, 0x8B, 0x30, 0x84, 0x54, 0xDF, 0x23, 
	0x19, 0x5B, 0x3D, 0x59, 0xF3, 0xAE, 0xA2, 0x82, 
	0x63, 0x01, 0x83, 0x2E, 0xD9, 0x51, 0x9B, 0x7C, 
	0xA6, 0xEB, 0xA5, 0xBE, 0x16, 0x0C, 0xE3, 0x61, 
	0xC0, 0x8C, 0x3A, 0xF5, 0x73, 0x2C, 0x25, 0x0B, 
	0xBB, 0x4E, 0x89, 0x6B, 0x53, 0x6A, 0xB4, 0xF1, 
	0xE1, 0xE6, 0xBD, 0x45, 0xE2, 0xF4, 0xB6, 0x66, 
	0xCC, 0x95, 0x03, 0x56, 0xD4, 0x1C, 0x1E, 0xD7, 
	0xFB, 0xC3, 0x8E, 0xB5, 0xE9, 0xCF, 0xBF, 0xBA, 
	0xEA, 0x77, 0x39, 0xAF, 0x33, 0xC9, 0x62, 0x71, 
	0x81, 0x79, 0x09, 0xAD, 0x24, 0xCD, 0xF9, 0xD8, 
	0xE5, 0xC5, 0xB9, 0x4D, 0x44, 0x08, 0x86, 0xE7, 
	0xA1, 0x1D, 0xAA, 0xED, 0x06, 0x70, 0xB2, 0xD2, 
	0x41, 0x7B, 0xA0, 0x11, 0x31, 0xC2, 0x27, 0x90, 
	0x20, 0xF6, 0x60, 0xFF, 0x96, 0x5C, 0xB1, 0xAB, 
	0x9E, 0x9C, 0x52, 0x1B, 0x5F, 0x93, 0x0A, 0xEF, 
	0x91, 0x85, 0x49, 0xEE, 0x2D, 0x4F, 0x8F, 0x3B, 
	0x47, 0x87, 0x6D, 0x46, 0xD6, 0x3E, 0x69, 0x64, 
	0x2A, 0xCE, 0xCB, 0x2F, 0xFC, 0x97, 0x05, 0x7A, 
	0xAC, 0x7F, 0xD5, 0x1A, 0x4B, 0x0E, 0xA7, 0x5A, 
	0x28, 0x14, 0x3F, 0x29, 0x88, 0x3C, 0x4C, 0x02, 
	0xB8, 0xDA, 0xB0, 0x17, 0x55, 0x1F, 0x8A, 0x7D, 
	0x57, 0xC7, 0x8D, 0x74, 0xB7, 0xC4, 0x9F, 0x72, 
	0x7E, 0x15, 0x22, 0x12, 0x58, 0x07, 0x99, 0x34, 
	0x6E, 0x50, 0xDE, 0x68, 0x65, 0xBC, 0xDB, 0xF8, 
	0xC8, 0xA8, 0x2B, 0x40, 0xDC, 0xFE, 0x32, 0xA4, 
	0xCA, 0x10, 0x21, 0xF0, 0xD3, 0x5D, 0x0F, 0x00, 
	0x6F, 0x9D, 0x36, 0x42, 0x4A, 0x5E, 0xC1, 0xE0
	},
/*  p1:   */
/*  dpMax      = 10.  lpMax      = 64.  cycleCnt=   2  0  0  1.         */
/* 28BDF76E31940AC5.1E2B4C376DA5F908.4C75169A0ED82B3F.B951C3DE647F208A. */
/* Karnaugh maps:
*  0011 1001 0010 0111. 1010 0111 0100 0110. 0011 0001 1111 0100. 1111 1000 0001 1100. 
*  1100 1111 1111 1010. 0011 0011 1110 0100. 1001 0110 0100 0011. 0101 0110 1011 1011. 
*  0010 0100 0011 0101. 1100 1000 1000 1110. 0111 1111 0010 0110. 0000 1010 0000 0011. 
*  1101 1000 0010 0001. 0110 1001 1110 0101. 0001 0100 0101 0111. 0011 1011 1111 0010. 
*/
	{
	0x75, 0xF3, 0xC6, 0xF4, 0xDB, 0x7B, 0xFB, 0xC8, 
	0x4A, 0xD3, 0xE6, 0x6B, 0x45, 0x7D, 0xE8, 0x4B, 
	0xD6, 0x32, 0xD8, 0xFD, 0x37, 0x71, 0xF1, 0xE1, 
	0x30, 0x0F, 0xF8, 0x1B, 0x87, 0xFA, 0x06, 0x3F, 
	0x5E, 0xBA, 0xAE, 0x5B, 0x8A, 0x00, 0xBC, 0x9D, 
	0x6D, 0xC1, 0xB1, 0x0E, 0x80, 0x5D, 0xD2, 0xD5, 
	0xA0, 0x84, 0x07, 0x14, 0xB5, 0x90, 0x2C, 0xA3, 
	0xB2, 0x73, 0x4C, 0x54, 0x92, 0x74, 0x36, 0x51, 
	0x38, 0xB0, 0xBD, 0x5A, 0xFC, 0x60, 0x62, 0x96, 
	0x6C, 0x42, 0xF7, 0x10, 0x7C, 0x28, 0x27, 0x8C, 
	0x13, 0x95, 0x9C, 0xC7, 0x24, 0x46, 0x3B, 0x70, 
	0xCA, 0xE3, 0x85, 0xCB, 0x11, 0xD0, 0x93, 0xB8, 
	0xA6, 0x83, 0x20, 0xFF, 0x9F, 0x77, 0xC3, 0xCC, 
	0x03, 0x6F, 0x08, 0xBF, 0x40, 0xE7, 0x2B, 0xE2, 
	0x79, 0x0C, 0xAA, 0x82, 0x41, 0x3A, 0xEA, 0xB9, 
	0xE4, 0x9A, 0xA4, 0x97, 0x7E, 0xDA, 0x7A, 0x17, 
	0x66, 0x94, 0xA1, 0x1D, 0x3D, 0xF0, 0xDE, 0xB3, 
	0x0B, 0x72, 0xA7, 0x1C, 0xEF, 0xD1, 0x53, 0x3E, 
	0x8F, 0x33, 0x26, 0x5F, 0xEC, 0x76, 0x2A, 0x49, 
	0x81, 0x88, 0xEE, 0x21, 0xC4, 0x1A, 0xEB, 0xD9, 
	0xC5, 0x39, 0x99, 0xCD, 0xAD, 0x31, 0x8B, 0x01, 
	0x18, 0x23, 0xDD, 0x1F, 0x4E, 0x2D, 0xF9, 0x48, 
	0x4F, 0xF2, 0x65, 0x8E, 0x78, 0x5C, 0x58, 0x19, 
	0x8D, 0xE5, 0x98, 0x57, 0x67, 0x7F, 0x05, 0x64, 
	0xAF, 0x63, 0xB6, 0xFE, 0xF5, 0xB7, 0x3C, 0xA5, 
	0xCE, 0xE9, 0x68, 0x44, 0xE0, 0x4D, 0x43, 0x69, 
	0x29, 0x2E, 0xAC, 0x15, 0x59, 0xA8, 0x0A, 0x9E, 
	0x6E, 0x47, 0xDF, 0x34, 0x35, 0x6A, 0xCF, 0xDC, 
	0x22, 0xC9, 0xC0, 0x9B, 0x89, 0xD4, 0xED, 0xAB, 
	0x12, 0xA2, 0x0D, 0x52, 0xBB, 0x02, 0x2F, 0xA9, 
	0xD7, 0x61, 0x1E, 0xB4, 0x50, 0x04, 0xF6, 0xC2, 
	0x16, 0x25, 0x86, 0x56, 0x55, 0x09, 0xBE, 0x91
	}
};

class Twofish : public EncryptionBase
{
	public:	
		Twofish(ConnectionBase *Connection);
        ~Twofish();

        size_t SetConfig(size_t ConfigID, void *Data);
		size_t EncryptData(size_t Len);
		size_t DecryptData(size_t Len, bool LastBlock);

	private:
        cipherInstance *_cipher;
        keyInstance *_key;
        char _IV[BLOCK_SIZE / 8];
        size_t _BitSize;
        size_t _Mode;
};

#endif