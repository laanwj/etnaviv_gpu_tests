bin_PROGRAMS = etnaviv_cl_test etnaviv_verifyops

EXTRA_LIBRARIES = shared.a

shared_a_SOURCES = drm_setup.c
shared_a_CFLAGS = $(LIBDRM_CFLAGS) $(LIBDRM_ETNAVIV_CFLAGS)

etnaviv_cl_test_SOURCES = etnaviv_cl_test.c
etnaviv_cl_test_LDADD = shared.a $(LIBDRM_LIBS) $(LIBDRM_ETNAVIV_LIBS)
etnaviv_cl_test_CFLAGS = $(LIBDRM_CFLAGS) $(LIBDRM_ETNAVIV_CFLAGS)

etnaviv_verifyops_SOURCES = etnaviv_verifyops.c
etnaviv_verifyops_LDADD = shared.a $(LIBDRM_LIBS) $(LIBDRM_ETNAVIV_LIBS)
etnaviv_verifyops_CFLAGS = $(LIBDRM_CFLAGS) $(LIBDRM_ETNAVIV_CFLAGS)
