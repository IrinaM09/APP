CC=gcc
MPICC=mpicc

SEQFLAGS=-ljpeg
THREADSFLAGS=-lpthread -ljpeg
OMPFLAGS=-fopenmp -ljpeg
MPIFLAGS=-ljpeg

all: secv omp threads mpi

secv: secvential.c
	$(CC) -o secv secvential.c $(SEQFLAGS)

omp: openmp.c
	$(CC) -o openmp openmp.c $(OMPFLAGS)

threads: pthreads.c
	$(CC) -o threads pthreads.c $(THREADSFLAGS)

mpi: mpi.c
	$(MPICC) -o mpi mpi.c $(MPIFLAGS)

clean:
	rm secv openmp threads mpi
