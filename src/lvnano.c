/*  
	Copyright (c) 2014 Daniel Smith  All rights reserved.

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom
    the Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included
    in all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
    THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
    IN THE SOFTWARE.
*/

#ifndef lvnano_c
#define lvnano_c
#include "nn.h"
#include "lvnano.h"
#include <extcode.h>
#include "core\global.h"
#include "utils\mutex.h"

static int lvnano_count = 0;

/*
Basically follows "zero-copy" receive method from doc files with an extra copy to move from lvland to nanoland
*/
int lvnano_receive(int s, LStrHandle h, int flags, InstanceDataPtr * idp)
{
	
	void * buf = NULL;

	//store socket in instance data pointer
	//Credit to Jack Dunaway@Wirebird Labs for the idea of doing this only on either side of the receive function
	//doing so prevents us from doing something weird if abort is pressed after receiving but before returning
	idp2s(idp) = s;
	int rlen = nn_recv(s, &buf, NN_MSG, flags);
	idp2s(idp) = -1;

	if (rlen < 0) {
		return -1 * nn_errno(); 
	}

	//now perform the resize and copy into the labview string handle h
	MgErr resize = DSSetHandleSize(h, (rlen + sizeof(int32)));
	if (resize != noErr) {
		return resize;
	}

	LStrLen(*h) = rlen;
	MoveBlock(buf, LHStrBuf(h), rlen);
	nn_freemsg(buf);

	return 0;
}

/*
Basically follows "zero-copy" send method from doc files with an extra copy to move from lvland to nanoland
*/
int lvnano_send(int s, LStrHandle h, int flags, InstanceDataPtr * idp)
{
	//todo check that lstrh is not 0 length
	
	if (h && LHStrPtr(h)) //best to check both but since the data is sent from labview as a handle by value both should exist
	{	
		
		//copy string handle into a nanomessage message for zero-copy method in doc
		void * nmsg = NULL;
		nmsg = nn_allocmsg(LHStrLen(h), 0); 
		if (!nmsg) return -1*nn_errno(); 
		//if null, errno should be ENOBUFS or something like that
		MoveBlock(LHStrBuf(h), nmsg, LHStrLen(h));

		//store socket in instance data pointer
		//Credit to Jack Dunaway@Wirebird Labs for the idea of doing this only on either side of the receive function
		//doing so prevents us from doing something weird if abort is pressed after receiving but before returning
		idp2s(idp) = s;
		int ret = nn_send(s, &nmsg, NN_MSG, flags);
		idp2s(idp) = -1;
		if (ret < 0) return -1*nn_errno();

	}
	else {
		return -1;
	}
	return 0;
}

/*
Labview init "reserve" callback function. Simply allocates space for the lvnano_inst
and increases the "lvnano_count" value by 1.
*/
MgErr lvnano_alloc(InstanceDataPtr * idp) {
	lvnano_count++;
	*idp = DSNewPClr(sizeof(lvnano_inst));
	if (!(*idp)) return mgErrStackOverflow;
	return mgNoErr;
}

/*
The "unreserve" callback. Decrements "lvnano_count" and deallocates the lv pointer for 
the instance data. If lvnano_count is 0 (meaning this is the last CLFN to unreserve)
we go ahead and call "nn_close" on every possible socket, throwing away any errors.
This is clumsy but since nanomsg no longer has an explicit context like zmq (its there
just hidden and unaccessible in global.c) its hard to make anything else work well.
*/
MgErr lvnano_dealloc(InstanceDataPtr * idp) {
	lvnano_count--;
	
	MgErr err = mgNoErr;
	idp2s(idp) = -1;
	err = DSDisposePtr(*idp);

	if (lvnano_count <= 0) {
		int i = 0;
		for (i = 0; i < NN_MAX_SOCKETS; ++i) nn_close(i);
	}
	return err;
}


/*
The critical Labview "abort" callback. Since nn_close seems to cause a fault if called while 
a blocking call is in progress, this function had to be added to global.c. It basically just
does what nn_term() does to the socket without totally killing off the global state forever
and ever and ever, which is what nn_term does.s
*/
MgErr lvnano_abort(InstanceDataPtr * idp) {
	if (idp2s(idp) >= 0) { 
		nn_close(idp2s(idp));
	}
	return mgNoErr;
}




/*rloop functions*/


void lvnano_receiver(void* p) {
	lvnano_rthread_ctx * rctxp = (lvnano_rthread_ctx*)p;

	while (!rctxp->reqAbort) {
		void * buf = NULL;
		int rlen = nn_recv(rctxp->s, &buf, NN_MSG, 0);
		if (rlen > 0) {
			lvnano_rthread_eventdata e = (lvnano_rthread_eventdata) DSNewHandle(rlen + sizeof(int32));
			LStrLen(*e) = rlen;
			MoveBlock(buf, LHStrBuf(e), rlen);
			nn_freemsg(buf);
			PostLVUserEvent((rctxp->lvevent), &e);
		}
		else if (rlen == ETERM || rlen == EINTR) {
			//someone aborted the socket
			return;
		}
	}
	return;
}

int lvnano_start_receiver(int s, lvnano_rthread_ctx ** rctxh, LVUserEventRef * lvevent) {
	struct nn_thread * rthread = (struct nn_thread *)DSNewPClr(sizeof(struct nn_thread));

	//initialize rthread context
	*rctxh = (lvnano_rthread_ctx *)DSNewPClr(sizeof(lvnano_rthread_ctx));
	(*rctxh)->s = s;
	(*rctxh)->lvevent = *lvevent;
	((*rctxh)->reqAbort) = LVFALSE;
	(*rctxh)->rthread = rthread;

	//launch thread
	nn_thread_init(rthread, lvnano_receiver, (void*)(*rctxh));

	return 0;
}

int lvnano_stop_receiver(lvnano_rthread_ctx * rctxp) {
	if (rctxp) {
		(rctxp->reqAbort) = LVTRUE;
		nn_close(rctxp->s);
		nn_thread_term(rctxp->rthread);
		DSDisposePtr(rctxp->rthread);
		DSDisposePtr(rctxp);
		return 0;
	}
	return -1;
}

//todo, list of active receivers

MgErr lvnano_rthread_alloc(InstanceDataPtr * idp) {
	return mgNoErr;
}


MgErr lvnano_rthread_dealloc(InstanceDataPtr * idp) {
	return mgNoErr;
}

MgErr lvnano_rthread_abort(InstanceDataPtr * idp) {
	MgErr err = mgNoErr;
	return mgNoErr;
}



#endif