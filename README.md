1. Installation
    
    1. This tool is integrated in [PolyOpt](http://web.cs.ucla.edu/~pouchet/software/polyopt/#installation), hence it assumes that PolyOpt is installed.
          
        1. For direct C-to-SystemC transformation, version PolyOpt-C-0.2.1 can be used.
        2. For C-to-SystemC with PolyOpt and HLS optimizations (e.g., tiling, communication cost), PolyOpt-0.3.0 is needed.
        
    2. Once PolyOpt is installed, replace 
        
        1. the folder "polyopt" with the folder in the reporsitory,
        2. ./pocc/optimizers/pluto/src/pocc-driver.c
        3. ./pocc/optimizers/pluto/pluto-0.5.5/src/main.c
       
      Rebuild the tool. It contains the functions to transform C to SystemC

2. The requirment of the input files(To me, this should not be this strict, need to make it more general).

    The input C code should contain at least two functions:

           1. kernel function, which is the design of the accelerator
           2. main, in which the kernel function is called. 
           
2. Introduction to flags.

    1. To directly transfer C to SystemC.
    
    ```
    PolyRose --systemc-no-pretile   --systemc-target-func=kernel_function  filename.c
    ```
    The brief explanation is as following: 
    
        * --systemc-no-pretile: directly generate SystemC from the input C code without transformation.
        
        * --systemc-target-func=kernel_function: where "kerenel_function" is the name of your design function.
        
        * filename.c: The input file name. 

3. Example
    
    Input: gemm.c
    ```
    PolyRose --systemc-no-pretile   --systemc-target-func=kernel_gemm  gemm.c
    ```
    Output: rose_gemm.c

    compile:
    ```
    g++ -g3 -I. -I$SYSTEMC_HOME/include -L. -L$SYSTEMC_HOME/lib-linux64 -lsystemc -lm -fno-deduce-init-list rose_gemm.c  -std=c++0x -o rose_gemm.out 
    ```
