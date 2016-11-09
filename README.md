Etnaviv GPU tests
===================

This contains various tests for Vivante GPUs based on the Etnaviv DRM driver.

Building
-----------

    ./autogen.sh
    ./configure
    make

For cross-builds simply pass `--host=<toolchain-tuple>` to `configure`. You may
have to override `PKG_CONFIG_PATH` `PKG_CONFIG_SYSROOT_DIR` `PKG_CONFIG_LIBDIR`
as well to make it able to find the DRM libraries.

Dependencies
-------------

This package needs `libdrm` installed with `libdrm_etnaviv` enabled. MESA is
not necessary.

Tests
------

### etnaviv_cl_test

This is an extremely basic test that writes "Hello World" to memory
from a CL shader.

### etnaviv_verifyops 

The intent of this test is to test the output of various opcodes against
putative CPU implementation of the same operation, and thus figure out exactly
what the GPU instructions do.

This is achieved by using the `CL` (OpenCL) functionality of GC2000 and higher.

Example output:

    $ etnaviv_verifyops /dev/dri/renderD128
    Version: 1.0.0
      Name: etnaviv
      Date: 20151214
      Description: etnaviv DRM
    add.u32: PASS
    imullo0.u32: PASS
    lshift.u32: PASS
    rshift.u32: PASS
    rotate.u32: PASS
    or.u32: PASS
    and.u32: PASS
    xor.u32: PASS
    not.u32: PASS


