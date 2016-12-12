#include "kernel_streams.h"
#include "tinyos.h"
#include "kernel_cc.h"
#include "kernel_sched.h"

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
	pipecb->start = 0;
	pipecb->end = 0;

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
	int retcode = -1;
	PipeCB* pipe = (PipeCB*)this;

	Mutex_Lock(&kernel_mutex);
	if(pipe->fcbr != NULL){
		int i;
		for(i=0; i<size; i++){
			while(pipe->start==pipe->end){
				Cond_Signal(&pipe->wCV);
				Cond_Wait(&kernel_mutex,&pipe->rCV);
			}

			buf[i]=pipe->buffer[pipe->start];
			pipe->start = (pipe->start+1)%BUF_SIZE;
		}
		retcode = i-1;
	}

	if(pipe->fcbw->refcount == 0 && pipe->start == pipe->end)
			retcode = 0;
	Mutex_Unlock(&kernel_mutex);

	return retcode;
}

int pipe_write(void* this, const char *buf, unsigned int size){
	int retcode = -1;
	PipeCB* pipe = (PipeCB*)this;

	Mutex_Lock(&kernel_mutex);
	if(pipe->fcbw != NULL){
		int i;
		for(i=0; i<size; i++){
			while(pipe->start==pipe->end){
				Cond_Signal(&pipe->rCV);
				Cond_Wait(&kernel_mutex,&pipe->wCV);
			}

			buf[i]=pipe->buffer[pipe->start];
			pipe->start = (pipe->start+1)%BUF_SIZE;
		}
		retcode = i-1;
	}

	if(pipe->fcbr->refcount == 0 && pipe->start == pipe->end)
			retcode = 0;
	Cond_Signal(&pipe->rCV);
	Mutex_Unlock(&kernel_mutex);

	return retcode;
}

int pipe_close(void* this){
	PipeCB* pipe = (PipeCB*)this;
	if(pipe->fcbw->refcount==0 && pipe->fcbr->refcount==0)
		free(pipe);
	Mutex_Unlock(&kernel_mutex);
	return 0;
}
