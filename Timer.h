#ifndef TIMER_H_
#define TIMER_H_
// timer.h
//
// Copyright (c) 2011 by Andrew W. Phillips.
//
// This file is distributed under the MIT license, which basically says
// you can do what you want with it but I take no responsibility for bugs.
// See http://www.opensource.org/licenses/mit-license.php for full details.
//

#include <time.h>

class timer
{
public:
	typedef double elapsed_t;
	timer(bool go=false) { el_ = 0; if (go) { h_ = false; start_ = clock(); } else h_ = true; }
	timer(elapsed_t el) { el_ = el; h_ = true; }
	void stop() { if (!h_) el_ += elapsed_t(clock() - start_) / CLOCKS_PER_SEC; h_ = true; }
	void restart() { if (h_) start_ = clock(); h_ = false; }
	void reset(bool go=true) { el_ = 0; if (go) { h_ = false; start_ = clock(); } else h_ = true; }
	elapsed_t elapsed() const
					{ return el_ + (h_ ? 0 : elapsed_t(clock() - start_) / CLOCKS_PER_SEC); }
	// NOTE: hours(), mins() and secs() should only be called if the timer is stopped, else
	// there is a risk of incorrect reporting of time if say the minute ticks over between
	// the reporting of mins and secs (e.g. cout << t.mins() << ':' << t.secs() << endl;)
	long hours() const { if (!h_) return -1; else return long(el_)/3600; }
	int mins() const { if (!h_) return -1; else return (long(el_)%3600)/60; }
	int secs() const { if (!h_) return -1; else return long(el_)%60; }
private:
	clock_t start_;			// Time of start or last restart (if h_ is false)
	elapsed_t el_;			// Elapsed time (not including time since _start if h_ is false)
	bool h_;			// Are we currently halted?
};
#endif
