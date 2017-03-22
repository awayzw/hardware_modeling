#include "systemc.h"
#include <sstream>
#include "latencies.h"
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
int A[1024UL][1024UL];
int B[1024UL][1024UL];
int C[1024UL][1024UL];

class kernel_gemm_module : public sc_module
{
public:
   sc_in<bool> scgen__start;
   sc_signal<bool> scgen__running;
   sc_in<float> scgen__alpha;
   sc_in<float> scgen__beta;
   sc_in<int> scgen__thread_id;
  

  public: void kernel_gemm()
{
    while(1){
   while(!scgen__start) wait();
   wait(ACC_LATENCY, SC_NS);
   float alpha=scgen__alpha;
   float beta=scgen__beta;
   int thread_id=scgen__thread_id;
      int i;
      int j;
      int k;
      
#pragma scop
{
        int c5;
        int c3;
        int c1;
{
          for (c1 = 0; c1 <= 1023; c1++) {
            for (c3 = 0; c3 <= 1023; c3++) {
              C[c1][c3] *= beta;
{
                for (c5 = 0; c5 <= 1023; c5++) {
                  C[c1][c3] += ((alpha * A[c1][c5]) * B[c5][c3]);
                }
              }
            }
          }
        }
      }
      
#pragma endscop
   scgen__running = false;
   sc_stop();
    }
  }
   SC_HAS_PROCESS(kernel_gemm_module);
   kernel_gemm_module(const sc_module_name& p_name) :
     sc_module(p_name)
   {
      SC_THREAD(kernel_gemm);
      sensitive << scgen__start;
   }
}
;

int sc_main(int argc,char **argv)
{
  float alpha = 0.5;
  float beta = 0.5;kernel_gemm_module* top_module = new kernel_gemm_module("top_module");
   sc_signal<bool> start;
   sc_signal<float> scgen__alpha_main;
   sc_signal<float> scgen__beta_main;
   sc_signal<int> scgen__thread_id_main;
   top_module->scgen__start(start);
   top_module->scgen__alpha(scgen__alpha_main);
   top_module->scgen__beta(scgen__beta_main);
   top_module->scgen__thread_id(scgen__thread_id_main);
{
  }
  scgen__thread_id_main = 0;
  scgen__beta_main = beta;
  scgen__alpha_main = alpha;
   // Start simulation
   sc_set_time_resolution(1,SC_NS);
   start=true;
   sc_start(1, SC_NS);
   start = false;
   cout << "start @" << sc_time_stamp() << endl;
   sc_start(); // start the simulation
   cout << "done @" << sc_time_stamp() << endl;
  return 0;
}
