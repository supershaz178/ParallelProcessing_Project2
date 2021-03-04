/*
Fractal code for CS 4380 / CS 5351

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

#include <cstdio>
#include <cmath>
#include <algorithm>
#include <sys/time.h>
#include <mpi.h>
#include "BMP43805351.h"

static void fractal(const int width, const int start, const int end, unsigned char* const pic)
{
  const double Delta = 0.0009;
  const double xMid = -0.212500155;
  const double yMid = -0.821455896;

  printf("Frames: %d", end-start); 
  printf("width: %d", width); 
  // compute pixels of each frame
  for (int frame = start; frame < end; frame++) {  // frames
    const double delta = Delta * (2 + cos(2 * M_PI * frame / (end-start)));
    const double xMin = xMid - delta;
    const double yMin = yMid - delta;
    const double dw = 2.0 * delta / width;
    for (int row = 0; row < width; row++) {  // rows
      const double cy = yMin + row * dw;
      for (int col = 0; col < width; col++) {  // columns
        const double cx = xMin + col * dw;
        double x = cx;
        double y = cy;
        double x2, y2;
        int count = 256;
        do {
          x2 = x * x;
          y2 = y * y;
          y = 2.0 * x * y + cy;
          x = x2 - y2 + cx;
          count--;
        } while ((count > 0) && ((x2 + y2) <= 4.0));
        pic[frame * width * width + row * width + col] = (unsigned char)count;
      }
    }
  }
}

int main(int argc, char *argv[])
{
  int commSize; 
  int myRank; 

  //Initialize MPI Processing
  MPI_Init(NULL, NULL); 
  MPI_Comm_size(MPI_COMM_WORLD, &commSize); 
  MPI_Comm_rank(MPI_COMM_WORLD, &myRank); 
  
  if(myRank == 0){
    printf("Fractal v2.2\n");
  }

  // check command line
  if (argc != 3) {
    fprintf(stderr, "USAGE: %s frame_width number_of_frames\n", argv[0]); exit(-1);
  }
  const int width = atoi(argv[1]);
  if (width < 8) {
    fprintf(stderr, "ERROR: frame_width must be at least 8\n"); exit(-1);
  }
  const int frames = atoi(argv[2]);
  if (frames < 1) {
    fprintf(stderr, "ERROR: number_of_frames must be at least 1\n"); exit(-1);
  }
  if(frames % commSize != 0){
    fprintf(stderr, "ERROR: number_of_frames must be a multiple of the number of processes\n"); 
    exit(-1); 
  }
  if(myRank == 0){
    printf("frames: %d\n", frames);
    printf("width: %d\n", width);
  }

  // allocate picture array
  unsigned char* local_pic = new unsigned char [(frames * width * width)];
  unsigned char* global_pic = NULL;

  if(myRank == 0){
    global_pic = new unsigned char[frames * width * width];
  } 

  //Set up processing blocks for process
  const int startFrame = myRank * (frames / commSize); 
  const int endFrame = (myRank+1) * (frames / commSize); 
  const int range = endFrame * width *width - (startFrame * width * width)

  // start time
  timeval beg, end;
  MPI_Barrier(MPI_COMM_WORLD);
  gettimeofday(&beg, NULL); 
  // execute timed code
  fractal(width, startFrame,endFrame, local_pic);

  printf("Gathering process: %d", myRank);
  MPI_Gather(&local_pic[startFrame], range, MPI_UNSIGNED_CHAR, global_pic, range, MPI_UNSIGNED_CHAR, 0, MPI_COMM_WORLD);
  // end time
  gettimeofday(&end, NULL);
  const double runtime = end.tv_sec - beg.tv_sec + (end.tv_usec - beg.tv_usec) / 1000000.0;
  if(myRank == 0){
    printf("compute time: %.6f s\n", runtime);
  }

  // write result to BMP files
  if ((width <= 256) && (myRank == 0)) {
    for (int frame = 0; frame < frames; frame++) {
      BMP24 bmp(0, 0, width, width);
      for (int y = 0; y < width; y++) {
        for (int x = 0; x < width; x++) {
          bmp.dot(x, y, global_pic[frame * width * width + y * width + x] * 0x000001 + 0x80ff00 - global_pic[frame * width * width + y * width + x] * 0x000100);
        }
      }
      char name[32];
      sprintf(name, "fractal%d.bmp", frame + 1000);
      bmp.save(name);
    }
  }

  if(myRank == 0){
    printf("Number of Processes: %d", commSize); 
  }

  // clean up
  delete [] local_pic; 
  delete [] global_pic;
  MPI_Finalize(); 
  return 0;
}
