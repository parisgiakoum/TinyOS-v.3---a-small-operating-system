#include "kernel_streams.h"
#include "tinyos.h"
#include "kernel_cc.h"


int Pipe(pipe_t* pipe)
{
	Fid_t *fid;
	FCB* fcb;
	Mutex_Lock(&kernel_mutex);

	if(! FCB_reserve(2, fid, &fcb)){	//Reserve 2 FCB's
		fid[0] = NOFILE;
		fid[1] = NOFILE;
		Mutex_Unlock(&kernel_mutex);
		fprintf(stderr, "Could not reserve FCB");
		return -1;
	}
	pipe->read=fid[0];
	pipe->write=fid[1];

	PipeCB *pipecb;
	pipecb->reader=fid[0];
	pipecb->writer=fid[1];

	pipecb->start=&pipecb->buffer[0];
	pipecb->end=&pipecb->buffer[0];

	// Reader
	fcb[0].streamobj=pipecb;
	fcb[0].streamfunc->Open=NULL;
	fcb[0].streamfunc->Close=Pipe_close();
	fcb[0].streamfunc->Read=Pipe_read();
	fcb[0].streamfunc->Write=-1;
	//Writer
	fcb[1]->streamobj=pipecb;
	fcb[1]->streamfunc->Open=NULL;
	fcb[1]->streamfunc->Close=Pipe_close();
	fcb[1]->streamfunc->Read=-1;
	fcb[1]->streamfunc->Write=Pipe_write();

	Mutex_Unlock(&kernel_mutex);
	return 0;
}
//TO-DO
int (*Pipe_read)(){
	return 0;
}

int (*Pipe_write)(){
	return 0;
}

int (*Pipe_close)(){
	return 0;
}
