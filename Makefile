CC=gcc
MPICC=mpicc

SEQFLAGS=-ljpeg
THREADSFLAGS=-lpthread -ljpeg
OMPFLAGS=-fopenmp -ljpeg
MPIGLAGS=-ljpeg

all: secv omp threads mpi

secv: 
	$(CC) -o secv secvential.c $(SEQFLAGS)

omp:
	$(CC) -o openmp openmp.c $(OMPFLAGS)

threads:
	$(CC) -o threads pthreads.c $(THREADSFLAGS)

mpi:
	$(MPICC) -o mpi mpi.c $(MPIFLAGS)


