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
	"unsafe"
)

type FSEvent struct {
	Path  string
	Flags uint32
}

type FSWatcher struct {
	handle C.int
	Events chan FSEvent
	fsw    *FSWatcher
}

func (w *FSWatcher) loop() {
	fmt.Println("LOOP STARTED")

	for {
		ev, _ := w.Next()

		if ev != nil {

			w.Events <- *ev
		}

		if ev != nil {
			w.Events <- *ev
		}
	}
}

func NewFSWatcher() (*FSWatcher, error) {
	h := C.moddwatch_create()

	if h < 0 {
		return nil, fmt.Errorf("failed to create watcher")
	}

	w := &FSWatcher{
		handle: h,
		Events: make(chan FSEvent),
	}

	go w.loop()

	return w, nil
}

func (w *FSWatcher) Add(path string) error {
	fmt.Println("ADD PATH:", path)

	cpath := C.CString(path)
	defer C.free(unsafe.Pointer(cpath))

	ret := C.moddwatch_add(
		w.handle,
		cpath,
	)

	fmt.Println("ADD RET:", ret)

	if ret != 0 {
		return fmt.Errorf("failed to add path")
	}

	return nil
}

func (w *FSWatcher) Next() (*FSEvent, error) {
	var ev C.moddwatch_event

	ret := C.moddwatch_next(
		w.handle,
		&ev,
	)

	if ret <= 0 {
		return nil, nil
	}

	return &FSEvent{
		Path:  C.GoString(&ev.path[0]),
		Flags: uint32(ev.flags),
	}, nil
}

func (w *FSWatcher) Close() {
	C.moddwatch_destroy(w.handle)
}
