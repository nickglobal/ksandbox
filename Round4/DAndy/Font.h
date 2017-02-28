/*
 * Font.h
 *
 *  Created on: Sep 6, 2013
 *      Author: dandy
 */

#ifndef FONT_H_
#define FONT_H_

#include "types.h"


/*
****************************************
*                                      *
*      FONT structures   			   *
*                                      *
****************************************
*/



typedef struct{
	u08	Width;				// in_Pixel_for_fixed_drawing;
	u08	Height;				// in_Pixel_for_all_characters;
	u08	First_Char;
	u08	Char_Count;
	u08	*pChar_Widths;		// for each character the separate width in pixels,
	u08	*pData;				// pointer to bit field of all characters
}FONT_INFO;

/*
typedef struct{
	u08 XSize;
	u08 YSize;
	u08 BytesPerLine;
	const u08 *pData;
}CHARINFO_;


typedef struct{
	u08 XSize;
	u08 XDist;
	u08 BytesPerLine;
	const u08 *pData;
}CHARINFO;

typedef struct{
	u08 XSize;
	u08 YSize;
	u16 XPos;
	u16 YPos;
	u08 XDist;
	const u08 *pData;
}CHARINFO_EXT;

typedef struct FONT_PROP{
	u16 First;                                		// first character
	u16 Last;                                 		// last character
	const CHARINFO *paCharInfo;            			// address of first character
	const struct GUI_FONT_PROP *pNext;       		// pointer to next
}FONT_PROP;

typedef struct FONT_PROP_EXT{
	u16 First;                                      // First character
	u16 Last;                                       // Last character
	const CHARINFO_EXT         *paCharInfo; 		// Address of first character
	const struct FONT_PROP_EXT *pNext;      		// Pointer to next
}FONT_PROP_EXT;

typedef struct {
	const u08 *pData;
	const u08 *pTransData;
//	const FONT_TRANSINFO *pTrans;
	u16 FirstChar;
	u16 LastChar;
	u08 XSize;
	u08 XDist;
	u08 BytesPerLine;
}FONT_MONO;


typedef struct FONT_INFO{
	u16 First;                        	// first character
	u16 Last;                         	// last character
	const CHARINFO* paCharInfo;    		// address of first character
	const struct FONT_INFO* pNext; 		// pointer to next
}FONT_INFO;
*/
/*
****************************************
*                                      *
*      FONT info structure             *
*                                      *
****************************************

This structure is used when retrieving information about a font.
It is designed for future expansion without incompatibilities.
*/
/*
typedef struct {
  u16 Flags;
  u08 Baseline;
  u08 LHeight;     // height of a small lower case character (a,x)
  u08 CHeight;     // height of a small upper case character (A,X)
}FONTINFO;

#define FONTINFO_FLAG_PROP (1<<0)    // Is proportional
#define FONTINFO_FLAG_MONO (1<<1)    // Is monospaced
#define FONTINFO_FLAG_AA   (1<<2)    // Is an antialiased font
#define FONTINFO_FLAG_AA2  (1<<3)    // Is an antialiased font, 2bpp
#define FONTINFO_FLAG_AA4  (1<<4)    // Is an antialiased font, 4bpp

*/

/**********************************************************************
*
*                 FONT Encoding
*
***********************************************************************
*/
/*
typedef int  Font_GetLineDistX(const char *s, u16 Len);
typedef int  Font_GetLineLen(const char *s, u16 MaxLen);
typedef void Font_DispLine(const char *s, u16 Len);

typedef struct{
	Font_GetLineDistX	*pfGetLineDistX;
	Font_GetLineLen		*pfGetLineLen;
	Font_DispLine 		*pfDispLine;
}ENC_APIList;

*/


u08 Font_GetCharWidth(char c);
void Font_DispString(const char *s);

#endif /* FONT_H_ */
