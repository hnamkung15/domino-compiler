1. Ensure you have python3 (> 3.5.1) git g++ (>=7.4) build-essential autotools-dev libncurses5-dev autoconf libtool and zlib1g-dev
(Use a package manager like macports or apt-get to get them.)
2. Get clang + llvm from https://releases.llvm.org/5.0.2/clang+llvm-5.0.2-x86_64-linux-gnu-ubuntu-16.04.tar.xz 
This was tested only for macOS Mojave (10.14) and Ubuntu 16.04.
3. Install sketch from https://people.csail.mit.edu/asolar/sketch-1.7.5.tar.gz
add the sketch binary to the path.
4. Install banzai from https://github.com/packet-transactions/banzai.git
5. ./autogen.sh
6. ./configure CLANG_DEV_LIBS=<wherever you untarred clang+llvm in step 2>
(make sure CLANG_DEV_LIBS is set to an absolute file system path)
7. make
8. make check
9. sudo make install (if you want to install it system wide).

=============List of all random mutations used by Domino mutator =============

rand = 1                               state_0 -----> p.tmp1 = state_0; state_0 = p.tmp1;
rand = 2 if (condition_1) {do A;} else {do B;} -----> if(!condition_1) {do B;} else {do A;}
rand = 3       if (condition_1 && condition_2) -----> if (condition_2 && condition_1) 
rand = 4                                       -----> if (0 == 1) {p.tmp_7_0= 0 - p.tmp_7_0;}
rand = 5       pkt.tmp_7_0;                    -----> p.tmp_7_0=p.tmp_7_0+1-1;
rand = 6                    state_0 = state_1; -----> state_0 = state_1 + (3*4-12)*10;
rand = 7              if (condition_1) {do A;} -----> if (condition_1 && 1==1) {do A;}
rand = 8                      if (condition_1) -----> if (condition_1 && (p.tmp_7_0==p.tmp_7_0+1-1))

Note:
constant_vector stores all the constants appearing in original program plus [0,1,2,3,...,2^bit_size-1]

cmd line Instruction:
Output generated .sk file: domino_to_chipmunk <source file name>
Output constant_set content: constant_set <source file name> bit_size
