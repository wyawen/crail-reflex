lib_LTLIBRARIES = libreflex.la

AM_CPPFLAGS = -msse4.1 -I@jdk@/include -I@jdk@/include/linux -I@ix@/deps/dpdk/build/include -I@ix@/deps/dpdk/lib/librte_eal/common -I@ix@/inc -I@ix@/inc/lwip -I@ix@/inc/lwip/ipv4 -I@ix@/inc/lwip/ipv6 -I@ix@/deps/pcidma -I@ix@/libix
libreflex_la_LIBADD = -L/usr/local/lib -levent -levent_core -levent_pthreads -levent_extra -levent_openssl -lixev


#AM_CPPFLAGS = -msse4.1 -I@jdk@/include -I@jdk@/include/linux -I@ix@/deps/dpdk/build/include -I@ix@/deps/dpdk/lib/librte_eal/common -I@ix@/inc -I@ix@/inc/lwip -I@ix@/inc/lwip/ipv4 -I@ix@/inc/lwip/ipv6 -I@ix@/libix -I@ix@/deps/pcidma
#libreflex_la_LDFLAGS = -Wl,-rpath=@ix@/deps/dpdk/x86_64-native-linuxapp-gcc/lib
#libreflex_la_LIBADD = -L/usr/local/lib -levent -levent_core -levent_pthreads -levent_extra -levent_openssl -lix -lixev -L@ix@/deps/dpdk/x86_64-native-linuxapp-gcc/lib -lrte_pmd_ixgbe -lrte_pmd_ena -lrte_ethdev -lrte_mbuf -lrte_eal -lrte_mempool -lrte_ring -lrte_mempool_ring   



include_HEADERS = src/stanford_mast_reflex_NativeDispatcher.h
libreflex_la_SOURCES = src/stanford_mast_reflex_NativeDispatcher.cpp

