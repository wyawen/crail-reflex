#include "stanford_mast_reflex_NativeDispatcher.h"
#include <unistd.h>


#include <sched.h>
#include <assert.h>

#include <iostream>
#include <sstream>

#include <cstdio>
#include <cstring>
#include <cerrno>
#include <cstdlib>
//#include <spdk/nvme.h>
//#include <spdk/env.h>
//#include <spdk/nvme_intel.h>
//#include <spdk/pci_ids.h>


// spdk is missing extern C in some headers
//extern "C" {
//#include <spdk/log.h>
//#include <nvme_internal.h>
//}

#include <event2/listener.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>

#include <arpa/inet.h>

#include <netinet/tcp.h>

#include <sys/prctl.h>

extern "C" {
#include <ix/env.h>
#include <ixev.h>
#include <ix/mempool.h>
#include <ix/list.h>
#include <ix/timer.h>

#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>

}

//#include <rte_config.h>
//#include <rte_lcore.h>


#define PACKAGE_NAME "stanford/mast/reflex"

#define SECTOR_SIZE 512 

#define NUM_WORKERS 1
//per worker
struct worker {
	int cpu;
	struct event_base *base;
	struct bufferevent *bev;
	pthread_t tid;
} workers[NUM_WORKERS];

struct completed_array {
    int index;
    long ids[0];
};

struct io_completion {
    int status_code_type;
    int status_code;
    const long id;
    completed_array* completed;
};

struct completion { //need to store data_handle and io_compl_addr
	void* data_handle;
	void* io_compl_addr;
	struct ixev_nvme_req_ctx ctx;
};

typedef struct __attribute__ ((__packed__)) {
  uint16_t magic;
  uint16_t opcode;
  struct completion* req_handle; //void* req_handle in ReFlex header
  unsigned long lba;
  unsigned int lba_count;
} binary_header_blk_t;

struct nvme_req {
    uint8_t cmd;
    unsigned long lba;
    unsigned int lba_count;
	long handle;
    unsigned long sent_time;
    // nvme buffer to read/write data into
    char buf[4096]; //FIXME: update for sgl
  
    struct list_node link;	
    char* payload;
    void* cb_data;
};

/*
 * Memcached protocol support 
 */

#define CMD_GET  0x00
#define CMD_SET  0x01
#define CMD_SET_NO_ACK  0x02
 
#define RESP_OK 0x00
#define RESP_EINVAL 0x04

#define REQ_PKT 0x80
#define RESP_PKT 0x81
#define MAX_EXTRA_LEN 8
#define MAX_KEY_LEN 8


int num_completions = 0;


class JNIString {
    private:
        jstring str_;
        JNIEnv* env_;
        const char* c_str_;
    public:
        JNIString(JNIEnv* env, jstring str) : str_(str), env_(env),
        c_str_(env_->GetStringUTFChars(str, NULL)) {}

        ~JNIString() {
            if (c_str_ != NULL) {
                env_->ReleaseStringUTFChars(str_, c_str_);
            }
        }

        const char* c_str() const {
            return c_str_;
        }
};



/*
 * Class:     stanford_mast_reflex_NativeDispatcher
 * Method:    _malloc
 * Signature: (JJ)J
 */
JNIEXPORT jlong JNICALL Java_stanford_mast_reflex_NativeDispatcher__1malloc
  (JNIEnv *env, jobject obj, jlong size, jlong alignment)
{
	void * ret = malloc((size_t) size);
	if (!ret){
		printf("ERROR: malloc could not allocate memory\n");
	}
	return (jlong) ret;
}

/*
 * Class:     stanford_mast_reflex_NativeDispatcher
 * Method:    _free
 * Signature: (J)V
 */
JNIEXPORT void JNICALL Java_stanford_mast_reflex_NativeDispatcher__1free
  (JNIEnv *env, jobject obj, jlong addr)
{
	free((void*) addr);
	return;
}





void connect_cb(struct bufferevent *bev, short events, void *ptr)
{
	if (events & BEV_EVENT_CONNECTED) {
		printf("connected to the server!\n");
    } else if (events & BEV_EVENT_ERROR) {
    	printf("could not connect to ReFlex server\n");
	}
}


static void set_affinity(int cpu)
{
	cpu_set_t cpu_set;
	CPU_ZERO(&cpu_set);
	CPU_SET(cpu, &cpu_set);
	if (sched_setaffinity(0, sizeof(cpu_set_t), &cpu_set) != 0) {
		perror("sched_setaffinity");
		exit(1);
	}
}

//----------------------------------------

#define BINARY_HEADER binary_header_blk_t
#define ROUND_UP(num, multiple) ((((num) + (multiple) - 1) / (multiple)) * (multiple))


static __thread int conn_opened;
static __thread int tid;
struct ip_tuple *ip_tuple[64];


struct pp_conn {
        struct ixev_ctx ctx;
        size_t rx_received;                     //the amount of data received/sent for the current ReFlex request
        size_t tx_sent;
        bool rx_pending;                        //is there a ReFlex req currently being received/sent
        bool tx_pending;
        int nvme_pending;
        long in_flight_pkts;
        long sent_pkts;
        long list_len;
        bool receive_loop;
        unsigned long seq_count;
        struct list_head pending_requests;
        long nvme_fg_handle;            //nvme flow group handle
        char data[4096 + sizeof(BINARY_HEADER)];
        char data_send[sizeof(BINARY_HEADER)];
};
static struct mempool_datastore pp_conn_datastore;
static __thread struct mempool pp_conn_pool;
struct pp_conn *conn; 

static struct mempool_datastore nvme_usr_datastore;
static __thread struct mempool req_pool;
static const int outstanding_reqs = 512; // 4096 * 8;

//fixme: hard-coding sector size for now
static int ns_sector_size = 512;




bool running = false;

/*
 * Class:     stanford_mast_reflex_NativeDispatcher
 * Method:    _hello_reflex
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_stanford_mast_reflex_NativeDispatcher__1hello_1reflex
  (JNIEnv *env, jobject obj)
{
	printf("Hello ReFlex!\n");
	
/*
	running = true;
        ixev_close(&conn->ctx);

        while (running)
                ixev_wait();
*/}






// written data to socket
void write_cb(struct pp_conn *conn) {
	printf("DEBUG> write_cb\n");
}


// read data from socket
static void receive_req(struct pp_conn *conn)
{
printf("DEBUG> receive_req\n");
	ssize_t ret;
	struct nvme_req *req;
	BINARY_HEADER* header;

/*	//-----------------------
        //1> return if received length is less than header length 
	struct evbuffer *input = bufferevent_get_input(bev);
        int length = evbuffer_get_length(input);

	//2> copy header from the response to 'header'
        binary_header_blk_t *header = new binary_header_blk_t;
        evbuffer_copyout(input, header, sizeof(binary_header_blk_t));
*/	//-----------------------
	
	while(1) {
		//1> return if received length is less than header length
		if(!conn->rx_pending) {
printf("	conn->rx_pending=false\n");
			ret = ixev_recv(&conn->ctx, &conn->data[conn->rx_received],
					sizeof(BINARY_HEADER) - conn->rx_received); 
			if (ret <= 0) {
				if (ret != -EAGAIN) {
					if(!conn->nvme_pending) {
						printf("Connection close 6\n");
						ixev_close(&conn->ctx);
					}
				}
				break;
			}
			else
				conn->rx_received += ret;

			if(conn->rx_received < sizeof(BINARY_HEADER))
				return;
		}

		// received the header
		conn->rx_pending = true;
		// 2> point 'header' to header from response 
		header = (BINARY_HEADER *)&conn->data[0];
		
		assert(header->magic == sizeof(BINARY_HEADER)); 	

		// process read response
		if (header->opcode == CMD_GET) {
printf("	process read response\n");
			//1> copy rest of response (data w/o header) to conn->data 
			ret = ixev_recv(&conn->ctx,
					&conn->data[conn->rx_received],
					sizeof(BINARY_HEADER) + header->lba_count * ns_sector_size - conn->rx_received);

			if (ret <= 0) {
				if (ret != -EAGAIN) {
					assert(0);
					if(!conn->nvme_pending) {
						printf("Connection close 7\n");
						ixev_close(&conn->ctx);
					}
				}
				break;
			}
			conn->rx_received += ret;
			
			//2> return if the received length is less than length of (header+data)
			if(conn->rx_received < (sizeof(BINARY_HEADER) + header->lba_count * ns_sector_size))
				return;

			void* addr_ctx = header->req_handle->data_handle;
			int datalen = header->lba_count * ns_sector_size;
			memcpy(addr_ctx, &conn->data[sizeof(BINARY_HEADER)], datalen);


/*			//-----------------------
 			//2> return if received length is less than length of (header+data)
			int datalen = header->lba_count * SECTOR_SIZE;
			int total_length = sizeof(binary_header_blk_t) + datalen;
			if (length < total_length) return;

			//1> copy (data w/o header) to addr_ctx (req_handle -> used as nvme_req for libix ) 
			void* addr_ctx = header->req_handle->data_handle;
			evbuffer_drain(input, sizeof(binary_header_blk_t)); //remove header from input 
			evbuffer_remove(input, addr_ctx, datalen); //Read data from an evbuffer and drain the bytes read.	
*/			//-----------------------
			
			//3> increment num-compl
			num_completions++;

			//4> update java-shared objects 
			volatile io_completion* completion;
			completion = reinterpret_cast<volatile io_completion*>(header->req_handle->io_compl_addr);

			completion->status_code = 0; //FIXME: nvme_completion->status.sc;
			completion->status_code_type = 0; //FIXME: nvme_completion->status.sct;
			completed_array* ca = completion->completed;
			ca->ids[ca->index++] = completion->id;

/*			//-----------------------
			//5> received more than one request, so process the rest
			if (length > total_length){ //received more than one request, so process the rest
				read_cb(bev, NULL);
			}
*/			//-----------------------
			
		}
		// process write response 
		else if (header->opcode == CMD_SET) {
printf("	process write response\n");
			num_completions++;

			volatile io_completion* completion;
			completion = reinterpret_cast<volatile io_completion*>(header->req_handle->io_compl_addr);

			completion->status_code = 0; //FIXME: nvme_completion->status.sc;
			completion->status_code_type = 0; //FIXME: nvme_completion->status.sct;
			completed_array* ca = completion->completed;
			ca->ids[ca->index++] = completion->id;
		}
		else {
			printf("Received unsupported command, closing connection\n");
			ixev_close(&conn->ctx);
			return;
		}

		// //struct nvme_req *req 
		//req = header->req_handle->req_handle;
	
		//mempool_free(&nvme_req_buf_pool, req->buf);
printf("	free req, set rx_pending=false, rx_received=0\n");
		mempool_free(&req_pool, req);
		conn->rx_pending = false;
		conn->rx_received = 0;	

	}

printf("done receive_req\n");
	//delete header->req_handle; //??? done in libevent before returning from a CMD_GET response
		
}

int use_list = 1;
int send_pending_client_reqs(struct pp_conn *conn);

static void main_handler(struct ixev_ctx *ctx, unsigned int reason)
{
        struct pp_conn *conn = container_of(ctx, struct pp_conn, ctx);

        if(reason == IXEVOUT) {
printf("	IXEVOUT\n");
                if(use_list) send_pending_client_reqs(conn);
        	//write_cb(conn);
	}
        else if(reason == IXEVHUP) {
printf("	IXEVHUP\n");
                printf("Connection close 5\n");
                ixev_close(ctx);
                return;
        }

printf("	IXEVIN\n");
        receive_req(conn);
}

int connected; 
static void pp_dialed(struct ixev_ctx *ctx, long ret)
{
printf("	pp_dialed called\n");
	connected = 1;	

        struct pp_conn *conn = container_of(ctx, struct pp_conn, ctx);
        unsigned long now = rdtsc();
        ixev_set_handler(&conn->ctx, IXEVIN | IXEVOUT | IXEVHUP, &main_handler);
        //running = true;
        if (tid == 0){
                printf("RqIOPS:\t IOPS:\t Avg:\t 10th:\t 20th:\t 30th:\t 40th:\t 50th:\t 60th:\t 70th:\t 80th:\t 90th:\t 95th:\t 99th:\t max:\t missed:\n");
        }

        conn_opened++;

        while(rdtsc() < now + 1000000) {}

num_completions++;
        return;
}

static void pp_release(struct ixev_ctx *ctx)
{
printf("        pp_release called\n");

        struct pp_conn *conn = container_of(ctx, struct pp_conn, ctx);
        conn_opened--;
        if(conn_opened==0)
                printf("Tid: %lx All connections released handle %lx open conns still %i\n", pthread_self(), conn->ctx.handle, conn_opened);
        mempool_free(&pp_conn_pool, conn);
        //terminate = true;
        running = false;
}

static struct ixev_ctx *pp_accept(struct ip_tuple *id)
{
printf("        pp_accept called\n");

        return NULL;
}




static struct ixev_conn_ops pp_conn_ops = {
        .accept         = &pp_accept,
        .release        = &pp_release,
        .dialed         = &pp_dialed,
};
//----------------------------------------

/*
 * Class:     stanford_mast_reflex_NativeDispatcher
 * Method:    _connect
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_stanford_mast_reflex_NativeDispatcher__1connect
  (JNIEnv *env, jobject obj, jlong ip_addr, jint port)
{
	printf("connect(): begin libixev init....\n");
	//helloworld(); //testing libix.so

	/********* TEST LIBIX **********/
	struct ixev_ctx ctx; 
	unsigned int pp_conn_pool_entries; 
        int nr_cpu, req_size_bytes;
        pthread_t thread[64];
        int tid[64];
        int i, ret;

 	int nr_threads = 1;
        nr_cpu = cpus_active;
       /* if (nr_cpu < 1) {
                fprintf(stderr, "got invalid cpu count %d\n", nr_cpu);
                exit(1);
        }

        assert(nr_threads <= nr_cpu);
        pthread_barrier_init(&barrier, NULL, nr_threads);
*/

//	printf(">>>debug: ip_addr=%ld, htonl(ip_addr)=%u\n", ip_addr, htonl(ip_addr));

	// set up ip & port 
        for (int i = 0; i < nr_threads; i++) {
                ip_tuple[i] =(struct ip_tuple*) malloc(sizeof(struct ip_tuple[i]));
                if (!ip_tuple[i])
                        exit(-1);

		ip_tuple[i]->dst_ip = ip_addr;

                timer_calibrate_tsc();

                ip_tuple[i]->dst_port = port + i;
                ip_tuple[i]->src_port = port;
                printf("Connecting to port: %i\n", port + i);
        }

        // initialize ixev 
	pp_conn_pool_entries = 16 * 4096;
        pp_conn_pool_entries = ROUND_UP(pp_conn_pool_entries, MEMPOOL_DEFAULT_CHUNKSIZE);
	ret = env_init();
	if (ret) 
		return;

	ixev_init(&pp_conn_ops);

	// create datastore for pp_conn
        ret = mempool_create_datastore(&pp_conn_datastore, pp_conn_pool_entries,
                                       sizeof(struct pp_conn), "pp_conn");
        if (ret) {
                fprintf(stderr, "unable to create mempool\n");
                return;
        }
	// create datastore for req_pool
        ret = mempool_create_datastore(&nvme_usr_datastore,
                                       outstanding_reqs * 2,
                                       sizeof(struct nvme_req),  "nvme_req_1");
        if (ret) {
                fprintf(stderr, "unable to create datastore\n");
                return;
        }


/*
	// rte_eal_remote_launch receive_lop
	for (i = 1; i < nr_cpu; i++) {
                //ret = pthread_create(&tid, NULL, start_cpu, (void *)(unsigned long) i);
                log_info("rte_eal_remote_launch...receive_loop\n");
                tid[i] = i;
                //ret = rte_eal_remote_launch(receive_loop, (void *)(unsigned long) i, i);              
                ret = rte_eal_remote_launch(receive_loop, &tid[i], i);

                if (ret) {
                        log_err("init: unable to start app\n");
                        //return -EAGAIN;
    			return;
		}
        }
*/
        //printf("Started %i threads\n", nr_cpu);
        //tid[0] = 0;

        //receive_loop(&tid[0]);

//adapted from receive_loop() --> only need to do once 
	int flags;

	//tid = *(int *)arg;
	//conn_opened = 0;	
	ret = ixev_init_thread(); //thread-local initializer: callonce per thread
	if (ret) {
		fprintf(stderr, "unable to init IXEV\n");
		return;
	};

	// create mempool for pp_conn
	ret = mempool_create(&pp_conn_pool, &pp_conn_datastore, MEMPOOL_SANITY_GLOBAL, 0);
	if (ret) {
		fprintf(stderr, "unable to create mempool\n");
		return;
	}
	// create mempool for req_pool
        ret = mempool_create(&req_pool, &nvme_usr_datastore, MEMPOOL_SANITY_GLOBAL, 0);
        if (ret) {
                fprintf(stderr, "unable to create mempool\n");
                return;
	}





	conn = (pp_conn*)mempool_alloc(&pp_conn_pool);
	if (!conn) {
		printf("MEMPOOL ALLOC FAILED !\n");
		return;
	}

	list_head_init(&conn->pending_requests);
	conn->rx_received = 0;
	conn->rx_pending = false;
	conn->tx_sent = 0;
	conn->tx_pending = false;
	conn->in_flight_pkts = 0x0UL;
	conn->sent_pkts = 0x0UL;
	//conn->list_len = 0x0UL; //??? no longer needed
	//conn->receive_loop = true; //??? no longer needed
	//conn->seq_count = 0;

	ixev_ctx_init(&conn->ctx);

	conn->nvme_fg_handle = 0; //set to this for now

	flags = fcntl(STDIN_FILENO, F_GETFL, 0);
	fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);

	//printf(">>>debug: calling ixev_dial\n");
	ixev_dial(&conn->ctx, ip_tuple[0]); //opens a connection --> need to support multiple connections? (multi-threaded)

	connected = 0; 
	while (!connected) 
		ixev_wait();

	printf("done init\n");
}


/*
 * Class:     stanford_mast_reflex_NativeDispatcher
 * Method:    _poll
 * Signature: ()V
 */
JNIEXPORT jint JNICALL Java_stanford_mast_reflex_NativeDispatcher__1poll
  (JNIEnv *env, jobject obj)
{
   	//printf("poll()\n");

/*	struct worker *worker;
   	int flags = 0;
	
	flags += EVLOOP_NONBLOCK;
	worker = &workers[0]; //FIXME: should be cpu number
*/   
	num_completions = 0;	
	//event_base_loop(worker->base, flags); --> update num_compl???
	//printf("polling\n");
	ixev_wait(); //wait for new events
	return num_completions;
}



/*
 * returns 0 if send was successfull and -1 if tx path is busy
 */
int send_client_req(struct nvme_req *req)
{
printf("debug: send_client_req\n");
//adapted from send_client_req(struct nvme_req *req)
	//struct pp_conn *conn = req->conn; 	//only 1 global connection
	int ret = 0;

	//0> save info from java
	//void* cb_data = reinterpret_cast<io_completion*>(compl_addr);
    	//void* payload = reinterpret_cast<void*>(addr);

	
	//1> set up header 
        binary_header_blk_t *header; 

	if (!conn->tx_pending){
printf("	setting up header\n");
		header = (BINARY_HEADER *)&conn->data_send[0];
		header->magic = sizeof(BINARY_HEADER); 
		/*if (is_write){ 
			header->opcode = CMD_SET;
printf("DEBUG> submit_io write\n");
		} else { 
			header->opcode = CMD_GET;		
printf("DEBUG> submit_io read\n");
		}
		header->lba = lba;
		header->lba_count = count;
		*/
		header->opcode = req->cmd; //set in submit_io
		header->lba = req->lba;    //set in submit_io
		header->lba_count = req->lba_count; //set in submit_io
		//header->req_handle = req; 
		
		header->req_handle = new completion;
       		header->req_handle->data_handle = (void *)req->payload;    //set in submit_io
        	header->req_handle->io_compl_addr = req->cb_data;  //set in submit_io

printf("	conn->tx_sent=%lu, header_size=%lu\n", conn->tx_sent, sizeof(BINARY_HEADER));

		//1.5> send header (copy header from conn->data_send to conn->ctx)
		while (conn->tx_sent < sizeof(BINARY_HEADER)) {
			ret = ixev_send(&conn->ctx, &conn->data_send[conn->tx_sent], //conn->data_send??
					sizeof(BINARY_HEADER) - conn->tx_sent);
			if (ret == -EAGAIN){
				return -1;
			}
printf("DEBUG> submit_io-send_client_req ixev_send\n");
			if (ret < 0) {
				if(!conn->nvme_pending) {
					printf("Connection close 2\n");
					ixev_close(&conn->ctx);
				}
				return -2;
			}
			conn->tx_sent += ret;
		}
	
		assert(conn->tx_sent==sizeof(BINARY_HEADER));
		conn->tx_pending = true;
		conn->tx_sent = 0;
	}

	ret = 0; //not needed?
	
	//2> add payload for write req 
//	if (is_write){
	if (req->cmd == CMD_SET) {
		//2.5> send payload (copy payload data from 'payload' to conn->ctx)
printf("	conn->tx_sent=%lu, payload_size=%u\n", conn->tx_sent, req->lba_count * ns_sector_size);
		while (conn->tx_sent < req->lba_count * ns_sector_size) {
printf("        conn->tx_sent=%lu, payload_size=%u\n", conn->tx_sent, req->lba_count * ns_sector_size);


			//assert(header->lba_count * ns_sector_size);
printf("DEBUG> submit_io-send_client_req ixev_send_zc\n");
printf("        req->payload=%x\n ", req->payload);

			ret = ixev_send_zc(&conn->ctx, &(req->payload[conn->tx_sent]),
					   req->lba_count * ns_sector_size - conn->tx_sent); 
			if (ret < 0) {
				if (ret == -EAGAIN){
					return -2;
                                }
	
				if(!conn->nvme_pending) {
					printf("Connection close 3\n");
					ixev_close(&conn->ctx);
				}
				return -2;
			}
			if(ret==0)
				printf("fhmm ret is zero\n");

			conn->tx_sent += ret;
		}
	}


	conn->tx_sent = 0;
	conn->tx_pending = false;
printf("done submit_io-send_client_req\n");

	return 0;
}



int send_pending_client_reqs(struct pp_conn *conn)
{
printf("debug: send_pending_client_reqs\n");
       // int sent_reqs = 0;
if(list_empty(&conn->pending_requests)) printf("	pending list empty\n");
        while(!list_empty(&conn->pending_requests)) {
                int ret;
                struct nvme_req *req = list_top(&conn->pending_requests, struct nvme_req, link);
                //req->sent_time = rdtsc();
printf("ix_client debug: send_pending_client_reqs calls send_client_req\n");
                ret = send_client_req(req);
                if(!ret) {
                        //sent_reqs++;
                        list_pop(&conn->pending_requests, struct nvme_req, link);
                        conn->list_len--;
                }
                else{
printf("ix_client debug: return from send_pending_client_reqs\n");
                        return ret;//sent_reqs;
                }
        }
        //return sent_reqs;
	return 0; 
}

int n_req = 1;

/*
 * Class:     stanford_mast_reflex_NativeDispatcher
 * Method:    _submit_io
 * Signature: (JJIJZ)V
 */
JNIEXPORT jint JNICALL Java_stanford_mast_reflex_NativeDispatcher__1submit_1io
  (JNIEnv *env, jobject obj, jlong addr, jlong lba, jint count, jlong compl_addr, jboolean is_write)
{

printf("DEBUG> submit_io: count=%d, n_req=\n", count);

if(use_list){
printf("	use_list\n");
        //0> save info from java
        void* cb_data = reinterpret_cast<io_completion*>(compl_addr);
        void* payload = reinterpret_cast<void*>(addr);
	
	struct nvme_req *req;

	req = (struct nvme_req *) mempool_alloc(&req_pool);
	int r = 1;
	while(!req){
		printf("        req cannot be allocated %d\n", r);
    		r++;
                //return -1;
                receive_req(conn);
                //limited qd, if we run out of req, try again later??? what to do for crail? 
               // break; ???
		req = (struct nvme_req *) mempool_alloc(&req_pool);
	}

/*
	req = (struct nvme_req *) mempool_alloc(&req_pool);
        if (!req) {
  		printf("	req cannot be allocated\n");
		//return -1;
	      	receive_req(conn);
                //limited qd, if we run out of req, try again later??? what to do for crail? 
               // break; ???
       	}

        req = (struct nvme_req *) mempool_alloc(&req_pool);
        if (!req) {
                printf("        req cannot be allocated\n");
                //return -1;
                receive_req(conn);
                //limited qd, if we run out of req, try again later??? what to do for crail?            
               // break; ???
        }
*/

 	req->lba = lba; 
        req->lba_count = count;
	if (is_write){ 
       		req->cmd = CMD_SET;
printf("DEBUG> submit_io write\n");
        } else { 
        	req->cmd = CMD_GET;               
printf("DEBUG> submit_io read\n");
	}
/*
	int size = count * ns_sector_size;
	req->payload = (char*) malloc(sizeof(char) * size);
	//req->payload = (char*)payload;
	//memcpy(req->payload, (char*)payload, size);
printf("	payload=%c, %x, %s\n", req->payload[65512], &(req->payload[65512]), req->payload);
*/

	req->payload = (char*) payload;
	req->cb_data = cb_data;
//printf("	java payload=%x addr=%x, req->payload=%x\n ", payload, addr, req->payload);

	conn->list_len++;
        list_add_tail(&conn->pending_requests, &req->link);

	send_pending_client_reqs(conn);

	return 0;

}else{

//adapted from send_client_req(struct nvme_req *req)
	//struct pp_conn *conn = req->conn; 	//only 1 global connection
	int ret = 0;

	//0> save info from java
	void* cb_data = reinterpret_cast<io_completion*>(compl_addr);
    	void* payload = reinterpret_cast<void*>(addr);

	
	//1> set up header 
        binary_header_blk_t *header; 

	if (!conn->tx_pending){
		header = (BINARY_HEADER *)&conn->data_send[0];
		header->magic = sizeof(BINARY_HEADER); 
		if (is_write){ 
			header->opcode = CMD_SET;
printf("DEBUG> submit_io write\n");
		} else { 
			header->opcode = CMD_GET;		
printf("DEBUG> submit_io read\n");
		}
		header->lba = lba;
		header->lba_count = count;
		//header->req_handle = req;
		header->req_handle = new completion;
       		header->req_handle->data_handle = payload;
        	header->req_handle->io_compl_addr = cb_data;

		//1.5> send header (copy header from conn->data_send to conn->ctx)
		while (conn->tx_sent < sizeof(BINARY_HEADER)) {
			ret = ixev_send(&conn->ctx, &conn->data_send[conn->tx_sent], //conn->data_send??
					sizeof(BINARY_HEADER) - conn->tx_sent);
			if (ret == -EAGAIN){
				return -1;
			}
printf("DEBUG> submit_io ixev_send\n");
			if (ret < 0) {
				if(!conn->nvme_pending) {
					printf("Connection close 2\n");
					ixev_close(&conn->ctx);
				}
				//return -2;
				printf("ret<0\n");
				return 0;
				ret = 0;
			}
			conn->tx_sent += ret;
		}
	
		assert(conn->tx_sent==sizeof(BINARY_HEADER));
		conn->tx_pending = true;
		conn->tx_sent = 0;
	}

	ret = 0; //not needed?
        
	//2> add payload for write req 
	if (is_write){
		//2.5> send payload (copy payload data from 'payload' to conn->ctx)
		while (conn->tx_sent < header->lba_count * ns_sector_size) {
			assert(header->lba_count * ns_sector_size);
printf("DEBUG> submit_io ixev_send_zc\n");
			ret = ixev_send_zc(&conn->ctx, payload,
					   header->lba_count * ns_sector_size - conn->tx_sent); 
			if (ret < 0) {
				if (ret == -EAGAIN){
					//return -2;
                                        printf("ret=-EAGAIN\n");
                                        return 0; 
                                }
	
				return -2;
				if(!conn->nvme_pending) {
					printf("Connection close 3\n");
					ixev_close(&conn->ctx);
				}
				//return -2;
				printf("ret<0\n");
				return 0;
			}
			if(ret==0)
				printf("fhmm ret is zero\n");

			conn->tx_sent += ret;
		}
	}


	conn->tx_sent = 0;
	conn->tx_pending = false;
printf("done submit_io\n");

	return 0;
}


	//-------libevent-------------------
/*	//0> save info from java 
	struct worker *worker;
	worker = &workers[0]; //FIXME: should be cpu number
	int pkt_len = 0;
    	int	ret = 0;
 
	void* cb_data = reinterpret_cast<io_completion*>(compl_addr);
    	void* payload = reinterpret_cast<void*>(addr);


	//1> set up header 
 	binary_header_blk_t *header;
	binary_header_blk_t* pkt = new binary_header_blk_t;
	if (!pkt) {
		printf("error: malloc for response pkt failed\n");
		return -1;
	}

	header = (binary_header_blk_t *)&pkt[0];
	header->magic = sizeof(binary_header_blk_t);
	header->req_handle = new completion;
	header->req_handle->data_handle = payload; 
	header->req_handle->io_compl_addr = cb_data; 
	header->lba = lba;
	header->lba_count = count;

	//2> differentiates read/write reqs 
	if (is_write){
		header->opcode = CMD_SET;
		
		//printf("submit_io: write\n");
		//2.1 write header to bufferevent
		pkt_len = sizeof(binary_header_blk_t);
		ret = bufferevent_write(worker->bev, (void*) pkt, pkt_len);
		if (ret != 0) {
			printf("send response failed\n");
		}
		//2.2 write payload data for write request
		pkt_len = count * SECTOR_SIZE;
		ret = bufferevent_write(worker->bev, (void*) payload, pkt_len);
		if (ret != 0) {
			printf("send response failed\n");
		}
		return 0;
	}

	// else read request...
	//printf("submit_io: read\n");
	header->opcode = CMD_GET;
	pkt_len = sizeof(binary_header_blk_t);
	ret = bufferevent_write(worker->bev, (void*) pkt, pkt_len);
	if (ret != 0) {
		printf("send response failed\n");
	}

  return 0;
*/
}
