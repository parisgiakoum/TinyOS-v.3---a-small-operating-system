
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

	while(1){
		Cond_Signal(&PortT[port]->lcb->wait_cv);
		s2 = Accept(&PortT[port]->fid);
		if(s2 == NOFILE) {
			//Mutex_Unlock(&kernel_mutex);
			//return -1;
		}else{
			scb3->peercb->s2 = s2;
			scb3->peercb->s3 = sock;
			scb3->type = PEER;
			// Stin Accept S2 = PEER;
			SCB* scb2 = get_scb(s2);
			scb2->peercb->s3 = sock;
			scb2->peercb->s2 = s2;

			pipe_t pipe;
			if(!Pipe(&pipe)){

			}else{
				Mutex_Unlock(&kernel_mutex);
				return -1;
			}

			Mutex_Unlock(&kernel_mutex);
			return 0;
		}
	}
	Mutex_Unlock(&kernel_mutex);


	return 0;
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
