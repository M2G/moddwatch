package moddwatch

/*
#cgo darwin CFLAGS: -I/opt/homebrew/include -I/usr/local/include
#cgo darwin LDFLAGS: -L/opt/homebrew/lib -L/usr/local/lib -lfswatch -lpthread
#cgo linux CFLAGS: -I/usr/local/include
#cgo linux LDFLAGS: -L/usr/lib/x86_64-linux-gnu/libfswatch -L/usr/local/lib -lfswatch -lpthread -Wl,-rpath,/usr/lib/x86_64-linux-gnu/libfswatch -Wl,-rpath,/usr/local/lib
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

	"github.com/M2G/moddwatch/filter"
)

// MaxLullWait is the maximum time to wait for a lull. This only kicks in if
// we've had a constant stream of modifications blocking us.
const MaxLullWait = time.Second * 8

// isUnder takes two absolute paths, and returns true if child is under parent.
func isUnder(parent string, child string) bool {
	parent = filepath.ToSlash(parent)
	child = filepath.ToSlash(child)
	off := strings.Index(child, parent)
	return off == 0 && (len(child) == len(parent) || child[len(parent)] == '/')
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
	return len(mod.Changed)+len(mod.Deleted)+len(mod.Added) == 0
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
	newincludes := slices.Clone(includePatterns)
	for i, v := range includePatterns {
		bdir, trailer := filter.SplitPattern(v)
		if !filepath.IsAbs(bdir) {
			bdir = filepath.Join(root, filepath.FromSlash(bdir))
		}
		if stat, err := os.Lstat(bdir); err == nil && stat.Mode()&os.ModeSymlink != 0 {
			// The file exists and is a symlink, so we rebase the include
			// patterns and the base directory
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
			// Everything else: doesn't exist, or exists but isn't a symlink -
			// same fallback either way.
			bdir = enclosingDir(bdir)
			if bdir == "" {
				bdir = root
			}
		}
		bases[i] = bdir
	}
	return newincludes, bases
}

type rawEvent struct {
	kind      string // "added", "changed", "deleted"
	cleanpath string
	abspath   string
}

// Watcher is a handle that allows a Watch to be terminated
type Watcher struct {
	session  *C.mw_session
	handle   cgo.Handle
	modch    chan *Mod
	rawch    chan rawEvent
	done     chan struct{} // closed by Stop: unblocks every send
	loopDone chan struct{} // closed once batchLoop has exited and closed modch
	stopOnce sync.Once
	includes []string
	excludes []string
	known    map[string]bool
	lullTime time.Duration
	root     string

	sync.Mutex // protège known
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
	select {
	case w.modch <- m:
	case <-w.done:
	}
}

// Stop watching, and close the channel passed to watch. This function can
// safely be called concurrently.
func (w *Watcher) Stop() {
	w.stopOnce.Do(func() {
		// Unblock the fswatch callback and batchLoop if they are stuck sending.
		close(w.done)

		C.mw_session_stop(w.session) // joins the monitor thread: no more callbacks after this
		C.mw_session_destroy(w.session)
		w.handle.Delete()

		<-w.loopDone // batchLoop has exited and closed modch
	})
}

//export goEventTrampoline
func goEventTrampoline(cpath *C.char, created, updated, removed, renamed C.int, userData C.uintptr_t) {
	h := cgo.Handle(userData)
	w, ok := h.Value().(*Watcher)
	if !ok {
		return
	}
	path := C.GoString(cpath)
	abspath := filepath.Join(w.root, path)

	cleanpath, err := filter.File(path, w.includes, w.excludes)
	if err != nil || cleanpath == "" {
		return
	}

	var kind string
	switch {
	case created != 0:
		kind = "changed"
		if !w.isKnown(cleanpath) {
			kind = "added"
			w.markKnown(cleanpath)
		}
	case updated != 0:
		kind = "changed"
		w.markKnown(cleanpath)
	case removed != 0:
		kind = "deleted"
		w.markUnknown(cleanpath)
	case renamed != 0:
		if _, err := os.Stat(abspath); err == nil {
			kind = "added"
			w.markKnown(cleanpath)
		} else {
			kind = "deleted"
			w.markUnknown(cleanpath)
		}
	default:
		return
	}

	select {
	case w.rawch <- rawEvent{kind: kind, cleanpath: cleanpath, abspath: abspath}:
	case <-w.done:
	}
}

func pathExists(p string) bool {
	fi, err := os.Stat(p)
	return err == nil && !fi.IsDir()
}

func keysFromPathMap(m map[string]string) []string {
	keys := make([]string, 0, len(m))
	for k := range m {
		keys = append(keys, k)
	}
	slices.Sort(keys)
	return keys
}

// resolveBatch turns a set of raw, accumulated per-category changes into a
// single Mod, resolving inconsistencies (e.g. a path both added and removed
// within the same lull window) against the file's actual current state on
// disk - the same principle mkmod applied in the original implementation.
func resolveBatch(added, removed, changed map[string]string) Mod {
	for k, abspath := range added {
		if pathExists(abspath) {
			delete(changed, k)
			delete(removed, k)
		} else {
			delete(added, k)
			delete(removed, k)
			delete(changed, k)
		}
	}
	for k, abspath := range removed {
		if pathExists(abspath) {
			delete(removed, k)
		} else {
			delete(added, k)
			delete(changed, k)
		}
	}
	return Mod{
		Added:   keysFromPathMap(added),
		Changed: keysFromPathMap(changed),
		Deleted: keysFromPathMap(removed),
	}
}

// batchLoop accumulates raw events from rawch until there is a lull in the
// stream of changes of duration lullTime (or MaxLullWait is reached under a
// constant stream of modifications), then emits a single merged Mod on
// modch. This mirrors the batching behaviour of the original notify-based
// implementation, adapted to fswatch's per-event CGo callback delivery.
func (w *Watcher) batchLoop() {
	defer close(w.loopDone)
	defer close(w.modch)

	added := make(map[string]string)
	removed := make(map[string]string)
	changed := make(map[string]string)
	pending := false

	// lull fires lullTime after the last event; maxWait fires MaxLullWait
	// after the first event of the batch.
	lull := time.NewTimer(w.lullTime)
	lull.Stop()
	maxWait := time.NewTimer(MaxLullWait)
	maxWait.Stop()

	flush := func() {
		lull.Stop()
		maxWait.Stop()
		pending = false
		m := resolveBatch(added, removed, changed)
		added = make(map[string]string)
		removed = make(map[string]string)
		changed = make(map[string]string)
		if !m.Empty() {
			w.send(&m)
		}
	}

	for {
		select {
		case <-w.done:
			return
		case evt := <-w.rawch:
			if !pending {
				pending = true
				maxWait.Reset(MaxLullWait)
			}
			lull.Reset(w.lullTime)
			switch evt.kind {
			case "added":
				added[evt.cleanpath] = evt.abspath
			case "changed":
				changed[evt.cleanpath] = evt.abspath
			case "deleted":
				removed[evt.cleanpath] = evt.abspath
			}
		case <-lull.C:
			flush()
		case <-maxWait.C:
			flush()
		}
	}
}

// Watch watches a set of include and exclude patterns relative to a given root.
// Mod structs representing discrete changesets are sent on the channel ch.
//
// Watch applies heuristics to cope with transient files and unreliable event
// notifications. Modifications are batched up until there is a lull in the
// stream of changes of duration lullTime. This lets us represent processes
// that progressively affect multiple files, like rendering, as a single
// changeset. Watch also keeps track of paths already known to exist so a
// Created event on an already-known path (a quirk of some platforms, notably
// FSEvents on macOS) is reported as Changed instead of Added.
//
// All paths emitted are slash-delimited and normalised. If a path lies under
// the specified root, it is converted to a path relative to the root, otherwise
// the returned path is absolute.
//
// Pattern syntax is as follows:
//
//   - any sequence of non-path-separators
//     **             any sequence of characters, including path separators
//     ?              any single non-path-separator character
//     [class]        any single non-path-separator character against a class
//     of characters (see below)
//     {alt1,...}     a sequence of characters if one of the comma-separated
//     alternatives matches
//
//     Any character with a special meaning can be escaped with a backslash (\).
//
// Character classes support the following:
//
//	[abc]		any single character within the set
//	[a-z]		any single character in the range
//	[^class] 	any single character which does not match the class
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

	w := &Watcher{
		session:  session,
		modch:    ch,
		rawch:    make(chan rawEvent, 4096),
		done:     make(chan struct{}),
		loopDone: make(chan struct{}),
		includes: newincludes,
		excludes: excludes,
		known:    known,
		lullTime: lullTime,
		root:     root,
	}
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

	go w.batchLoop()
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
