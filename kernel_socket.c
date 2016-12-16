
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
/*
int socket_close(void *this){
	return 0;
}

file_ops sock_ops = {
		.Close = socket_close
};
*/
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
//	fcb->streamfunc = &sock_ops;
//	fcb->streamobj = sock;
	sock->fcb = fcb;
	sock->fid = fid;
	sock->port = port;
	sock->refcount=0;
	sock->type = UNBOUND;
	sock->wait_cv = COND_INIT;


	Mutex_Unlock(&kernel_mutex);

	return fid;
}

int Listen(Fid_t sock)
{
	return -1;
}


Fid_t Accept(Fid_t lsock)
{
	return NOFILE;
}


int Connect(Fid_t sock, port_t port, timeout_t timeout)
{
	return -1;
}


int ShutDown(Fid_t sock, shutdown_mode how)
{
	return -1;
}

