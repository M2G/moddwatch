package moddwatch

/*
#cgo darwin CFLAGS: -I/opt/homebrew/include -I/usr/local/include
#cgo darwin LDFLAGS: -L/opt/homebrew/lib -L/usr/local/lib -lfswatch -lpthread
#cgo linux LDFLAGS: -L/usr/lib/x86_64-linux-gnu/libfswatch -lfswatch -lpthread -Wl,-rpath,/usr/lib/x86_64-linux-gnu/libfswatch
#include "mw_watch.h"
#include "cshim.h"
#include <stdlib.h>
*/
import "C"

import (
	"fmt"
	"maps"
	"os"
	"path/filepath"
	"runtime/cgo"
	"slices"
	"strings"
	"sync"
	"time"
	"unsafe"

	"moddwatch/filter"
)

// isUnder takes two absolute paths, and returns true if child is under parent.
func isUnder(parent string, child string) bool {
	parent = filepath.ToSlash(parent)
	child = filepath.ToSlash(child)
	off := strings.Index(child, parent)
	if off == 0 && (len(child) == len(parent) || child[len(parent)] == '/') {
		return true
	}
	return false
}

func normPaths(root string, abspaths []string) ([]string, error) {
	aroot, err := filepath.Abs(root)
	if err != nil {
		return nil, err
	}
	ret := make([]string, len(abspaths))
	for i, p := range abspaths {
		norm, err := filepath.Abs(p)
		if err != nil {
			return nil, err
		}
		if isUnder(aroot, norm) {
			norm, err = filepath.Rel(aroot, norm)
			if err != nil {
				return nil, err
			}
		}
		ret[i] = filepath.ToSlash(norm)
	}
	return ret, nil
}

// Mod encapsulates a set of changes
type Mod struct {
	Changed []string
	Deleted []string
	Added   []string
}

func (mod Mod) String() string {
	return fmt.Sprintf(
		"Added: %v\nDeleted: %v\nChanged: %v",
		mod.Added, mod.Deleted, mod.Changed,
	)
}

// All returns a single list of all files changed or added - deleted files are
// not included.
func (mod Mod) All() []string {
	all := make(map[string]bool)
	for _, p := range mod.Changed {
		all[p] = true
	}
	for _, p := range mod.Added {
		all[p] = true
	}
	return _keys(all)
}

// Has checks if a given Mod includes a specified file
func (mod Mod) Has(p string) bool {
	for _, v := range mod.All() {
		if filepath.Clean(p) == filepath.Clean(v) {
			return true
		}
	}
	return false
}

// Empty checks if this mod set is empty
func (mod Mod) Empty() bool {
	if (len(mod.Changed) + len(mod.Deleted) + len(mod.Added)) > 0 {
		return false
	}
	return true
}

func joinLists(a []string, b []string) []string {
	m := map[string]bool{}
	for _, v := range a {
		m[v] = true
	}
	for _, v := range b {
		m[v] = true
	}
	ret := slices.Collect(maps.Keys(m))
	slices.Sort(ret)
	return ret
}

// Join two Mods together, resulting in a new structure where each modification
// list is sorted alphabetically.
func (mod Mod) Join(b Mod) Mod {
	return Mod{
		Changed: joinLists(mod.Changed, b.Changed),
		Deleted: joinLists(mod.Deleted, b.Deleted),
		Added:   joinLists(mod.Added, b.Added),
	}
}

// Filter applies a filter, returning a new Mod structure
func (mod Mod) Filter(root string, includes []string, excludes []string) (*Mod, error) {
	changed, err := filter.Files(mod.Changed, includes, excludes)
	if err != nil {
		return nil, err
	}
	deleted, err := filter.Files(mod.Deleted, includes, excludes)
	if err != nil {
		return nil, err
	}
	added, err := filter.Files(mod.Added, includes, excludes)
	if err != nil {
		return nil, err
	}
	return &Mod{Changed: changed, Deleted: deleted, Added: added}, nil
}

func (mod *Mod) normPaths(root string) (*Mod, error) {
	changed, err := normPaths(root, mod.Changed)
	if err != nil {
		return nil, err
	}
	deleted, err := normPaths(root, mod.Deleted)
	if err != nil {
		return nil, err
	}
	added, err := normPaths(root, mod.Added)
	if err != nil {
		return nil, err
	}
	return &Mod{Changed: changed, Deleted: deleted, Added: added}, nil
}

func _keys(m map[string]bool) []string {
	if len(m) == 0 {
		return nil
	}
	keys := slices.Collect(maps.Keys(m))
	slices.Sort(keys)
	return keys
}

// Find the nearest enclosing directory
func enclosingDir(path string) string {
	for {
		if stat, err := os.Lstat(path); err == nil {
			if stat.IsDir() {
				return path
			}
		}
		if path == "" {
			return ""
		}
		path = filepath.Dir(path)
	}
}

// Given a set of include patterns relative to a root, which directories do we
// need to monitor for changes? Returns a modified set of includes ready to pass
// to a post filter, and a set of base directories
func baseDirs(root string, includePatterns []string) ([]string, []string) {
	root = filepath.FromSlash(root)
	bases := make([]string, len(includePatterns))
	newincludes := includePatterns[:]
	for i, v := range includePatterns {
		bdir, trailer := filter.SplitPattern(v)
		if !filepath.IsAbs(bdir) {
			bdir = filepath.Join(root, filepath.FromSlash(bdir))
		}
		if stat, err := os.Lstat(bdir); err == nil {
			if stat.Mode()&os.ModeSymlink != 0 {
				// Case 1: The file exists and is a symlink, so we rebase the
				// include patterns and the base directory
				lnk, err := os.Readlink(bdir)
				if err != nil {
					continue
				}
				if filepath.IsAbs(lnk) {
					bdir = lnk
				} else {
					bdir = filepath.Join(bdir, lnk)
				}
				if trailer != "" {
					newincludes[i] = bdir + "/" + trailer
				} else {
					newincludes[i] = bdir
				}
			} else {
				// Case 2: The file exists and is not a symlink, so we leave
				// bdir unmodified.
				bdir = enclosingDir(bdir)
				if bdir == "" {
					bdir = root
				}
			}
		} else {
			bdir = enclosingDir(bdir)
			if bdir == "" {
				bdir = root
			}
		}
		bases[i] = bdir
	}
	return newincludes, bases
}

// Watcher is a handle that allows a Watch to be terminated
type Watcher struct {
	session  *C.mw_session
	handle   cgo.Handle
	modch    chan *Mod
	closed   bool
	includes []string
	excludes []string
	known    map[string]bool

	sync.Mutex
}

func (w *Watcher) isKnown(p string) bool {
	w.Lock()
	defer w.Unlock()
	return w.known[p]
}

func (w *Watcher) markKnown(p string) {
	w.Lock()
	defer w.Unlock()
	w.known[p] = true
}

func (w *Watcher) markUnknown(p string) {
	w.Lock()
	defer w.Unlock()
	delete(w.known, p)
}

func (w *Watcher) send(m *Mod) {
	w.Lock()
	defer w.Unlock()
	if !w.closed {
		w.modch <- m
	}
}

// Stop watching, and close the channel passed to watch. This function can
// safely be called concurrently.
func (w *Watcher) Stop() {
	w.Lock()
	defer w.Unlock()
	if !w.closed {
		C.mw_session_stop(w.session)
		C.mw_session_destroy(w.session)
		w.handle.Delete()
		close(w.modch)
		w.closed = true
	}
}

//export goEventTrampoline
func goEventTrampoline(cpath *C.char, created, updated, removed, renamed C.int, userData C.uintptr_t) {
	h := cgo.Handle(userData)
	w, ok := h.Value().(*Watcher)
	if !ok {
		return
	}
	path := C.GoString(cpath)

	cleanpath, err := filter.File(path, w.includes, w.excludes)
	if err != nil || cleanpath == "" {
		return
	}

	m := &Mod{}
	switch {
	case created != 0:
		if w.isKnown(cleanpath) {
			m.Changed = []string{cleanpath}
		} else {
			m.Added = []string{cleanpath}
			w.markKnown(cleanpath)
		}
	case updated != 0:
		m.Changed = []string{cleanpath}
		w.markKnown(cleanpath)
	case removed != 0:
		m.Deleted = []string{cleanpath}
		w.markUnknown(cleanpath)
	case renamed != 0:
		if _, err := os.Stat(path); err == nil {
			m.Added = []string{cleanpath}
			w.markKnown(cleanpath)
		} else {
			m.Deleted = []string{cleanpath}
			w.markUnknown(cleanpath)
		}
	default:
		return
	}
	w.send(m)
}

// Watch watches a set of include and exclude patterns relative to a given root.
// Mod structs representing discrete changesets are sent on the channel ch.
//
// Unlike the original notify-based implementation, each fswatch event is sent
// as its own distinct *Mod rather than batched over a lull period - fswatch
// has its own internal debounce (lullTime is converted to seconds and passed
// to fsw_set_latency). Watch keeps track of paths already known to exist so a
// Created event on an already-known path (a quirk of some platforms, notably
// FSEvents on macOS) is reported as Changed instead of Added.
//
// All paths emitted are slash-delimited and normalised. If a path lies under
// the specified root, it is converted to a path relative to the root, otherwise
// the returned path is absolute.
//
// Pattern syntax is as follows:
//   *              any sequence of non-path-separators
//   **             any sequence of characters, including path separators
//   ?              any single non-path-separator character
//   [class]        any single non-path-separator character against a class
//                  of characters (see below)
//   {alt1,...}     a sequence of characters if one of the comma-separated
//                  alternatives matches
//
//  Any character with a special meaning can be escaped with a backslash (\).
//
// Character classes support the following:
// 		[abc]		any single character within the set
// 		[a-z]		any single character in the range
// 		[^class] 	any single character which does not match the class
func Watch(
	root string,
	includes []string,
	excludes []string,
	lullTime time.Duration,
	ch chan *Mod,
) (*Watcher, error) {
	newincludes, _ := baseDirs(root, includes)

	initial, err := List(root, includes, excludes)
	if err != nil {
		return nil, fmt.Errorf("could not list initial files for root '%s': %s", root, err)
	}
	known := make(map[string]bool, len(initial))
	for _, p := range initial {
		known[p] = true
	}

	cRoot := C.CString(root)
	defer C.free(unsafe.Pointer(cRoot))

	latencySeconds := lullTime.Seconds()
	session := C.mw_session_create(cRoot, C.double(latencySeconds))
	if session == nil {
		return nil, fmt.Errorf("could not create fswatch session for root '%s'", root)
	}

	w := &Watcher{session: session, modch: ch, includes: newincludes, excludes: excludes, known: known}
	w.handle = cgo.NewHandle(w)

	ok := C.mw_session_start(
		session,
		(C.mw_event_callback)(unsafe.Pointer(C.mw_go_bridge)),
		C.uintptr_t(w.handle),
	)
	if !bool(ok) {
		w.handle.Delete()
		C.mw_session_destroy(session)
		return nil, fmt.Errorf("could not start fswatch monitor for root '%s'", root)
	}

	return w, nil
}

// List all files under the root that match the specified patterns. The file
// list returned is a catalogue of all files currently on disk that could occur
// in a Mod structure for a corresponding watch.
//
// All paths returned are slash-delimited and normalised. If a path lies under
// the specified root, it is converted to a path relative to the root, otherwise
// the returned path is absolute.
//
// The pattern syntax is the same as Watch.
func List(root string, includePatterns []string, excludePatterns []string) ([]string, error) {
	root = filepath.FromSlash(root)
	newincludes, bases := baseDirs(root, includePatterns)
	ret := []string{}
	for _, b := range bases {
		err := filepath.WalkDir(
			b,
			func(p string, d os.DirEntry, err error) error {
				if err != nil {
					return nil
				}
				fi, err := d.Info()
				if err != nil || fi.Mode()&os.ModeSymlink != 0 {
					return nil
				}
				cleanpath, err := filter.File(p, newincludes, excludePatterns)
				if err != nil {
					return nil
				}
				if d.IsDir() {
					m, err := filter.MatchAny(p, excludePatterns)
					if err != nil && !m {
						return filepath.SkipDir
					}
				} else if cleanpath != "" {
					ret = append(ret, cleanpath)
				}
				return nil
			},
		)
		if err != nil {
			return nil, err
		}
	}
	return normPaths(root, ret)
}