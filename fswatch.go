package moddwatch

/*
#cgo CFLAGS: -I/opt/homebrew/include
#cgo LDFLAGS: -L/opt/homebrew/lib -lfswatch
#include "fswatch_bridge.h"
#include <stdlib.h>
*/
import "C"
import (
	"fmt"
	"time"
	"unsafe"

	"github.com/rjeczalik/notify"
)

type fswEvent struct {
	event notify.Event
	path  string
}

func (e fswEvent) Event() notify.Event { return e.event }
func (e fswEvent) Path() string        { return e.path }
func (e fswEvent) Sys() interface{}    { return nil }

type FSWatcher struct {
	handle C.MODDWATCH_HANDLE
	Events chan notify.EventInfo
	done   chan struct{}
}

func NewFSWatcher() (*FSWatcher, error) {
	h := C.moddwatch_create()
	if h == nil {
		return nil, fmt.Errorf("moddwatch_create: failed to initialise libfswatch session")
	}
	return &FSWatcher{
		handle: h,
		Events: make(chan notify.EventInfo, 4096),
		done:   make(chan struct{}),
	}, nil
}

func (w *FSWatcher) Add(path string) error {
	cs := C.CString(path)
	defer C.free(unsafe.Pointer(cs))
	if ret := C.moddwatch_add(w.handle, cs); ret != 0 {
		return fmt.Errorf("moddwatch_add: error %d for path %q", int(ret), path)
	}
	return nil
}

// NOTE :
// Start launches the monitor in a POSIX thread (via C) and waits until it is
// ready, then starts the polling goroutine. Blocks ~300ms for kqueue init.
func (w *FSWatcher) Start() {
	// moddwatch_start spawns a pthread internally and sleeps 300ms before
	// returning - so when this call returns the monitor is live.
	C.moddwatch_start(w.handle)

	go func() {
		defer close(w.Events)
		buf := (*C.char)(C.malloc(4096))
		defer C.free(unsafe.Pointer(buf))
		var flags C.uint32_t

		for {
			select {
			case <-w.done:
				return
			default:
			}

			if ret := C.moddwatch_next(w.handle, buf, &flags); ret != 0 {
				p := C.GoString(buf)
				var ev notify.Event
				switch uint32(flags) {
				case 1:
					ev = notify.Create
				case 2:
					ev = notify.Write
				case 3:
					ev = notify.Remove
				case 4:
					ev = notify.Rename
				default:
					continue
				}
				select {
				case w.Events <- fswEvent{event: ev, path: p}:
				case <-w.done:
					return
				}
			} else {
				time.Sleep(10 * time.Millisecond)
			}
		}
	}()
}

func (w *FSWatcher) Stop() {
	close(w.done)
	C.moddwatch_destroy(w.handle)
}
