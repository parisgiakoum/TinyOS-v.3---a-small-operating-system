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
	if (sock==NOFILE) return NULL;	//?????

	SCB* socket;
	FCB* sock_fcb = get_fcb(sock);
	if (sock_fcb == NULL) return NULL;

	socket = sock_fcb->streamobj;

	return socket;
}

file_ops sock_ops = {
		.Open = NULL,
		.Read = false_return_read,
		.Write = false_return_write,
		.Close = socket_close
};
file_ops listener_ops = {
		.Open = NULL,
		.Read = false_return_read,
		.Write = false_return_write,
		.Close = listener_close
};
file_ops peer_ops = {
		.Open = NULL,
		.Read = socket_read,
		.Write = socket_write,
		.Close = peer_close
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

	scb->fcb->streamfunc = &listener_ops;

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
		if(lsock == NOFILE){
			Mutex_Unlock(&kernel_mutex);
			return NOFILE;
		}
		Cond_Wait(&kernel_mutex, &listener->lcb->wait_cv);
	}

	rlnode* s3_node = rlist_pop_front(&listener->lcb->requests);

	msg_packet* msg = (msg_packet*)(s3_node->obj);
	SCB* s3 = msg->sclient;

	s3->type = PEER;
	s3->fcb->streamfunc = &peer_ops;

	SCB* s2;	//Host

	Mutex_Unlock(&kernel_mutex);
	s2 = get_scb(Socket(listener->port));
	Mutex_Lock(&kernel_mutex);

	s2->type = PEER;
	s2->fcb->streamfunc = &peer_ops;

	PeerCB* peercb = xmalloc(sizeof(PeerCB));

	pipe_t* pipe_in = (pipe_t*) xmalloc(sizeof(pipe_t));
	pipe_t* pipe_out= (pipe_t*) xmalloc(sizeof(pipe_t));

	//Error checking
	Mutex_Unlock(&kernel_mutex);
	if(Pipe(pipe_in) == -1 || Pipe(pipe_out) == -1){
		Cond_Broadcast(&s3->peercb->cv);

		return NOFILE;
	}
	Mutex_Lock(&kernel_mutex);
	s2->peercb = peercb;
	s2->peercb->cv = COND_INIT;



	s2->peercb->pipes.read = pipe_in->read;
	s2->peercb->pipes.write = pipe_out->write;

	s3->peercb->pipes.read = pipe_out->read;
	s3->peercb->pipes.write = pipe_in->write;

	msg->result = 0;

	Cond_Broadcast(&s3->peercb->cv);

	Mutex_Unlock(&kernel_mutex);

	return s2->fid;
}


int Connect(Fid_t sock, port_t port, timeout_t timeout)
{
	SCB* scb3;

	Mutex_Lock(&kernel_mutex);
	scb3 = get_scb(sock);

	if(scb3 == NULL || port <= NOPORT || port > MAX_PORT || PortT[port] == NULL) {
		Mutex_Unlock(&kernel_mutex);
		return -1;
	}
	//MSG
	msg_packet* msg = (msg_packet*)xmalloc(sizeof(msg_packet));
	msg->sclient = scb3;
	msg->result = -1;

	rlnode *node = (rlnode*)xmalloc(sizeof(rlnode));
	rlnode_init(node, msg);

	PeerCB* peer;
	peer = (PeerCB*) xmalloc(sizeof(PeerCB));
	peer->cv = COND_INIT;

	scb3->peercb = peer;

	//Insert to the listeners list
	rlist_push_back(&PortT[port]->lcb->requests, node);

	Cond_Signal(&PortT[port]->lcb->wait_cv);

	Cond_Wait(&kernel_mutex, &scb3->peercb->cv);

	if(msg->result == -1)
		fprintf(stderr, "\n\nMessage = -1. Could not connect.\n\n");


	Mutex_Unlock(&kernel_mutex);

	return msg->result;
}

int ShutDown(Fid_t sock, shutdown_mode how)
{
	int retcode = -1;
	SCB* scb;
	if(sock <0 || sock > MAX_FILEID){
			return -1;
		}

	Mutex_Lock(&kernel_mutex);
	scb = get_scb(sock);
	if(how == SHUTDOWN_READ){
		FCB* piper = get_fcb(scb->peercb->pipes.read);
		if(piper->refcount == 0){
			scb->peercb->pipes.read = NOFILE;
			retcode = -1;
		}
	}else if(how == SHUTDOWN_WRITE){
		FCB* pipew = get_fcb(scb->peercb->pipes.write);
		if(pipew->refcount == 0){
			scb->peercb->pipes.write = NOFILE;
			retcode = -1;
		}
	}else if (how == SHUTDOWN_BOTH) {
		FCB* piper = get_fcb(scb->peercb->pipes.read);
		FCB* pipew = get_fcb(scb->peercb->pipes.write);

		if(piper->refcount == 0 && pipew->refcount ==0){
			scb->peercb->pipes.read = NOFILE;
			scb->peercb->pipes.write = NOFILE;
			retcode = -1;
		}
	}

	Mutex_Unlock(&kernel_mutex);
	return retcode;
}

int socket_read(void* this, char *buf, unsigned int size){
	int retcode = -1;
	SCB* sock = (SCB*)this;

	Mutex_Lock(&kernel_mutex);
	retcode = Read(sock->peercb->pipes.read ,buf, size);

	Mutex_Unlock(&kernel_mutex);
	return retcode;
}

int socket_write(void* this, const char *buf, unsigned int size){
	int retcode = -1;
	SCB* sock = (SCB*)this;

	Mutex_Lock(&kernel_mutex);
	retcode = Write(sock->peercb->pipes.write ,buf, size);

	Mutex_Unlock(&kernel_mutex);
	return retcode;
}

int socket_close(void *this)
{
/* FIX CLOSE */

	SCB* scb = (SCB *)this;

	if (PortT[scb->port] != NULL) {
		PortT[scb->port] = NULL;
		scb->type = UNBOUND;

		free(scb);
	}

	return 0;
}
int listener_close(void *this){
/* FIX CLOSE */

	SCB* scb = (SCB *)this;

	if (PortT[scb->port] != NULL) {
		PortT[scb->port] = NULL;
		scb->type = UNBOUND;

		free(scb->lcb);
		//free(scb);
	}

	return 0;
}
int peer_close(void *this){
/* FIX CLOSE */

	SCB* scb = (SCB *)this;

	if (PortT[scb->port] != NULL) {
		PortT[scb->port] = NULL;
		scb->type = UNBOUND;

		free(scb->peercb);
		//free(scb);
	}

	return 0;
}
