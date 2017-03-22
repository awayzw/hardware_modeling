/**
 * gemm.c: This file is part of the PolyBench/C 3.2 test suite.
 *
 *
 * Contact: Louis-Noel Pouchet <pouchet@cse.ohio-state.edu>
 * Web address: http://polybench.sourceforge.net
 */
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <math.h>

#include "gemm.h"

int A[1024][1024];
int B[1024][1024];
int C[1024][1024];

void kernel_gemm(float alpha, float beta)
{

  int i, j, k;

#pragma scop
  /* C := alpha*A*B + beta*C */
  for (i = 0; i < 1024; i++)
    for (j = 0; j < 1024; j++)
      {
	C[i][j] *= beta;
	for (k = 0; k < 1024; ++k)
	  C[i][j] += alpha * A[i][k] * B[k][j];
      }
#pragma endscop
}


int main(int argc, char** argv)
{
float alpha = 0.5;
float beta = 0.5;
  kernel_gemm (alpha, beta);

  return 0;
}
