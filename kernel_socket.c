
#include "tinyos.h"
#include "kernel_cc.h"
#include "kernel_streams.h"

SCB* PortT[MAX_PORT+1];


void initialize_ports() {
	/* initialize the ports */
	  for(port_t p=0; p<=MAX_PORT; p++) {
	    PortT[p] = NULL;
	  }
}

SCB* get_scb(Fid_t sock)
{
	if (sock==NOFILE) return NULL;

	SCB* socket;
	FCB* sock_fcb = get_fcb(sock);
	if (sock_fcb == NULL) return NULL;

	socket = sock_fcb->streamobj;

	return socket;
}

file_ops sock_ops = {
		.Close = socket_close
};

Fid_t Socket(port_t port)
{
	Fid_t fid;
	FCB *fcb;

	if(port < NOPORT || port > MAX_PORT)
		return NOFILE;


	Mutex_Lock(&kernel_mutex);
	if(!FCB_reserve(1, &fid, &fcb)){
			fid = NOFILE;
			Mutex_Unlock(&kernel_mutex);
			//fprintf(stderr, "Could not reserve FCB for Socket\n");
			return fid;
	}
	SCB* sock = xmalloc(sizeof(SCB));
	fcb->streamfunc = &sock_ops;
	fcb->streamobj = sock;

	sock->lcb = NULL;
	sock->peercb = NULL;
	sock->fcb = fcb;
	sock->fid = fid;
	sock->port = port;
	sock->refcount=0;
	sock->type = UNBOUND;

	Mutex_Unlock(&kernel_mutex);

	return fid;
}

int Listen(Fid_t sock)
{
	SCB* scb;
	int retcode = -1;

	Mutex_Lock(&kernel_mutex);
	scb = get_scb(sock);

	if(scb == NULL || PortT[scb->port] != NULL || scb->type != UNBOUND || scb->port == NOPORT) {
		Mutex_Unlock(&kernel_mutex);
		return retcode;
	}

	LCB* lcb = xmalloc(sizeof(LCB));
	scb->type = LISTENER;

	rlnode_new(&lcb->requests);

	lcb->wait_cv = COND_INIT;
	scb->lcb = lcb;


	PortT[scb->port] = scb;
	retcode = 0;

	Mutex_Unlock(&kernel_mutex);

	return retcode;
}


Fid_t Accept(Fid_t lsock)
{
	SCB* listener;

	Mutex_Lock(&kernel_mutex);
	if(lsock <0 || lsock > MAX_FILEID){
		Mutex_Unlock(&kernel_mutex);
		return -1;
	}

	listener = get_scb(lsock);

	if(listener == NULL || listener->type != LISTENER) {
			Mutex_Unlock(&kernel_mutex);
			return -1;
	}

	while(is_rlist_empty(&listener->lcb->requests)){
		Cond_Wait(&kernel_mutex, &listener->lcb->wait_cv);
	}

	rlnode* s3_node = rlist_pop_front(&listener->lcb->requests);

	msg* mes = (msg*)s3_node->obj;
	SCB* s3 = mes.s3;

	s3.type = PEER;

	SCB* s2;
	s2->peercb->cv = COND_INIT;
	s2->port = listener->port;
	s2->type = PEER;

	pipe_t* pipe_in;
	pipe_t* pipe_out;

	//Error checking
	Pipe(pipe_in);
	Pipe(pipe_out);

	s2->peercb->pipes.read = pipe_in->read;
	s2->peercb->pipes.write = pipe_out->write;

	s3->peercb->pipes.read = pipe_in->write;
	s3->peercb->pipes.write = pipe_out->read;

	mes->result = 0;



	return NOFILE;
}


int Connect(Fid_t sock, port_t port, timeout_t timeout)
{

	SCB* scb3;
	Fid_t s2;

	Mutex_Lock(&kernel_mutex);
	scb3 = get_scb(sock);

	if(scb3 == NULL || port <= NOPORT || port > MAX_PORT || PortT[scb3->port] == NULL) {
		Mutex_Unlock(&kernel_mutex);
		return -1;
	}
	//MSG
	msg msg;
	msg.s3 = scb3;
	msg.result = -1;

	rlnode node;
	rlnode_init(&node, &msg);

	PeerCB* peer;
	peer = (PeerCB*) xmalloc(sizeof(PeerCB));
	peer->cv = COND_INIT;

	scb3->peercb = peer;

	//Insert to the listeners list
	rlist_push_back(&PortT[port]->lcb->requests, &node);

	Cond_Signal(&PortT[port]->lcb->wait_cv);

	Cond_Wait(&kernel_mutex, &scb3->peercb->cv);

	if(msg.result == -1)
		fprintf(stderr, "\n\nMessage = -1. Could not connect.\n\n");


	Mutex_Unlock(&kernel_mutex);


	return msg.result;
}


int ShutDown(Fid_t sock, shutdown_mode how)
{
	return -1;
}

int socket_close(void *this)
{
/* FIX CLOSE */

	SCB* scb = (SCB *)this;

	if (PortT[scb->port] != NULL) {
		PortT[scb->port] = NULL;
		scb->type = UNBOUND;
		//ti ginete me requests, cv?
	}

	return 0;
}
