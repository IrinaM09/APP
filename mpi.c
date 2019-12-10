#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <jpeglib.h>
#include <mpi.h>

// compilare mpicc -o mpi mpi.c -ljpeg
// rulare mpirun -np <nr_proc> ./mpi <image_in> <image_out>
// ex. mpirun -np 4 ./mpi in/house.pgm house_line.pgm

typedef struct {
	int width;
	int height;
	unsigned char *data;
} image;

// Gaussian noise reduction (sum /= 16)
int edgeDetectionFilter[3][3] = {{-1, -1, -1},
								 {-1, 8, -1},
								 {-1, -1, -1}};


//Get the interval to process
void getInterval(int *start, int *end, int rank, int P, int height) {
	*start = rank * height / P;
	*end = (rank + 1) * height / P;

	if (rank == P -1 && height % P != 0)
		*end = height;
} 

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
	unsigned long data_size = (unsigned long)img->width * img->height * 3;
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
int computeSum(int row, int column, image *in)
{
	int sum = 0;
	int rowIdx = row - 1;
	int colIdx = column - 3;

	for (int i = rowIdx, fi = 0; i < rowIdx + 3; i++, fi++)
	{
		for (int j = colIdx, fj = 0; j < colIdx + 9; j += 3, fj++)
		{
			sum += (int)(in->data[i * 3 * in->width + j]) * edgeDetectionFilter[fi][fj];
		}
	}

	return sum;
}

//Apply filter
void applyFilter(image *in, image *out, int rank, int P)
{
	int start, end;

	getInterval(&start, &end, rank, P, in->height);

	for (int i = start; i < end; i++)
	{
		for (int j = 0; j < 3 * in->width; j++)
		{
			//Border case
			if (i < 1 || i >= end - 1 ||
				j < 3 || j >= 3 * in->width - 3)
			{
				out->data[i *  3 * in->width + j] = in->data[i * 3 * in->width + j];
				continue;
			}

			out->data[i * 3 * in->width + j] = (char)(computeSum(i, j, in) / 16);
		}
	}
}


//Compute the whole image
void computeImage(image *out, int rank, int P) {
	int start, end;
	
	if (rank != 0) {
		getInterval(&start, &end, rank, P, out->height);

		end = end - start + 1;

    	MPI_Send(out->data, out->width * end, MPI_UNSIGNED_CHAR, rank, 0, MPI_COMM_WORLD);
	} 
	else {
		for (int proc = 1; proc < P; proc++) {
			getInterval(&start, &end, proc, P, out->height);

    		MPI_Recv(out->data, out->width * end, MPI_UNSIGNED_CHAR, proc, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
		}
	}
}

int main(int argc, char * argv[]) {
	image in;
	image out;

	int rank;
	int P;

	MPI_Init(&argc, &argv);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &P);

	if (rank == 0) {
		// Read the input image
		readInput(argv[1], &in);

		// Send image to the other processes
		for (int j = 1; j < P; j++){ 
    		MPI_Send(&in.width, 1, MPI_INT, j, 0, MPI_COMM_WORLD);
    		MPI_Send(&in.height, 1, MPI_INT, j, 0, MPI_COMM_WORLD);
    		MPI_Send(in.data, in.width * in.height * 3, MPI_UNSIGNED_CHAR, rank, 0, MPI_COMM_WORLD);
    	}
	}

    if (rank != 0) {
    	int data_size = out.width * out.height * 3;
        in.data = (unsigned char *) malloc(data_size);

        MPI_Recv(&in.width, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        MPI_Recv(&in.height, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
		MPI_Recv(in.data, in.width * in.height * 3, MPI_UNSIGNED_CHAR, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
   	}

	printf("successfully read input\n");
	
	out.height = in.height;
	out.width = in.width;
	unsigned long data_size = (unsigned long)out.width * out.height * 3;
	out.data = (unsigned char *) malloc(data_size);

	printf("successfully Initialized output\n");

	// Apply the filter on image on chunks
	applyFilter(&in, &out, rank, P);
	MPI_Barrier(MPI_COMM_WORLD);

	printf("successfully applied filter\n");

	// Compute the whole image
	computeImage(&out, rank, P);

	if (rank == 0) {
		writeData(argv[2], &out);
	}

	MPI_Finalize();

	printf("successfully wrote data \n");

	free(in.data);
	free(out.data);

	return 0;
}
