/*
 * types.h
 *
 *  Created on: Nov 1, 2013
 *      Author: dandy
 */

#ifndef type_H
#define type_H

#ifndef NULL
#define	NULL	((void *)0)
#endif

#define FALSE			0
#define TRUE			1//!FALSE
#define	NO				0
#define	YES				1
#define ON				1
#define OFF		 		0
#define	FAILURE			0	   
//#define	ERROR			0
//#define	SUCCESS			1//!FAILURE
#define	NEGATIVE		0
#define	POSITIVE		1

//****************************************************************************


typedef enum {
	Ok_code = 0,
	Bad_code = 1
} OkBad_enum;

typedef float                    f32;
typedef unsigned long long       u64;
typedef unsigned long            u32;
typedef unsigned short           u16;
typedef unsigned char            u08;
typedef unsigned char            u8;

typedef long long       		 s64;
typedef signed long              s32;
typedef signed short             s16;
typedef signed char              s08;
typedef signed char              s8;
	
//	typedef volatile unsigned long   vu32;
//	typedef volatile unsigned short  vu16;
//	typedef volatile unsigned char   vu8;
	
//	typedef volatile signed long     vs32;
//	typedef volatile signed short    vs16;
//	typedef volatile signed char     vs8;


//****************************************************************************
typedef void (*pFuncVoid_typedef)(void);		//óêàçàòåëü íà ôóíêöèþ
typedef int (*piFuncVoid_typedef)(void);		//óêàçàòåëü íà ôóíêöèþ
typedef int (*intFuncVoid_typedef)(void);		//óêàçàòåëü íà ôóíêöèþ
typedef	void *(*ppvFuncVoid_typedef)(void);		//óêàçàòåëü íà ôóíêö. êîòîðàÿ âîçâðàùàåò óêàçàòåëü íà ôóíê


typedef void (*pFuncInt_typedef)(int);		//óêàçàòåëü íà ôóíêöèþ
//****************************************************************************
#define	GetOffset(Pnt1, Pnt2)		((u32)(Pnt1) - (u32)(Pnt2))
#define	IntMax(val1, val2)			((val1 > val2) ? val1 : val2)
#define	IntMin(val1, val2)			((val1 < val2) ? val1 : val2)
#define absMacro(val)  				( (val > 0)	? val : (-val))
//**************************************************************************
#define	Pi				((f32)3.1415926535897932384626433832795)
#define _2PI			((f32)(Pi * 2.0))
#define _4PI			((f32)(Pi * 4.0))
#define _2PI_div_3		((f32)(Pi * 2.0/3.0))
#define _4PI_div_3		((f32)(Pi * 4.0/3.0))
#define	SQRT_3_DIV2		((f32)0.86602540378443864676372317075294)
#define	SQRT_2			((f32)1.4142135623730950488016887242097)
#define	SQRT_3			((f32)1.7320508075688772935274463415059)
#define	One_div_SQRT_2	((f32)(1.0/1.4142135623730950488016887242097))
#define	INF_ADR			((void *)0xFFFFFFFF)
#define	SQRT2_DIV_SQRT3	((f32)0.80011020107325135198078994570679)
//**************************************************************************
#endif
