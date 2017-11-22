# Crail-ReFlex (libix version)

[ReFlex](https://github.com/stanford-mast/reflex) storage backend for [Crail](https://github.com/zrlio/crail).


## Building Crail-ReFlex (libix version)

Building the source requires [Apache Maven](http://maven.apache.org/) and [GNU/autotools](http://www.gnu.org/software/autoconf/autoconf.html) and Java version 8 or higher.
To build Crail-ReFlex and its example programs, execute the following steps:

1. Compile ReFlex as a shared library, see instructions in [ReFlex](https://github.com/wyawen/reflex/tree/sharedlib)

1. Compile Crail, see instructions in [Crail README](https://github.com/zrlio/crail). You will first need to compile [DiSNI](https://github.com/zrlio/disni) and [DaRPC](https://github.com/zrlio/darpc).

2. Obtain and compile the Java sources for Crail-ReFlex, copy jar to CRAIL\_HOME jars directory: 

   ```
   git clone https://github.com/wyawen/crail-reflex.git
   cd crail-reflex
   git checkout libix 
   mvn -DskipTests install
   cp /path/to/crail-reflex/target/crail-reflex-1.0.jar /path/to/crail/assembly/target/crail-1.0-bin/jars/
   ```

3. Copy commons-cli-1.3.1.jar from Crail and all jars from disni/target to crail-reflex/target
   ```
   cp /path/to/crail/assembly/target/crail-1.0-bin/jars/commons-cli-1.3.1.jar /path/to/crail-reflex/target/
   cp /path/to/disni/target/*.jar /path/to/crail-reflex/target/   
   ```

4. Compile libreflex and add to classpath: 
   
   ```
   cd libreflex 
   ./autoprepare.sh
   ./configure --with-jdk=/path/to/jdk --with-ix=/path/to/reflex
   sudo make install
   export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/lib
   ```

A Crail-ReFlex datanode communicates with a Crail namenode. There are several options for the namenode setup. If your hardware supports RDMA, you can use the default DaRPC-based namenode in Crail. Otherwise, you can either i) setup [SoftiWARP](https://github.com/zrlio/softiwarp) for software-based RDMA support and run the default DaRPC Crail namenode or ii) run the [Crail-netty](https://github.com/zrlio/crail-netty) namenode which uses TCP/IP and does not require RDMA support.


## Running benchmarks
### Set up the environment 
   ```
   sudo su
   export JAVA_HOME=/path/to/jdk
   export CRAIL_HOME=/path/to/crail
   export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/lib
   ```

### Running a simple example

To run a simple benchmark that connects to a ReFlex server and sends I/O requests, run the following command:

   ```
   java com.ibm.disni.benchmarks.ReFlexEndpointClient -a <IP_ADDR> -p <PORT> -m <RANDOM,SEQUENTIAL> -i <NUM_ITER> -rw <READ_FRACTION> -s <REQ_SIZE_BYTES> -qd <QUEUE_DEPTH>
   ```

Set the classpath (with `-cp` option) to `/usr/local/lib:target/*`. 

### Running Crail iobench 

To run the crail benchmark using ReFlex as the storage tier, start up a ReFlex server, then clone [crail-deployment](https://github.com/patrickstuedi/crail-deployment) to set up the namenode and datanode.  Run iobench after updating libreflex.so.  
  
   ```
   cp /usr/local/lib/libreflex.so crail-deployment/crail-1.0/lib/
   example: ./bin/crail iobench -t write -f /tmp -s 1024 -k 1000 
   ```

## Running a Crail-ReFlex datanode

Set the namenode IP address and ReFlex storage tier properties in `crail-site.conf` and `core-site.xml`. 
Example `crail-site.conf` ReFlex storage tier property settings:

   ```
   crail.storage.types                     com.ibm.crail.storage.reflex.ReFlexStorageTier
   crail.storage.reflex.bindip             10.79.6.130
   crail.storage.reflex.port               1234
   ```

Start the crail-reflex datanode with the following command:

   ```
   ./bin/crail datanode -t com.ibm.crail.storage.reflex.ReFlexStorageTier 
   ```
