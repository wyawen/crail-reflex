/*
 * DiSNI: Direct Storage and Networking Interface
 *
 * Author: Jonas Pfefferle <jpf@zurich.ibm.com>
 *
 * Copyright (C) 2016, IBM Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

package stanford.mast.reflex;

//import com.ibm.disni.util.DiSNILogger;
//import org.slf4j.Logger;



public class NativeDispatcher {

	static {
		try {
		System.out.format("load libreflex....\n");
		System.loadLibrary("reflex");
		} catch (UnsatisfiedLinkError e) {
			System.err.println("Native code library failed to load.\n" + e);
		}
	}

	/* buffers returned are locked and have vtophys translation -> required for local NVMe access */
	public native long _malloc(long size, long alignment);
	public native void _free(long address);

	/* ReFlex-libevent functions */
	public native void _hello_reflex();
	public native void _close_connection();
	public native void _connect(long ip_addr, int port);
	public native int _poll(); 
	public native int _submit_io(long address, long lba, int count, long compl_addr, boolean write); 
	


}
