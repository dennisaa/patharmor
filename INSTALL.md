Patharmor Build & Installation Instructions
===========================================

These instructions are based on getting Patharmor running from scratch
on Ubuntu 14.04.3 (LTS) amd64 desktop.

Disclaimer
==========
Following this procedure will run some scripts and code as root, and load
a kernel module, and start a listening network server. It should not
install anything on your system outside its own subdirectory. However
you're doing this at your own risk. If you want to run no risk to
your system, use a from-scratch installed system as this procedure
specifies. See also the disclaimer in LICENSE.md.

Test configuration
==================

Tested with
  * install image a3de42e9b563f3ccf100fa84f4f7c831e659320f
    http://releases.ubuntu.com/trusty/ubuntu-14.04.3-desktop-amd64.iso
  * kernel version 3.16.0-49
  * llvm-3.4, clang-3.4 and g++-4.8
  * boost 1.55

Starting from a bare install, it takes around 45 minutes in a vbox test on
reasonably recent hardware. Most of this time is spent building Dyninst.

PA Build procedure from scratch
===============================

Install this image, with updates downloaded while installing, and
allowing third-party software. Then open a terminal and type:

     sudo apt-get update && sudo apt-get install git && sudo apt-get upgrade
     sudo apt-get install linux-image-3.16.0-49-generic linux-headers-3.16.0-49-generic

The expected output is in [typescript.kernel](typescript.kernel).

Now, reboot with this linux-image-3.16.0-49-generic kernel version. Once you are
rebooted, verify the kernel version:

     patharmor@patharmor-test:~$ uname -a
     Linux patharmor-test 3.16.0-49-generic #65~14.04.1-Ubuntu SMP Wed Sep 9 10:03:23 UTC 2015 x86_64 x86_64 x86_64 GNU/Linux

Now get the repo and install some dependencies from packages:

     git clone https://github.com/dennisaa/patharmor.git
     cd patharmor
     sudo apt-get install g++ clang libssl-dev cmake libboost1.55-all-dev libelf-dev libiberty-dev 

The expected output is in [typescript.packages](typescript.packages).

This installs llvm-3.4, clang-3.4, g++-4.8, libssl-dev, cmake, libelf and
boost 1.55.

Of these, Dyninst depends on libelf, libiberty and boost. We choose
packages instead of letting Dyninst build the dependencies to work around
build problems that arise otherwise. We do rely on Dyninst to build its
own libdwarf so that it has a PIC version readily available.

Build & install the packaged, patched Dyninst (see patches/):

     cd Dyninst-8.2.1/install-dir
     cmake .. -DCMAKE_INSTALL_PREFIX=`pwd`
     make -j2 && make install

The expected output is in [typescript.dyninst](typescript.dyninst).

Build & install patharmor components:

     cd ../.. # back to the top level of the Patharmor distribution
     make && make install # installs in bin/

The expected output is in [typescript.pa](typescript.pa).

Build & load the patharmor LKM:

     sh lkm-start.sh

The expected output is in [typescript.lkm](typescript.lkm).

Building the demo app
=====================
From the Patharmor distribution root:

     cd nginx-0.8.54
     sudo apt-get install libpcre3-dev
     ./configure --prefix=`pwd`/install-dir --with-cc-opt="-Wno-error -O0 -fasynchronous-unwind-tables -fPIC -g -pthread"
     make && make install

The expected output is in [typescript.nginx](typescript.nginx).

Running
=======
A demonstration of PA in action on the demo app.

To run PA on a server application, we suggest two terminals (xterms).

On the first xterm, from the PA distribution root, build and load the LKM.

     sh lkm-start.sh

A successful load is indicated in the shell as 'loading armor module' and in
dmesg by this excerpt:

     [ 1662.754093] armor_module: module verification failed: signature and/or  required key missing - tainting kernel
     [ 1662.775508] jmp_src @ ffffffff81080e00:
     [ 1662.775511] 0f 1f 44 00 00 55 65 48 8b 04 25 48 b8 00 00 48 
     [ 1662.775520] [intercept.c] inserting relative jump ffffffff81080e00 --> ffffffffc05b4291
     [ 1662.775522] jmp_src @ ffffffff81080e00:
     [ 1662.775523] e9 8c 34 53 3f 55 65 48 8b 04 25 48 b8 00 00 48 
     [ 1662.775531] jmp_back_src @ ffffffffc05b42a7:
     [ 1662.775532] 90 90 90 90 90 48 83 f8 09 74 2f 48 83 f8 0a 0f 
     [ 1662.775539] [intercept.c] inserting relative jump ffffffffc05b42a7 --> ffffffff81080e05
     [ 1662.775542] jmp_back_src @ ffffffffc05b42a7:
     [ 1662.775543] e9 59 cb ac c0 48 83 f8 09 74 2f 48 83 f8 0a 0f 
     [ 1662.775635] Armor module initialized

On the second xterm, run the analysis system that will analyze LBRs in
userspace. Do this from the nginx distribution root.

     sh ../run-at.sh ./install-dir/sbin/nginx # required only once
     sh ../run-analyser.sh ./install-dir/sbin/nginx

This output should end after some verbose CFG information with "Initializing JIT daemon".

On the first xterm, also go to the nginx root and start nginx itself. 
Note, this will listen on port 80.

     sh ../run-app.sh ./install-dir/sbin/nginx

Now run a load on the server:

     sudo apt-get install apache2-utils
     ab -n 1000 http://127.0.0.1/

Now kill it (this provokes some more LBR analysis):

     sudo killall nginx

That is it! The LBR analysis window will show you which LBRs are being
analyzed. Once all processes using /dev/armor have exited (the lbr
analysis process and all server processes), dmesg will show interesting
LKM activity stats, like number of unique LBRs analyzed, cached, various
caught system calls.

For every analyzed LBR, the daemon will output the LBR and then:

    \------------------- is valid

if the daemon accepts the LBR as a legitimate in terms of the found CFG.

Now cleanly shutdown the analyser by typing ^C in the analyser window. It will show you a summary similar
to:

     SIGINT caught, exiting
     
     __________ TOTALS ____________________________________________
     LBRs validated: 7
     indirect branches:    57
      - indirect calls:     1
      - indirect jumps:    20
      - local returns :    22
      - lib returns   :    14
     direct branches  :    29 (direct calls only)
     empty entries    :    25
     unknown edges    :     1
     
     
     __________ AVERAGES PER LBR __________________________________
     indirect branches:     8.14
      - indirect calls:     0.14
      - indirect jumps:     2.86
      - local returns :     3.14
      - lib returns   :     2.00
     direct branches  :     4.14 (direct calls only)
     empty entries    :     3.57
     unknown edges    :     0.14
     
     
     __________ KERNEL SUMMARY ____________________________________


And once all processes have closed /dev/armor (which should be now), the LKM prints a summary
similar to:

     [ 2315.364761] Some statistics:
     [ 2315.364767] Signal handlers entered: 4
     [ 2315.364769] Signal returns:          4
     [ 2315.364771] Library calls:    28122
     [ 2315.364772] Library returns:  28121
     [ 2315.364773] Callback calls:   0
     [ 2315.364775] Callback returns: 0
     [ 2315.364776] 
     [ 2315.364777] Number of newly created tasks: 2
     [ 2315.364779] Number of exited tasks:        3
     [ 2315.364780] 
     [ 2315.364781] System calls to execve:    0
     [ 2315.364783] System calls to read:    0
     [ 2315.364784] System calls to write:    0
     [ 2315.364786] System calls to mmap:      18    (only PROT_EXEC: 4)
     [ 2315.364788] System calls to mprotect:  8     (only PROT_EXEC: 0)
     [ 2315.364790] System calls to sigaction: 12
     [ 2315.364791] System calls total:        38
     [ 2315.364792] 
     [ 2315.364794] Context switches (sched-out): 2067
     [ 2315.364795] Context switches (sched-in):  2069
     [ 2315.364797] 
     [ 2315.364798] Number of digests computed: 20
     [ 2315.364799] LBR JIT lookups:     20
     [ 2315.364801] LBR JIT cache hits:  13
     [ 2315.364802] LBR JIT hits:        7
     [ 2315.364803] LBR JIT misses:      0
     [ 2315.364805] LBR JIT unsupported: 0
     [ 2315.364806] LBR JIT timeouts:    0
     [ 2315.364807] LBR JIT sec:         7
     [ 2315.364809] LBR JIT usec:        452000
     [ 2315.364810] armor-module is now closed

This shows 7 unique LBR's that were forwarded to userspace and all validated to be
correct LBR's.

The expected output for running the application is in [typescript.run](typescript.run).
The expected output for the analyser is in [typescript.run-nginx-analyzer](typescript.run-nginx-analyzer).

Notes on usage
==============

If the target binary changes, the call targets analysis has to be re-run. To do this from e.g. nginx:

     sh ../run-at.sh ./install-dir/sbin/nginx

This will re-run the address lookups.

If the LBRs ever stop being analyzed, check dmesg for timeouts. The kernel module
will stop forwarding LBRs to the userland daemon if it ever sees a timeout. If
this happens, re-start the app and analyser.

Demonstration of finding a bug
==============================
The toy-bug directory contains a small program that will use an indirect call to
call a user-specified address. This is similar to a scenario where an attacker gains
control of program control flow by manipulating the input in a malicious way. In this
case, no clever manipulation is necessary - the program will do as it is told. Patharmor
will catch this as an invalid invocation of the targeted call.

Make sure di-opt is restarted from an earlier invocation killall di-opt

Again, on the first xterm start the analyzer from the toy-bug dir:

     make
     sh ../run-at.sh ./toy
     sh ../run-analyser.sh ./toy

Another xterm, also from the toy-bug dir:

     sh ../run-app.sh ./toy

This will invoke some 'dangerous' system calls legitimately. (That is, they are in the source of the toy program.) The analysis output should end in 

     \------------------- is valid

showing this is a legitimate invocation three times. One each for mmap, mprotect and sigaction. The process then ends because no argument was supplied.

The argument will be used to perform an indirect call that is NOT in the source of the
program. Now look at what offset sigaction() is in the debug output of the invocation (1st xterm).
NOTE: We use sigaction as it does not require valid args to trigger an LBR validation.

          * Found 10 PLT entries that we have to wrap:
            o 0x400600->0x601018->0x7f31ba529e30: puts                 +0x06fe30: _IO_puts
            o 0x400610->0x601020->0x7f31ba4f0f60: sigaction            +0x036f60: __sigaction
            o 0x400620->0x601028->0x7f31ba5ae9c0: mmap                 +0x0f49c0: __GI___mmap64                 >> simple, exit points at: 0x7f31ba5ae9d2 0x7f31ba5ae9e3 <<
            o 0x400630->0x601030->0x7f31ba50e400: printf               +0x054400: printf
            o 0x400640->0x601038->0x7f31bbb137c5: __libc_start_main    +0x0017c5: __libc_start_main
            o 0x400650->0x601040->0x7f31ba50e370: fprintf              +0x054370: fprintf
            o 0x400660->0x601048->0x000000000000: __gmon_start__           >> not found <<
            o 0x400670->0x601050->0x7f31ba518450: __isoc99_sscanf      +0x05e450: __isoc99_sscanf
            o 0x400680->0x601058->0x7f31ba5aea20: mprotect             +0x0f4a20: __GI_mprotect                 >> simple, exit points at: 0x7f31ba5aea2f 0x7f31ba5aea40 <<
            o 0x400690->0x601060->0x7f31ba4f6290: exit                 +0x03c290: exit
          * Modifications succesful

This shows you you can use 0x400610 as sigaction offset.

Expected output from running the toy app is in [typescript.run-toy](typescript.run-toy), the analyser in [typescript.toy-analyser](typescript.toy-analyser).

Restart the analysing daemon so it can deal with the new invocation.

     sh ../run-analyser.sh ./toy

Now invoke the program and supply the sigaction offset as an arg.

     sh ../run-app.sh ./toy 0x400610

The analyser will first analyse the legitimate mmap() LBR, e.g.:

        LBR state to 0x00007f31ba5ae9c0 is valid
        lbr[ 0], <from:            (nil), to:            (nil)>  empty
        lbr[ 1], <from:            (nil), to:            (nil)>  empty
        lbr[ 2], <from:            (nil), to:            (nil)>  empty
        lbr[ 3], <from:            (nil), to:            (nil)>  empty
        lbr[ 4], <from:            (nil), to:            (nil)>  empty
        lbr[ 5], <from:            (nil), to:            (nil)>  empty
        lbr[ 6], <from:            (nil), to:            (nil)>  empty
        lbr[ 7], <from:            (nil), to:            (nil)>  empty
        lbr[ 8], <from:            (nil), to:            (nil)>  empty
        lbr[ 9], <from:            (nil), to:            (nil)>  empty
        lbr[10], <from:   0x7f31bbb137b6, to:         0x40078d>  ARMOR.load
        lbr[11], <from:         0x4007b1, to:         0x400600>  call
        lbr[12], <from:         0x400600, to:   0x7f31ba529e30>  indirect.jump
        lbr[13], <from:   0x7f31bbb13b1a, to:         0x4007b6>  lib.return
        lbr[14], <from:         0x4007d6, to:         0x400620>  call
        lbr[15], <from:         0x400620, to:   0x7f31ba5ae9c0>  indirect.jump
        !ind.:   3, call:   2, icall:   0, ijmp:   2, locret:   0, libret:   1, iindex:  12
        #ind.:   3, call:   2, icall:   0, ijmp:   2, locret:   0, libret:   1, iindex:  12, e:  10, u: 0 | l:  1, lbrs: 1
            \------------------- is valid

Then the mprotect() LBR, e.g.:

        LBR state to 0x00007f31ba5aea20 is valid
        lbr[ 0], <from:            (nil), to:            (nil)>  empty
        lbr[ 1], <from:            (nil), to:            (nil)>  empty
        lbr[ 2], <from:            (nil), to:            (nil)>  empty
        lbr[ 3], <from:         0x400600, to:   0x7f31ba529e30>  indirect.jump
        lbr[ 4], <from:   0x7f31bbb13b1a, to:         0x4007b6>  lib.return
        lbr[ 5], <from:         0x4007d6, to:         0x400620>  call
        lbr[ 6], <from:         0x400620, to:   0x7f31ba5ae9c0>  indirect.jump
        lbr[ 7], <from:   0x7f31bbb13b1a, to:         0x4007db>  lib.return
        lbr[ 8], <from:         0x4007e4, to:         0x400600>  call
        lbr[ 9], <from:         0x400600, to:   0x7f31ba529e30>  indirect.jump
        lbr[10], <from:   0x7f31bbb13b1a, to:         0x4007e9>  lib.return
        lbr[11], <from:         0x4007ee, to:         0x400600>  call
        lbr[12], <from:         0x400600, to:   0x7f31ba529e30>  indirect.jump
        lbr[13], <from:   0x7f31bbb13b1a, to:         0x4007f3>  lib.return
        lbr[14], <from:         0x400802, to:         0x400680>  call
        lbr[15], <from:         0x400680, to:   0x7f31ba5aea20>  indirect.jump
        !ind.:   9, call:   4, icall:   0, ijmp:   5, locret:   0, libret:   4, iindex:   3
        #ind.:  12, call:   6, icall:   0, ijmp:   7, locret:   0, libret:   5, iindex:  15, e:  13, u: 0 | l:  1, lbrs: 2
            \------------------- is valid

And finally the legitimate sigaction() LBR:

        LBR state to 0x00007f31ba4f0f60 is valid
        lbr[ 0], <from:            (nil), to:            (nil)>  empty
        lbr[ 1], <from:            (nil), to:            (nil)>  empty
        lbr[ 2], <from:            (nil), to:            (nil)>  empty
        lbr[ 3], <from:            (nil), to:            (nil)>  empty
        lbr[ 4], <from:   0x7f31bbb13b1a, to:         0x4007f3>  lib.return
        lbr[ 5], <from:         0x400802, to:         0x400680>  call
        lbr[ 6], <from:         0x400680, to:   0x7f31ba5aea20>  indirect.jump
        lbr[ 7], <from:   0x7f31bbb13b1a, to:         0x400807>  lib.return
        lbr[ 8], <from:         0x40080c, to:         0x400600>  call
        lbr[ 9], <from:         0x400600, to:   0x7f31ba529e30>  indirect.jump
        lbr[10], <from:   0x7f31bbb13b1a, to:         0x400811>  lib.return
        lbr[11], <from:         0x400816, to:         0x400600>  call
        lbr[12], <from:         0x400600, to:   0x7f31ba529e30>  indirect.jump
        lbr[13], <from:   0x7f31bbb13b1a, to:         0x40081b>  lib.return
        lbr[14], <from:         0x40082f, to:         0x400610>  call
        lbr[15], <from:         0x400610, to:   0x7f31ba4f0f60>  indirect.jump
        !ind.:   8, call:   4, icall:   0, ijmp:   4, locret:   0, libret:   4, iindex:   6
        #ind.:  20, call:  10, icall:   0, ijmp:  11, locret:   0, libret:   9, iindex:  21, e:  17, u: 0 | l:  1, lbrs: 3
            \------------------- is valid

Now, the program parses the argv[1], 0x400610, which is again the sigaction() offset.
But it is not in the source so the LBR fails to validate:

        lbr[ 0], <from:            (nil), to:            (nil)>  empty
        lbr[ 1], <from:            (nil), to:            (nil)>  empty
        lbr[ 2], <from:            (nil), to:            (nil)>  empty
        lbr[ 3], <from:            (nil), to:            (nil)>  empty
        lbr[ 4], <from:   0x7f31bbb13b1a, to:         0x400834>  lib.return
        lbr[ 5], <from:         0x400839, to:         0x400600>  call
        lbr[ 6], <from:         0x400600, to:   0x7f31ba529e30>  indirect.jump
        lbr[ 7], <from:   0x7f31bbb13b1a, to:         0x40083e>  lib.return
        lbr[ 8], <from:         0x40088a, to:         0x400670>  call
        lbr[ 9], <from:         0x400670, to:   0x7f31ba518450>  indirect.jump
        lbr[10], <from:   0x7f31bbb13b1a, to:         0x40088f>  lib.return
        lbr[11], <from:         0x4008ba, to:         0x400630>  call
        lbr[12], <from:         0x400630, to:   0x7f31ba50e400>  indirect.jump
        lbr[13], <from:   0x7f31bbb13b1a, to:         0x4008bf>  lib.return
        lbr[14], <from:         0x4008c3, to:         0x400610>  unknown
        lbr[15], <from:         0x400610, to:   0x7f31ba4f0f60>  indirect.jump
        !ind.:   8, call:   3, icall:   0, ijmp:   4, locret:   0, libret:   4, iindex:   6
        #ind.:  28, call:  13, icall:   0, ijmp:  15, locret:   0, libret:  13, iindex:  27, e:  21, u: 1 | l:  1, lbrs: 4
            \------------------- is NOT valid

This demonstrates patharmor successfully distinguishes legitimate from nonlegitimate call sequences.

Expected output from running the toy app is in [typescript.run-toy-non-legit](typescript.run-toy-non-legit), the analyser in [typescript.toy-analyser-non-legit](typescript.toy-analyser-non-legit).
