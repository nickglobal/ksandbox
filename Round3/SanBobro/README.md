Little video example "how it works" you can find on the following link:
      https://youtu.be/xDkwDnMeIRA
      
Module parameters are:
"off" - int, default = 1, 
	means: 1 - driver turns off the display during unloading. 
	       0 - driver doesn't turn off the display during unloading. 
	       It allows not to wait when display turns on each time.

"time" - int, default = 0,
	means: Initial timer time in seconds. Timer will start to tick since this time.
	       Correct values are in [0..999].

"md" - int, default = 4,
	means: mdelay in mseconds between shift 1 row/line when animation moves a numeral(s). 
	       To provide a correct timer running use values in [0..12].