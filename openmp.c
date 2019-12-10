#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <jpeglib.h>
#include <omp.h>

// compilare gcc -o openmp -fopenmp openmp.c
// export OMP_NUM_THREADS=4
// rulare ./openmp <image_in> <image_out>
// ex. ./openmp in/house.jpg house_line.jpg 

typedef struct {
	unsigned long width;
	unsigned long height;
	unsigned char *data;
} image;

// Gaussian noise reduction (sum /= 16)
int edgeDetectionFilter[3][3] = {{-1, -1, -1},
								 {-1, 8, -1},
								 {-1, -1, -1}};


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

	printf("Input image width and height: %lu %lu\n", (*img).width, (*img).height);

	img->data = (unsigned char *)malloc(data_size * sizeof(unsigned char));

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

	printf("Output image width and height: %lu %lu\n", (*img).width, (*img).height);

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
int computeSum(unsigned long row, unsigned long column, image *in)
{
	int sum = 0;
	unsigned long rowIdx = row - 1;
	unsigned long colIdx = column - 3;

	for (unsigned long i = rowIdx, fi = 0; i < rowIdx + 3; i++, fi++)
	{
		for (unsigned long j = colIdx, fj = 0; j < colIdx + 9; j += 3, fj++)
		{
			sum += (int)(in->data[i * 3 * in->width + j]) * edgeDetectionFilter[fi][fj];
		}
	}

	return sum;
}

//Apply filter
void applyFilter(image *in, image *out)
{
	unsigned long i, j;

	// no need to make i private; it is already private
	#pragma omp parallel for private (j)
	for (i = 0; i < in->height; i++)
	{
		for (j = 0; j < 3 * in->width; j++)
		{
			//Border case
			if (i < 1 || i >= in->height - 1 ||
				j < 3 || j >= 3 * in->width - 3)
			{
				out->data[i *  3 * in->width + j] = in->data[i * 3 * in->width + j];
				continue;
			}

			out->data[i * 3 * in->width + j] = (unsigned char)(computeSum(i, j, in) / 16);
		}
	}
}


int main(int argc, char * argv[]) {
	image in;
	image out;

	readInput(argv[1], &in);

	printf("successfully read input\n");
	
	out.height = in.height;
	out.width = in.width;
	unsigned long data_size = (unsigned long) out.width * out.height * 3;
	out.data = (unsigned char *)malloc(data_size * sizeof(unsigned char));
	if (out.data == NULL)
		return -1;

	printf("successfully Initialized output\n");

	applyFilter(&in, &out);

	printf("successfully applied filter\n");

	writeData(argv[2], &out);

	printf("successfully wrote data \n");

	free(in.data);
	free(out.data);

	return 0;
}
