/* This program prints each argument as a separate line
 * on the OLED display */

/* Uncomment this for debug version
 *  (output only 2 lines, output to console instead of OLED)
 *
 * #define DEBUG
 */

#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#define LETTER_H (8)
#define LETTER_W (7)
#define DISP_H (64)
#define DISP_W (128)

/* value of pixels in file */
#ifdef DEBUG
    #define BLACK (' ')
    #define WHITE ('*')
#else
    #define BLACK (0)
    #define WHITE (0xFF)
#endif

#define DISP_FILE ("/dev/pkdisp")


/* font */
struct letter
{
	char letter[LETTER_H*LETTER_W+1];
};
#include "font.c"

/* screen buffer. One char per one pixel */
static char buf[DISP_W*DISP_H] = {0};



/**************** draw_letter *************************/
/* draw one letter in buffer */

int draw_letter(int lx, int ly, char ch)
{
   int x,y;

   if ((lx>=(DISP_W-LETTER_W)) || (ly>=(DISP_H-LETTER_H)))
   {
      printf ("cannot print \'%c\' out of screen (x=%d, y=%d)", ch, lx, ly);
      return -1;
   }

   /* check symbol */
   if ((ch < ' ') || (ch > '~'))
   {
	   /* wrong char. print '-' instead */
	   ch = '-';
   }

   for (y=0; y<LETTER_H; y++)
   {
      for (x=0; x<LETTER_W; x++)
      {
         buf[(y+ly)*DISP_W+x+lx] = alphabet[ch-' '].letter[y*LETTER_W+x] == '*'? WHITE: BLACK;
      }
   }
   return 0;
}

/**************** save_screen *************************/
/* flushes screen to the file/device */
int save_screen (void)
{
#ifndef DEBUG
   int fd = open (DISP_FILE, O_WRONLY);
   if (fd<=0)
   {
      printf ("error open \'%s\': %s\n", DISP_FILE, strerror(errno));
      return -1;
   }
   write (fd, buf, DISP_W*DISP_H);
   close (fd);
#else
  int i;
  for (i=0; i<16; i++)
  {
	 write (1, buf+DISP_W*i, DISP_W);
	 write (1, "\n", 1);
  }
#endif
   return 0;
}

/****************** main entry ************************/
int main(int argc, char *argv[])
{
   int l, num, i, len;

   /* default print */
   char *line = "Hello World";

   if (2 <= argc)
   {
      /* draw each argument as separate line */

      /* check how many lines do we have */
      num = argc - 1;
      if (num >= (DISP_H/LETTER_H))
      {
         printf ("cannot print more lines then display has\n");
         num = DISP_H/LETTER_H;
      }

      /* draw each line */
      for (l=0; l<num; l++)
      {
         line = argv[l+1];
		 len = strlen(line);
		 if (len > (DISP_W/LETTER_W))
		 {
			 /* truncate lines with display width */
			 len = DISP_W/LETTER_W;
		 }
         printf ("draw [%d] \"%s\"\n", len, argv[l+1]);
         for (i=0; i<len; i++)
         {
            draw_letter (LETTER_W*i, LETTER_H*l, line[i]);
         }
      }
   }
   else
   {
      /* default print */
      printf ("draw \"%s\"\n", line);

      for (i=0; i<strlen(line); i++)
      {
         draw_letter (LETTER_W*i, 0, line[i]);
      }
   }

   save_screen();

   return;
}
