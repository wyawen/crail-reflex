lib_LTLIBRARIES = libreflex.la

AM_CPPFLAGS = -I@jdk@/include -I@jdk@/include/linux
libreflex_la_LIBADD =  -L/usr/local/lib -levent -levent_core -levent_pthreads -levent_extra -levent_openssl 


#AM_CPPFLAGS = -I@jdk@/include -I@jdk@/include/linux -I@ix@/deps/dpdk/build/include -I@ix@/inc -I@ix@/libix
#libreflex_la_LIBADD =  -L/usr/local/lib -levent -levent_core -levent_pthreads -levent_extra -levent_openssl -L@ix@/deps/dpdk/x86_64-native-linuxapp-gcc/lib -lrte_eal -lrte_mbuf -lrte_mempool -lrte_ring -lrte_pmd_ixgbe -lethdev
#@ix@/deps/dpdk/x86_64-native-linuxapp-gcc/lib/librte_eal.so @ix@/deps/dpdk/x86_64-native-linuxapp-gcc/lib/librte_mbuf.so @ix@/deps/dpdk/x86_64-native-linuxapp-gcc/lib/librte_mempool.so @ix@/deps/dpdk/x86_64-native-linuxapp-gcc/lib/librte_ring.so @ix@/deps/dpdk/x86_64-native-linuxapp-gcc/lib/librte_pmd_ixgbe.so @ix@/deps/dpdk/x86_64-native-linuxapp-gcc/lib/libethdev.so
#libreflex_la_LIBADD = -L/usr/local/lib -levent -levent_core -levent_pthreads -levent_extra -levent_openssl 
#-L@ix@/libix -lix -L@ix@/deps/dpdk/build/lib -lrte_eal -lrte_mempool -lrte_ring
#libreflex_la_LDFLAGS = -Wl,--whole-archive @ix@/libix/libix.a @ix@/dp/libixreflex.a -Wl,--no-whole-archive @ix@/deps/dpdk/x86_64-native-linuxapp-gcc/lib/librte_eal.so @ix@/deps/dpdk/x86_64-native-linuxapp-gcc/lib/librte_mbuf.so @ix@/deps/dpdk/x86_64-native-linuxapp-gcc/lib/librte_mempool.so @ix@/deps/dpdk/x86_64-native-linuxapp-gcc/lib/librte_ring.so @ix@/deps/dpdk/x86_64-native-linuxapp-gcc/lib/librte_pmd_ixgbe.so @ix@/deps/dpdk/x86_64-native-linuxapp-gcc/lib/libethdev.so

#libreflex_la_LDFLAGS = -L@ix@/libix -Wl,--whole-archive -lix -Wl,--no-whole-archive
#libreflex_la_LDFLAGS = -Wl,--whole-archive -L@ix@/libix -lix -L@ix@/deps/dpdk/build/lib -lrte_eal -lrte_mempool -lrte_ring -Wl,--no-whole-archive
#libreflex_la_LDFLAGS = -Wl,--whole-archive, -L@ix@/dp -lixreflex -L@ix@/libix -lix -L@ix@/deps/dpdk/x86_64-native-linuxapp-gcc/lib -lrte_eal -lrte_mbuf -lrte_mempool -lrte_ring -lrte_pmd_ixgbe -lethdev   --no-whole-archive
#libreflex_la_LDFLAGS = -Wl,--whole-archive,@ix@/dp/libixreflex.a,@ix@/libix/libix.a,@ix@/deps/dpdk/build/lib/librte_eal.a,@ix@/deps/dpdk/build/lib/librte_mempool.a,@ix@/deps/dpdk/build/lib/librte_ring.a,--no-whole-archive


include_HEADERS = src/stanford_mast_reflex_NativeDispatcher.h
libreflex_la_SOURCES = src/stanford_mast_reflex_NativeDispatcher.cpp
