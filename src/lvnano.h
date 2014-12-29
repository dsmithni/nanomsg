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

#ifndef lvnano_h
#define lvnano_h

#include "nn.h"
#include <extcode.h>

//instance data pointer for labview's use
typedef struct {
	int s;
} lvnano_inst, *lvnano_ptr, **lvnano_hdl;

#define idp2s(x) (((lvnano_ptr)(*x))->s)

//actual functions
NN_EXPORT int lvnano_receive(int s, LStrHandle h, int flags, InstanceDataPtr * idp);

NN_EXPORT int lvnano_send(int s, LStrHandle h, int flags, InstanceDataPtr * idp);


//labview CLFN callbacks
NN_EXPORT MgErr lvnano_alloc(InstanceDataPtr * idp);

NN_EXPORT MgErr lvnano_dealloc(InstanceDataPtr * idp);

NN_EXPORT MgErr lvnano_abort(InstanceDataPtr * idp);


#endif //lvnano_h