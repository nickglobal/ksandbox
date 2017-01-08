#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>



int main()
{
	int i;
	FILE *file;
	printf("Create the file.\n");

	file = fopen("test.bin", "w");
	if (!file)
		{
		printf("Error creating the file!\n");
		return -1;
		}

	for (i=0; i<1*1024*16; i++)
	{
		fwrite(&i, 4, 1, file);
	}

   fsync(fileno(file));
	fclose(file);

	return -1;
}
