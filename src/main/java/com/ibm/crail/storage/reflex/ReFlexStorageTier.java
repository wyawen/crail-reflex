/*
 * Crail: A Multi-tiered Distributed Direct Access File System
 *
 * Author:
 * Jonas Pfefferle <jpf@zurich.ibm.com>
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

package com.ibm.crail.storage.reflex;

import com.ibm.crail.conf.CrailConfiguration;
import com.ibm.crail.metadata.DataNodeInfo;
import com.ibm.crail.storage.StorageEndpoint;
import com.ibm.crail.storage.StorageServer;
import com.ibm.crail.storage.reflex.client.ReFlexStorageEndpoint;
import com.ibm.crail.utils.CrailUtils;
import com.ibm.crail.storage.StorageTier;
import org.apache.commons.cli.*;
import org.slf4j.Logger;
import stanford.mast.reflex.ReFlexEndpointGroup;
import java.io.IOException;
import java.net.InetAddress;
import java.util.Arrays;

public class ReFlexStorageTier extends StorageTier {

	private static final Logger LOG = CrailUtils.getLogger();
	private ReFlexEndpointGroup clientGroup;

	public void printConf(Logger logger) {
		ReFlexStorageConstants.printConf(logger);
	}

	public void init(CrailConfiguration crailConfiguration, String[] args) throws IOException {
		ReFlexStorageConstants.updateConstants(crailConfiguration);

		if (args != null) {
			Options options = new Options();
			Option bindIp = Option.builder("a").desc("ip address to bind to").hasArg().build();
			Option port = Option.builder("p").desc("port to bind to").hasArg().type(Number.class).build();
			//Option pcieAddress = Option.builder("s").desc("PCIe address of NVMe device").hasArg().build();
			options.addOption(bindIp);
			options.addOption(port);
			//options.addOption(pcieAddress);
			CommandLineParser parser = new DefaultParser();
			try {
				CommandLine line = parser.parse(options, Arrays.copyOfRange(args, 2, args.length));
				if (line.hasOption(port.getOpt())) {
					ReFlexStorageConstants.PORT = ((Number) line.getParsedOptionValue(port.getOpt())).intValue();
				}
				if (line.hasOption(bindIp.getOpt())) {
					ReFlexStorageConstants.IP_ADDR = InetAddress.getByName(line.getOptionValue(bindIp.getOpt()));
				}
			} catch (ParseException e) {
				System.err.println(e.getMessage());
				HelpFormatter formatter = new HelpFormatter();
				formatter.printHelp("ReFlex storage tier", options);
				System.exit(-1);
			}
		}

		ReFlexStorageConstants.verify();
	}

	public StorageEndpoint createEndpoint(DataNodeInfo info) throws IOException {
		try {
			if (clientGroup == null) {
				synchronized (this) {
					if (clientGroup == null) {
						clientGroup = new ReFlexEndpointGroup();
					}
				}
			}
			return new ReFlexStorageEndpoint(clientGroup, CrailUtils.datanodeInfo2SocketAddr(info));
		} catch(Exception e){
			throw new IOException(e);
		}
	}

	@Override
	public StorageServer launchServer() throws Exception {
		return new ReFlexStorageServer();
	}

	public void close() throws Exception {

	}
}
