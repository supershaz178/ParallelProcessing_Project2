/*
Maximal independent set code for CS 4380 / CS 5351

Copyright (c) 2021 Texas State University. All rights reserved.

Redistribution in source or binary form, with or without modification,
is *not* permitted. Use in source or binary form, with or without
modification, is only permitted for academic use in CS 4380 or CS 5351
at Texas State University.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

Author: Martin Burtscher
*/

#include <cstdlib>
#include <cstdio>
#include <sys/time.h>
#include <mpi.h>
#include "ECLgraph.h"

static const unsigned char undecided = 0;
static const unsigned char in = 1;
static const unsigned char out = 2;

// https://stackoverflow.com/questions/664014/what-integer-hash-function-are-good-that-accepts-an-integer-hash-key
static unsigned int hash(unsigned int val)
{
  val = ((val >> 16) ^ val) * 0x45d9f3b;
  val = ((val >> 16) ^ val) * 0x45d9f3b;
  return (val >> 16) ^ val;
}

static void mis(const ECLgraph g, unsigned char* const status, unsigned int* const rndval, const int start, const int end)
{
  // initialize local arrays
  for (int v = start; v < end; v++) status[v] = undecided;
  for (int v = start; v < end; v++) rndval[v] = hash(v + 712453897);

  //intialize global recieving arrays
  unsigned char* global_status = new unsigned char [g.nodes];
  bool global_goagain;
 

  bool goagain;
  // repeat until all nodes' status has been decided
  do {
    goagain = false;
    // go over all the nodes
    for (int v = 0; v < g.nodes; v++) {
      if (status[v] == undecided) {
        int i = g.nindex[v];
        // try to find a neighbor whose random number is lower
        while ((i < g.nindex[v + 1]) && ((status[g.nlist[i]] == out) || (rndval[v] < rndval[g.nlist[i]]) || ((rndval[v] == rndval[g.nlist[i]]) && (v < g.nlist[i])))) {
          i++;
        }
        if (i < g.nindex[v + 1]) {
          // found such a neighbor -> status still unknown
          goagain = true;
        } else {
          // no such neighbor -> all neighbors are "out" and my status is "in"
          for (int i = g.nindex[v]; i < g.nindex[v + 1]; i++) {
            status[g.nlist[i]] = out;
          }
          status[v] = in;
        }
      }
    }

    MPI_Allreduce(&status[start], global_status, end - start, MPI_UNSIGNED_CHAR, MPI_MAX, MPI_COMM_WORLD);
    MPI_Allreduce(&goagain, &global_goagain, 1, MPI::BOOL, MPI_LOR, MPI_COMM_WORLD); 
  } while (goagain);
}

int main(int argc, char* argv[])
{
  int myRank; 
  int commSize; 

  //Initalize MPI Processing 
  MPI_Init(NULL,NULL); 
  MPI_Comm_size(MPI_COMM_WORLD, &commSize); 
  MPI_Comm_rank(MPI_COMM_WORLD, &myRank); 
  
  if(myRank == 0){
    printf("Maximal Independent Set v1.6\n");
  }

  // check command line
  if (argc != 2) {fprintf(stderr, "USAGE: %s input_file\n", argv[0]); exit(-1);}

  // read input
  ECLgraph g = readECLgraph(argv[1]);

  if(myRank == 0){
    printf("input: %s\n", argv[1]);
    printf("nodes: %d\n", g.nodes);
    printf("edges: %d\n", g.edges);
  }
 
  // Blocked Assignment of Verticies Start and End Point
  const int startVertex = myRank * (g.nodes / commSize); 
  const int endVertex = (myRank+ 1) * (g.nodes / commSize); 
  const int vertexRange = endVertex - startVertex; 

  // allocate arrays
  unsigned char* const status = new unsigned char [g.nodes];
  unsigned int* const rndval = new unsigned int [g.nodes];

  // start time
  MPI_Barrier(MPI_COMM_WORLD);
  timeval beg, end;
  gettimeofday(&beg, NULL);

  // execute timed code
  mis(g, status, rndval, startVertex, endVertex);

  // end time
  gettimeofday(&end, NULL);
  const double runtime = end.tv_sec - beg.tv_sec + (end.tv_usec - beg.tv_usec) / 1000000.0;

  if(myRank == 0){
    printf("compute time: %.6f s\n", runtime);
  }
  

  // determine and print set size
  int count = 0;
  for (int v = 0; v < g.nodes; v++) {
    if (status[v] == in) {
      count++;
    }
  }

  if( myRank  == 0){
    printf("elements in set: %d (%.1f%%)\n", count, 100.0 * count / g.nodes);
  }

  // verify result
  if(myRank == 0){
    for (int v = 0; v < g.nodes; v++) {
      if ((status[v] != in) && (status[v] != out)) {fprintf(stderr, "ERROR: found unprocessed node\n"); exit(-1);}
      if (status[v] == in) {
        for (int i = g.nindex[v]; i < g.nindex[v + 1]; i++) {
          if (status[g.nlist[i]] == in) {fprintf(stderr, "ERROR: found adjacent nodes in MIS\n"); exit(-1);}
        }
      } else {
        bool flag = true;
        for (int i = g.nindex[v]; i < g.nindex[v + 1]; i++) {
          if (status[g.nlist[i]] == in) {
            flag = false;
            break;
          }
        }
        if (flag) {fprintf(stderr, "ERROR: set is not maximal\n"); exit(-1);}
      }
    }
    printf("verification passed\n");
  }
  // clean up
  MPI_Finalize();
  freeECLgraph(g);
  delete [] status;
  delete [] rndval;
  return 0;
}
