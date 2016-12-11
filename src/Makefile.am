bin_PROGRAMS = etnaviv_cl_test_gc2000 \
		etnaviv_cl_test_gc3000 \
		etnaviv_verifyops \
		etnaviv_cl_bench

EXTRA_LIBRARIES = shared.a

shared_a_SOURCES = drm_setup.c gpu_code.c
shared_a_CFLAGS = $(LIBDRM_CFLAGS) $(LIBDRM_ETNAVIV_CFLAGS)

etnaviv_cl_test_gc2000_SOURCES = etnaviv_cl_test_gc2000.c
etnaviv_cl_test_gc2000_LDADD = shared.a $(LIBDRM_LIBS) $(LIBDRM_ETNAVIV_LIBS)
etnaviv_cl_test_gc2000_CFLAGS = $(LIBDRM_CFLAGS) $(LIBDRM_ETNAVIV_CFLAGS)

etnaviv_cl_test_gc3000_SOURCES = etnaviv_cl_test_gc3000.c
etnaviv_cl_test_gc3000_LDADD = shared.a $(LIBDRM_LIBS) $(LIBDRM_ETNAVIV_LIBS)
etnaviv_cl_test_gc3000_CFLAGS = $(LIBDRM_CFLAGS) $(LIBDRM_ETNAVIV_CFLAGS)

etnaviv_verifyops_SOURCES = etnaviv_verifyops.c
etnaviv_verifyops_LDADD = shared.a $(LIBDRM_LIBS) $(LIBDRM_ETNAVIV_LIBS)
etnaviv_verifyops_CFLAGS = $(LIBDRM_CFLAGS) $(LIBDRM_ETNAVIV_CFLAGS)

etnaviv_cl_bench_SOURCES = etnaviv_cl_bench.c
etnaviv_cl_bench_LDADD = shared.a $(LIBDRM_LIBS) $(LIBDRM_ETNAVIV_LIBS)
etnaviv_cl_bench_CFLAGS = $(LIBDRM_CFLAGS) $(LIBDRM_ETNAVIV_CFLAGS)
