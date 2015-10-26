# patharmor
PathArmor context-sensitive CFI implementation
----------------------------------------------

Stuff that gets built in this directory from PA to which LICENSE.md applies:
  * lkm: directory containing the Linux kernel module
  * dyninst-pass: dyninst pass used in PA
  * dyninst-static: static part of PA LBR analysis
  * include: many include dirs
  * shared: runtime loaded module in PA
  * toy-bug: toy example that can be used to trigger a non-legitimate LBR

Stuff that gets built that is included but not part of PA itself to
which LICENSE.md does not apply:
  * Dyninst-8.2.1 with patches in patches/ applied
  * DynamoRIO-Linux-5.0.0-9
  * nginx-0.8.54: stock nginx distribution we used to test PA

Other dirs:
  * bin: install directory for di modules

Notes:
  * These Makefiles assume LLVM 3.4

Building / Installing
=====================

Detailed instructions are in [INSTALL.md](INSTALL.md).

Authors
=======

This software is the open-sourcing of the research prototype supporting the paper
"Practical Context-Sensitive CFI", published in ACM Computer and Communications
Security (CCS) 2015. The authors on the paper, and to some degree the software, are:

First authors:
 * Victor van der Veen
 * Dennis Andriesse

Further:
 * Enes Göktas
 * Ben Gras
 * Lionel Sambuc
 * Asia Slowinska
 * Herbert Bos
 * Cristiano Giuffrida

Special thanks also to:
 * Xi Chen (Address-Taken implementation)
 * Alyssa Milburn (help with debugging LLVM's DSA)


