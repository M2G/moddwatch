//go:build linux

package moddwatch

/*
#cgo CFLAGS: -I/usr/local/include
#cgo LDFLAGS: -L/usr/local/lib -lfswatch -lstdc++ -lintl
#include "fswatch_bridge.h"
#include <stdlib.h>
*/
import "C"
import (
	"fmt"
	"time"
	"unsafe"
)

type fswEvent struct {
	event Event
	path  string
}

func (e fswEvent) Event() Event     { return e.event }
func (e fswEvent) Path() string     { return e.path }
func (e fswEvent) Sys() interface{} { return nil }

type FSWatcher struct {
	handle C.MODDWATCH_HANDLE
	Events chan EventInfo
	done   chan struct{}
}

func NewFSWatcher() (*FSWatcher, error) {
	h := C.moddwatch_create()
	if h == nil {
		return nil, fmt.Errorf("moddwatch_create: failed to initialise libfswatch session")
	}
	return &FSWatcher{
		handle: h,
		Events: make(chan EventInfo, 4096),
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

func (w *FSWatcher) Start() {
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
				var ev Event
				switch uint32(flags) {
				case 1:
					ev = Create
				case 2:
					ev = Write
				case 3:
					ev = Remove
				case 4:
					ev = Rename
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
