CC=gcc
MPICC=mpicc

SEQFLAGS=-ljpeg -L.
THREADSFLAGS=-lpthread -ljpeg -L.
OMPFLAGS=-fopenmp -ljpeg -L.
MPIFLAGS=-ljpeg -L.

all: secv omp threads mpi hybrid

secv: secvential.c
	$(CC) -o secv secvential.c $(SEQFLAGS)

omp: openmp.c
	$(CC) -o openmp openmp.c $(OMPFLAGS)

threads: pthreads.c
	$(CC) -o threads pthreads.c $(THREADSFLAGS)

mpi: mpi.c
	$(MPICC) -o mpi mpi.c $(MPIFLAGS)

hybrid: hybrid.c
	$(MPICC) -o hybrid hybrid.c $(MPIFLAGS) $(OMPFLAGS)

clean:
	rm secv openmp threads mpi hybrid
