#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <jpeglib.h>

// compilare gcc -o pthreads pthreads.c -lpthread -ljpeg
// rulare ./pthreads <image_in> <image_out>
// ex. ./pthreads in/house.jpg house_line.jpg

typedef struct {
	int width;
	int height;
	unsigned char *data;
} image;

struct interval
{
	int thread_id;
	int start;
	int end;
};

// Gaussian noise reduction (sum /= 16)
int edgeDetectionFilter[3][3] = {{-1, -1, -1},
								 {-1, 8, -1},
								 {-1, -1, -1}};

image in;
image out;
int P = 4;

//Read a given image
void readInput(const char *fileName, image *img)
{
	FILE *input = NULL;
	struct jpeg_decompress_struct info;
	struct jpeg_error_mgr err;

	if (fileName == NULL ||
		(input = fopen(fileName, "rb")) == NULL)
	{
		fprintf(stderr, "can't open %s\n", fileName);
		return;
	}

	//error handler
	info.err = jpeg_std_error(&err); 

  	//init jpeg decompress object
	jpeg_create_decompress(&info);    

	//specify data source
 	jpeg_stdio_src(&info, input);

 	//read the header    
  	jpeg_read_header(&info, TRUE);

  	// start decompression
  	jpeg_start_decompress(&info);

	//get width and height
	img->width = info.output_width;
	img->height = info.output_height;
	unsigned long data_size = (unsigned long) img->width * img->height * 3;
    unsigned char* rowptr[1];
    unsigned char* jdata;

	printf("Input image width and height: %d %d\n", (*img).width, (*img).height);

	img->data = (unsigned char *)malloc(data_size);

	while (info.output_scanline < info.output_height)
	{
	    rowptr[0] = (unsigned char *)img->data 
	    			+ 3 * info.output_width * info.output_scanline; 

	    jpeg_read_scanlines(&info, rowptr, 1);
	}

	//finish decompression
  	jpeg_finish_decompress(&info);

  	//release object
  	jpeg_destroy_decompress(&info);

   	fclose(input);
}


void writeData(const char *fileName, image *img)
{
	FILE *out;
  	JSAMPROW row_pointer[1];
  	int row_stride;
	struct jpeg_compress_struct info;
	struct jpeg_error_mgr jerr; 

	if ((out = fopen(fileName, "wb")) == NULL)
	{
	    fprintf(stderr, "can't open %s\n", fileName);
	    exit(1);
	}

  	//init jpeg decompress object
	jpeg_create_compress(&info);

	//specify data dest
 	jpeg_stdio_dest(&info, out);

	//set width and height
	info.image_width = img->width;
	info.image_height = img->height;
	info.input_components = 3;
    info.in_color_space = JCS_RGB;
    info.err = jpeg_std_error(&jerr); 
	
  	unsigned char* rowptr[1];

	printf("Output image width and height: %d %d\n", (*img).width, (*img).height);

    jpeg_set_defaults(&info);

    jpeg_start_compress(&info, TRUE);
	
	while (info.next_scanline < info.image_height)
	{
	    rowptr[0] = (unsigned char *)img->data
		            + 3 * info.image_width * info.next_scanline; 

	    jpeg_write_scanlines(&info, rowptr, 1);
	}

	jpeg_finish_compress(&info);

	fclose(out);

	jpeg_destroy_compress(&info);
}

//Apply filter on sum x product of neighbours
int computeSum(int row, int column)
{
	int sum = 0;
	int rowIdx = row - 1;
	int colIdx = column - 3;

	for (int i = rowIdx, fi = 0; i < rowIdx + 3; i++, fi++)
	{
		for (int j = colIdx, fj = 0; j < colIdx + 9; j += 3, fj++)
		{
			sum += (int)(in.data[i * 3 * in.width + j]) * edgeDetectionFilter[fi][fj];
		}
	}

	return sum;
}

//Apply filter
void* applyFilter(void *var)
{
	struct interval crtThread = *(struct interval*) var;

	for (int i = crtThread.start; i < crtThread.end; i++)
	{
		for (int j = 0; j < 3 * in.width; j++)
		{
			//Border case
			if (i < 1 || i >= in.height - 1 ||
				j < 3 || j >= 3 * in.width - 3)
			{
				out.data[i *  3 * in.width + j] = in.data[i * 3 * in.width + j];
				continue;
			}

			out.data[i * 3 * in.width + j] = (char)(computeSum(i, j) / 16);
		}
	}
	pthread_exit(NULL);
}


int main(int argc, char * argv[]) {
	pthread_t tid[P];
	int thread_id[P];
	struct interval interval[P];

	readInput(argv[1], &in);

	printf("successfully read input\n");

	// Initialize the threads
	for(int i = 0; i < P; i++)
		thread_id[i] = i;

	// Compute the work interval of each thread
	for (int i = 0; i < P; i++) {	
		interval[i].start = i * in.height/P;
		interval[i].end = (i+1) * in.height/P;
		interval[i].thread_id = i;
	}
	
	if (in.height % P != 0) {
		interval[P-1].end = in.height-1;
		interval[P-1].thread_id = P-1;
	}

	// Initialize output image	
	out.height = in.height;
	out.width = in.width;
	unsigned long data_size = (unsigned long) out.width * out.height * 3;
	out.data = (unsigned char *)malloc(data_size);

	printf("successfully Initialized output\n");

	for(int i = 0; i < P; i++) {
		pthread_create(&(tid[i]), NULL, applyFilter, &(interval[i]));
	}

	// Join the threads
	for(int i = 0; i < P; i++) {
		pthread_join(tid[i], NULL);
	}

	printf("successfully applied filter\n");

	writeData(argv[2], &out);

	printf("successfully wrote data \n");

	free(in.data);
	free(out.data);

	return 0;
}
