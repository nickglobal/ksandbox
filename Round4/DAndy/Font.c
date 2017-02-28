/*
 * Font.c
 *
 *  Created on: Sep 6, 2013
 *      Author: dandy
 */

#include "Font.h"
#include "Point.h"
//#include "FB_Graph.h"


u16 Font_GetCharOffset(char c){
	u16 offset = 0;
	FONT_INFO *pFont = Graphic_GetFont();
	u08 FontNbRows = (pFont->Height / 8);
	u08 *pCharWinth = pFont->pChar_Widths;

	if(c >= pFont->First_Char)
		c -= pFont->First_Char;

	while(c--){
		offset += (*pCharWinth) * FontNbRows;
		pCharWinth++;
	}
	return offset;
}
//----------------------------------------------------------


u08 *Font_GetCharData(char c){
	volatile FONT_INFO *pFont = Graphic_GetFont();

	return (u08 *)(pFont->pData + Font_GetCharOffset(c));
}
//----------------------------------------------------------

u08 Font_GetCharWidth(char c){
	FONT_INFO *pFont = Graphic_GetFont();
	if(c >= pFont->First_Char)
		c -= pFont->First_Char;

	return *(pFont->pChar_Widths + c);
}
//----------------------------------------------------------

void Font_DrawOneByte(u08 x, u08 y, u08 byte){
	u08 bit = 0;
	u16 charColor = Graphic_GetForeground();

	for(bit = 0; bit < 8; bit++){
        //if(byte & (1 << (8 - bit))){
		if(byte & (1 << (bit))){
            charColor = Graphic_GetForeground();
		}
		else{
            charColor = Graphic_GetBackground();
		}
		Graphic_setPoint_(x, y + bit, charColor);	// draw foreground color
	}
}
//----------------------------------------------------------


void Font_Dispchar(char c){
	u16 x, y;
	volatile u08 *pCurChar = 0, *ptmpChar = 0;
	u08 CharWidth = 0;
	FONT_INFO *pFont = Graphic_GetFont();
	_Point pos = Graphic_GetPos();
	u08 data = 0;

	u08 FontNbRows = (pFont->Height / 8);

	CharWidth 	= Font_GetCharWidth(c);
	pCurChar 	= Font_GetCharData(c);

    for( y = 0; y < FontNbRows; y++ ){
	    for( x = 0; x < CharWidth; x++){

			//ptmpChar = pCurChar + x + (y * FontNbRows);
//			ptmpChar = pCurChar + x + (y * (FontNbRows - 1) * CharWidth);
			ptmpChar = pCurChar + x + (y * CharWidth);
			data = *ptmpChar;
			Font_DrawOneByte(pos.X + x, pos.Y + (y * 8), *ptmpChar);
		}
	}

	pos.X += CharWidth + 1;
	Graphic_SetPos(pos);
}
//----------------------------------------------------------



void Font_DispString(const char *s){

	while (*s){
		Font_Dispchar(*s++);
	}
}
//----------------------------------------------------------


