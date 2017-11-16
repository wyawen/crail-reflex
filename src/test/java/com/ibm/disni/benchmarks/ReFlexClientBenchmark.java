package com.ibm.disni.benchmarks;

import org.apache.commons.cli.*;

import java.io.IOException;
import java.net.URI;

public abstract class ReFlexClientBenchmark {

	enum AccessPattern {
		SEQUENTIAL,
		RANDOM,
		SAME
	}

	abstract long run(long iterations, int queueDepth, int transferSize, AccessPattern accessPattern, boolean write) throws IOException;

	abstract void connect(URI uri) throws Exception;

	abstract void close() throws IOException;

	long IPconvertDec(String ip){
		String[] addrArray = ip.split("\\."); 
		long num = 0; 
		for (int i = 0; i < addrArray.length; i++) { 
			int power = 3 - i; 
			num += ((Integer.parseInt(addrArray[i]) % 256 * Math.pow(256, power))); 
		}
		return num;
	}

	void start(String[] args) throws Exception {
		Options options = new Options();
		Option address = Option.builder("a").required().desc("ip address").hasArg().build();
		Option port = Option.builder("p").desc("port").hasArg().type(Number.class).build();
		Option iterations = Option.builder("i").required().desc("iterations").hasArg().type(Number.class).build();
		Option queueDepth = Option.builder("qd").required().desc("queue depth").hasArg().type(Number.class).build();
		Option size = Option.builder("s").required().desc("size (bytes)").hasArg().type(Number.class).build();
		Option accessPattern = Option.builder("m").required().desc("access pattern: rand/seq/same").hasArg().build();
		Option readWrite = Option.builder("rw").required().desc("read/write").hasArg().build();

		options.addOption(address);
		options.addOption(port);
		options.addOption(iterations);
		options.addOption(queueDepth);
		options.addOption(size);
		options.addOption(accessPattern);
		options.addOption(readWrite);

		CommandLineParser parser = new DefaultParser();
		CommandLine line = null;
		HelpFormatter formatter = new HelpFormatter();
		int iterationsValue = 0;
		int queueDepthValue = 0;
		int sizeValue = 0;
		long IPaddr = 0;
		int dst_port = 0;
		try {
			line = parser.parse(options, args);
			iterationsValue = ((Number)line.getParsedOptionValue("i")).intValue();
			queueDepthValue = ((Number)line.getParsedOptionValue("qd")).intValue();
			sizeValue = ((Number)line.getParsedOptionValue("s")).intValue();
			IPaddr = IPconvertDec(line.getOptionValue("a"));
			dst_port = ((Number)line.getParsedOptionValue("p")).intValue();

		} catch (ParseException e) {
			formatter.printHelp("reflex", options);
			System.exit(-1);
		}

		URI uri = new URI("reflex://" + IPaddr + ":" + dst_port);
		connect(uri);

		AccessPattern accessPatternValue = AccessPattern.valueOf(line.getOptionValue("m"));
		String str = line.getOptionValue("rw");
		boolean write = false;

		if (str.compareTo("0") == 0) {
		//if (str.compareTo("write") == 0) {
			write = true;
		}

		System.out.println("Start running test...");
		long time = run(iterationsValue, queueDepthValue, sizeValue, accessPatternValue, write);

		System.out.println((write ? "wrote" : "read") + " " + sizeValue + "bytes with QD = " + queueDepthValue +
				", iterations = " + iterationsValue + ", pattern = " + accessPatternValue.name());
		System.out.println("------------------------------------------------");
		double timeUs = time / 1000.0;
		System.out.println("Latency = " + timeUs / iterationsValue + "us");
		double iops = (double)iterationsValue * 1000 * 1000 * 1000 / time;
		System.out.println("IOPS = " + iops);
		System.out.println("MB/s = " + iops * sizeValue / 1024.0 / 1024.0);
		System.out.println("------------------------------------------------");

		close();
	}
}
