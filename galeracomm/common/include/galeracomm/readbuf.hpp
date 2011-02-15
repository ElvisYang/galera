#ifndef READBUF_HPP
#define READBUF_HPP

#include <galerautils.hpp>

#include <galeracomm/types.h>
#include <galeracomm/exception.hpp>
#include <galeracomm/logger.hpp>
#include <iostream>

class ReadBuf {

    mutable volatile int refcnt;
    bool instack;
    const unsigned char *buf;
    mutable unsigned char *priv_buf;
    size_t buflen;
    gu::Monitor mon;

    // Disable copy constructor
    ReadBuf (const ReadBuf&);
    // Private copy operator to disallow assignment 
    ReadBuf& operator=(ReadBuf& r);// { return r;}
    // Private destructor to disallow allocating ReadBuf from stack

public:

    ~ReadBuf() {
	if (instack == false) {
	    LOG_FATAL("~ReadBuf(): Invalid call to dtor");
#ifdef DEBUG_ASSERT
            while (1) { sleep (1); }
#endif
	    throw FatalException("Must not call dtor explicitly, object not in stack");
	}
	delete[] priv_buf;
    }

    ReadBuf(const void* buf, const size_t buflen, bool inst) :
	refcnt(1), instack(inst),
        buf(reinterpret_cast<const unsigned char *>(buf)),
        priv_buf(0), buflen(buflen), mon()
    {}
    
    
    ReadBuf(const void *buf, const size_t buflen) :
	refcnt(1), instack(false),
        buf(reinterpret_cast<const unsigned char *>(buf)),
        priv_buf(0), buflen(buflen), mon()
    {}

    ReadBuf(const void* bufs[], const size_t buflens[], const size_t nbufs, 
	    const size_t tot_len) :
	refcnt(1), instack(false), buf(0),
        priv_buf(new unsigned char[tot_len]), buflen(0), mon()
    {
	for (size_t i = 0; i < nbufs; ++i) {
	    memcpy(priv_buf + buflen, bufs[i], buflens[i]);
	    buflen += buflens[i];
	}

	if (buflen != tot_len)
	    throw FatalException("buflen != tot_len");
    }
    
    ReadBuf *copy() const {
	if (instack) {
	    ReadBuf* ret = new ReadBuf(get_buf(), get_len());
	    ret->copy();
	    ret->release();
	    return ret;
	} else {
	    gu::Critical crit(mon);
	    if (priv_buf == 0) {
		priv_buf = new unsigned char[buflen];
		memcpy(priv_buf, buf, buflen);
	    }
	    ++refcnt;
	    return const_cast<ReadBuf*>(this);
	}
    }
    
    
    ReadBuf *copy(const size_t offset) const {
	if (offset > buflen) {
            log_error << "offset: " << offset << " > buflen: " << buflen;
#ifdef DEBUG_ASSERT
            while (1) sleep (1);
#endif
            throw DException("offset > buflen");
        }

	if (offset > 0) {
	    ReadBuf *ret = new ReadBuf(get_buf(offset), buflen - offset);
	    ret->copy();
	    ret->release();
	    return ret;
	} else {
	    return copy();
	}
    }

    const void *get_buf(const size_t offset) const {
	if (offset > buflen)
	    throw FatalException("Trying to read past buffer end");
	return (priv_buf ? priv_buf : buf) + offset;
    }
    
    const void *get_buf() const {
	return priv_buf ? priv_buf : buf;
    }
    
    size_t get_len() const {
	return buflen;
    }
    
    size_t get_len(size_t off) const {
	if (off > buflen)
	    throw FatalException("Offset greater than buffer length");
	return buflen - off;
    }

    void release() {
	mon.enter();
	// log_debug << "release: " << this << ", refcnt: " << refcnt;
	assert(refcnt > 0);
	if (--refcnt == 0) {
	    instack = true;
	    mon.leave();
	    delete this; // !!!!
	} else {
	    mon.leave();
	}
    }

    int get_refcnt() const {
	return refcnt;
    }
};

#endif /* READBUF_HPP */