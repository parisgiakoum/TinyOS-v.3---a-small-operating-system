#include "kernel_streams.h"
#include "tinyos.h"
#include "kernel_cc.h"

file_ops __pipe_read_ops = {
		.Open = NULL,
		.Read = pipe_read,
		.Write = false_return,
		.Close = pipe_close
	};
	file_ops __pipe_write_ops = {
		.Open = NULL,
		.Read = false_return,
		.Write = pipe_write,
		.Close = pipe_close
	};

int Pipe(pipe_t* pipe)
{
	Fid_t fid[2];
	FCB *fcb[2];
	Mutex_Lock(&kernel_mutex);

	if(! FCB_reserve(2, fid, fcb)){	//Reserve 2 FCB's
		fid[0] = NOFILE;
		fid[1] = NOFILE;
		Mutex_Unlock(&kernel_mutex);
		fprintf(stderr, "Could not reserve FCB");
		return -1;
	}
	pipe->read=fid[0];
	pipe->write=fid[1];

	PipeCB* pipecb = xmalloc(sizeof(PipeCB));
	pipecb->pipe_ptr = pipe;
	pipecb->fcbr = fcb[0];
	pipecb->fcbw = fcb[1];
	pipecb->start = &pipecb.buffer[0];
	pipecb->end = &pipecb.buffer[0];

	pipecb->rCV=COND_INIT;
	pipecb->wCV=COND_INIT;

	// Reader
	fcb[0]->streamobj = &pipecb;
	fcb[0]->streamfunc = &__pipe_read_ops;
	//Writer
	fcb[1]->streamobj = &pipecb;
	fcb[1]->streamfunc = &__pipe_write_ops;

	Mutex_Unlock(&kernel_mutex);
	return 0;
}
//TO-DO
int false_return(void* this, char *buf, unsigned int size){
	return -1;
}

int pipe_read(void* this, char *buf, unsigned int size){
	return 0;
}

int pipe_write(void* this, const char *buf, unsigned int size){

	return 0;
}

int pipe_close(){
	return 0;
}
