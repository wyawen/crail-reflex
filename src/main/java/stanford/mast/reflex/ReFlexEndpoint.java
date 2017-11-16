package stanford.mast.reflex;

import sun.nio.ch.DirectBuffer;
import java.net.URI;
import java.nio.ByteBuffer;
import java.io.IOException;
import java.nio.ByteOrder;

//public class ReFlexEndpoint implements DiSNIEndpoint {
public class ReFlexEndpoint { 

	private final ReFlexEndpointGroup group; 
    private final NativeDispatcher nativeDispatcher;
	private volatile boolean open;
	private final long nsSize;
	private final int nsSectorSize;
	private long dst_ip_addr; 
	private int dst_port; 

	private int ioQueueSize;

	// Completed Array (note: this is in NvmeQueuePair for nvmef) 
	private final ByteBuffer completedArray;
	private final long completedArrayAddress;
	private static final int COMPLETED_ARRAY_INDEX_OFFSET = 0;
	private static final int COMPLETED_ARRAY_START_OFFSET = 8;
	private static final int COMPLETED_ARRAY_SIZE = 1024;


	public ReFlexEndpoint(ReFlexEndpointGroup group, NativeDispatcher nativeDispatcher) {
		this.group = group;
		this.nativeDispatcher = nativeDispatcher;
		this.open = false; //newConnection != null;
		this.nsSectorSize = 512;
		this.nsSize = 0x1749a956000L; // / nsSectorSize; //for SAMSUNG device
		//this.nsSize = 0x5d27216000L / nsSectorSize; //for Intel device

		//FIXME: nvmf gets IOqueueSize when connect, and doesn't seem to constantly update it
		//       so I think it's a static max io queue size value but should check
		this.ioQueueSize = 16; 

		// "completed" array init
		this.completedArray = ByteBuffer.allocateDirect(COMPLETED_ARRAY_SIZE * 8 + COMPLETED_ARRAY_START_OFFSET);
		this.completedArray.order(ByteOrder.nativeOrder());
		this.completedArrayAddress = ((DirectBuffer)completedArray).address();
	}

	public void hello_reflex(){
		nativeDispatcher._hello_reflex();
	}

        public void close_connection(){
                nativeDispatcher._close_connection();
        }

	public void connect(URI uri) throws Exception {
		if (open){
			return;
		}
		this.dst_ip_addr = Long.parseLong(uri.getHost());
		this.dst_port = uri.getPort();
		System.out.format("call nativeDispatcher in connect...");
		nativeDispatcher._connect(dst_ip_addr, dst_port); 
		this.open = true;		
	}	

	private enum Operation {
		READ,
		WRITE
	}

	public ReFlexCommand Op(Operation op, ByteBuffer buffer, long linearBlockAddress) throws IOException {
		if (!open){
			throw new IOException("endpoint is closed");
		}
		if (buffer.remaining() % nsSectorSize != 0){
			throw new IOException("Remaining buffer a multiple of sector size");
		}
		IOCompletion completion = new IOCompletion();
		return new ReFlexCommand(this, nativeDispatcher, buffer, linearBlockAddress, completion, op == Operation.WRITE);
	}


	public ReFlexCommand write(ByteBuffer buffer, long linearBlockAddress) throws IOException{
		return Op(Operation.WRITE, buffer, linearBlockAddress);
	}
	
	public ReFlexCommand read(ByteBuffer buffer, long linearBlockAddress) throws IOException {
		System.out.print("ReFlex read..."); 	
		return Op(Operation.READ, buffer, linearBlockAddress);
	}


	public ReFlexCommand newCommand() {
		return new ReFlexCommand(this, this.nativeDispatcher, new IOCompletion());
	}

	public synchronized int processCompletions(long[] completed) throws IOException {
		
		completedArray.putInt(COMPLETED_ARRAY_INDEX_OFFSET, 0);
		int ret = nativeDispatcher._poll();
		if (ret < 0) {
			throw new IOException("nvme_qpair_process_completions failed with " + ret);
		}
		int numCompleted = completedArray.getInt(COMPLETED_ARRAY_INDEX_OFFSET);
		for (int i = 0; i < numCompleted; i++) {
			completed[i] = completedArray.getLong(i*8 + COMPLETED_ARRAY_START_OFFSET);
		}
		
		assert ret == numCompleted; //FIXME: why doesn't this trigger when it's false sometimes??

		if (ret != numCompleted) {
			System.out.format("ReflexEndpoint (disni): processCompletions ret is %d and numCompleted is %d!\n", ret, numCompleted); 
			//numCompleted = ret; //hack: for connecting			
		}

		return numCompleted;
	}

	//FIXME: ReFlex endpoint should ask the ReFlex server
	public int getSectorSize() { 
		return nsSectorSize;
	}

	//FIXME: ReFlex endpoint should ask the ReFlex server
	public long getNamespaceSize() {
		return nsSize;
	}

	public boolean isOpen() {
		return open;
	}

	public synchronized void close() throws IOException, InterruptedException {
		open = false;
	}

	public int getIOQueueSize() {
		return ioQueueSize;
	}
	
	// this function is analagous to setQueuePair in nvmef
	long getCompletedArrayAddress() { 
		return completedArrayAddress;
	}

	/*
	// FIXME: this is just for benchmark...
	public ByteBuffer allocateBuffer(int size, int alignment) {
		if (size < 0) {
			throw new IllegalArgumentException("negative size");
		}
		if (alignment < 0) {
			throw new IllegalArgumentException("negative alignment");
		}
		//long address = nativeDispatcher._malloc(size, alignment);
		long address = nativeDispatcher._malloc(size, alignment);
		if (address == 0) {
			throw new OutOfMemoryError("No more space to allocate bencharmk buffer with _malloc");
		}

		Class directByteBufferClass;
		try {
			directByteBufferClass = Class.forName("java.nio.DirectByteBuffer");
		} catch (ClassNotFoundException e) {
			throw new RuntimeException("No class java.nio.DirectByteBuffer");
		}
		Constructor<Object> constructor = null;
		try {
			constructor = directByteBufferClass.getDeclaredConstructor(Long.TYPE, Integer.TYPE);
		} catch (NoSuchMethodException e) {
			throw new RuntimeException("No constructor (long, int) in java.nio.DirectByteBuffer");
		}
		constructor.setAccessible(true);
		ByteBuffer buffer;
		try {
			buffer = (ByteBuffer)constructor.newInstance(address, size);
		} catch (InstantiationException e) {
			throw new RuntimeException(e);
		} catch (IllegalAccessException e) {
			throw new RuntimeException(e);
		} catch (InvocationTargetException e) {
			throw new RuntimeException(e);
		}

		return buffer;
	}

	//FIXME: this is just for benchmark
	public void freeBuffer(ByteBuffer buffer) {
		nativeDispatcher._free(((DirectBuffer)buffer).address());
	}
	*/

	// FIXME: functions to add:
	// public synchronized void register_tenant(SLO)
	// public synchronized void unregister_tenant()
	// public int getMaxTransferSize() 
	

    public static void main(String[] args_main) {
      
	/*	
		ReFlexEndpoint reflex = new ReFlexEndpoint();
		int numCompletions = 0;
		int req_size = 4096;
		ThreadLocal<long[]> completed; 
		completed = new ThreadLocal<long[]>() {
			public long[] initialValue() {
				//return new long[ioQeueueSize];
				return new long[16]; //FIXME: match ioQueueSize
			}
		};

			;
		long nsSize = reflex.getNamespaceSize();
		System.out.format("namespace size is %d%n", nsSize); 			
		ByteBuffer buffer = ByteBuffer.allocateDirect(req_size);
		
		try{
			reflex.connect(0x0A4F0711L, 9876);
			int num_writes = 0;
			while (true){
				long[] ca = completed.get();
				numCompletions = reflex.processCompletions(ca);
				if (numCompletions > 0 && num_writes < 3){ //NOTE: requires connect_cb to set num_completions++
					long lba = ThreadLocalRandom.current().nextLong(nsSize);
					reflex.read(buffer, lba).execute();
					//num_writes++;
					//reflex.write(buffer, lba).execute();
					
					//for read/write completions...
					  // from crail-nvmf/.../client/NvmfDataNodeEndpoint.java...
					 //for (int i = 0; i < numberCompletions; i++) {
					 //int idx = (int)ca[i];
					 //NvmeCommand command = commands[idx];
					 //IOCompletion completion = command.getCompletion();
					 //completion.done();
					 //futures[idx].signal(completion.getStatusCodeType(), completion.getStatusCode());
					 //freeCommands.add(command);
					 //}

				}
			}
		}catch(IOException e){
			e.printStackTrace();
		}
	*/
    }
}
