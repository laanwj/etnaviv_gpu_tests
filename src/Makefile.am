bin_PROGRAMS = etnaviv_cl_test_gc2000 \
		etnaviv_cl_test_gc3000 \
		etnaviv_verifyops \
		etnaviv_cl_bench \
		etnaviv_triangle_gc3000 \
		etnaviv_cube_gc7000 \
		etnaviv_bltdemo_gc7000 \
		etnaviv_blttest_gc7000 \
		etnaviv_blttest2_gc7000

EXTRA_LIBRARIES = libshared.a

libshared_a_SOURCES = drm_setup.c gpu_code.c write_bmp.c etna_fb.c etnaviv_tiling.c
libshared_a_CFLAGS = $(LIBDRM_CFLAGS) $(LIBDRM_ETNAVIV_CFLAGS)

etnaviv_cl_test_gc2000_SOURCES = etnaviv_cl_test_gc2000.c
etnaviv_cl_test_gc2000_LDADD = libshared.a $(LIBDRM_LIBS) $(LIBDRM_ETNAVIV_LIBS)
etnaviv_cl_test_gc2000_CFLAGS = $(LIBDRM_CFLAGS) $(LIBDRM_ETNAVIV_CFLAGS)

etnaviv_cl_test_gc3000_SOURCES = etnaviv_cl_test_gc3000.c
etnaviv_cl_test_gc3000_LDADD = libshared.a $(LIBDRM_LIBS) $(LIBDRM_ETNAVIV_LIBS)
etnaviv_cl_test_gc3000_CFLAGS = $(LIBDRM_CFLAGS) $(LIBDRM_ETNAVIV_CFLAGS)

etnaviv_verifyops_SOURCES = etnaviv_verifyops.c
etnaviv_verifyops_LDADD = libshared.a $(LIBDRM_LIBS) $(LIBDRM_ETNAVIV_LIBS)
etnaviv_verifyops_CFLAGS = $(LIBDRM_CFLAGS) $(LIBDRM_ETNAVIV_CFLAGS)

etnaviv_cl_bench_SOURCES = etnaviv_cl_bench.c
etnaviv_cl_bench_LDADD = libshared.a $(LIBDRM_LIBS) $(LIBDRM_ETNAVIV_LIBS)
etnaviv_cl_bench_CFLAGS = $(LIBDRM_CFLAGS) $(LIBDRM_ETNAVIV_CFLAGS)

etnaviv_triangle_gc3000_SOURCES = triangle_gc3000.c
etnaviv_triangle_gc3000_LDADD = libshared.a $(LIBDRM_LIBS) $(LIBDRM_ETNAVIV_LIBS)
etnaviv_triangle_gc3000_CFLAGS = $(LIBDRM_CFLAGS) $(LIBDRM_ETNAVIV_CFLAGS)

etnaviv_cube_gc7000_SOURCES = cube_gc7000.c
etnaviv_cube_gc7000_LDADD = libshared.a $(LIBDRM_LIBS) $(LIBDRM_ETNAVIV_LIBS)
etnaviv_cube_gc7000_CFLAGS = $(LIBDRM_CFLAGS) $(LIBDRM_ETNAVIV_CFLAGS)

etnaviv_bltdemo_gc7000_SOURCES = bltdemo_gc7000.c
etnaviv_bltdemo_gc7000_LDADD = libshared.a $(LIBDRM_LIBS) $(LIBDRM_ETNAVIV_LIBS)
etnaviv_bltdemo_gc7000_CFLAGS = $(LIBDRM_CFLAGS) $(LIBDRM_ETNAVIV_CFLAGS)

etnaviv_blttest_gc7000_SOURCES = blttest_gc7000.c
etnaviv_blttest_gc7000_LDADD = libshared.a $(LIBDRM_LIBS) $(LIBDRM_ETNAVIV_LIBS)
etnaviv_blttest_gc7000_CFLAGS = $(LIBDRM_CFLAGS) $(LIBDRM_ETNAVIV_CFLAGS)

etnaviv_blttest2_gc7000_SOURCES = blttest2_gc7000.c
etnaviv_blttest2_gc7000_LDADD = libshared.a $(LIBDRM_LIBS) $(LIBDRM_ETNAVIV_LIBS)
etnaviv_blttest2_gc7000_CFLAGS = $(LIBDRM_CFLAGS) $(LIBDRM_ETNAVIV_CFLAGS)
