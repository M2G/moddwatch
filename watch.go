package moddwatch

import (
	"fmt"
	"maps"
	"os"
	"path/filepath"
	"slices"
	"strings"
	"sync"
	"time"

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

// An existenceChecker checks the existence of a file
type existenceChecker interface {
	Check(p string) bool
}

type statExistenceChecker struct{}

func (sc statExistenceChecker) Check(p string) bool {
	fi, err := os.Stat(p)
	if err == nil && !fi.IsDir() {
		return true
	}
	return false
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

type fset map[string]bool

func mkmod(exists existenceChecker, added fset, removed fset, changed fset, renamed fset) Mod {
	ret := Mod{}
	for k := range renamed {
		if exists.Check(k) {
			added[k] = true
		} else {
			removed[k] = true
		}
	}
	for k := range added {
		if exists.Check(k) {
			delete(changed, k)
			delete(removed, k)
		} else {
			delete(added, k)
			delete(removed, k)
			delete(changed, k)
		}
	}
	for k := range removed {
		if exists.Check(k) {
			delete(removed, k)
		} else {
			delete(added, k)
			delete(changed, k)
		}
	}
	ret.Added = _keys(added)
	ret.Changed = _keys(changed)
	ret.Deleted = _keys(removed)
	return ret
}

func batch(lullTime time.Duration, maxTime time.Duration, exists existenceChecker, ch chan EventInfo) *Mod {
	added := make(map[string]bool)
	removed := make(map[string]bool)
	changed := make(map[string]bool)
	renamed := make(map[string]bool)
	hadLullMod := false
	for {
		select {
		case evt, ok := <-ch:
			if !ok {
				return nil
			}
			hadLullMod = true
			switch evt.Event() {
			case Create:
				added[evt.Path()] = true
			case Remove:
				removed[evt.Path()] = true
			case Write:
				changed[evt.Path()] = true
			case Rename:
				renamed[evt.Path()] = true
			}
		case <-time.After(lullTime):
			if hadLullMod == false {
				m := mkmod(exists, added, removed, changed, renamed)
				return &m
			}
			hadLullMod = false
		case <-time.After(maxTime):
			m := mkmod(exists, added, removed, changed, renamed)
			return &m
		}
	}
}

// Watcher is a handle that allows a Watch to be terminated
type Watcher struct {
	evtch  chan EventInfo
	modch  chan *Mod
	fsw    *FSWatcher
	closed bool

	sync.Mutex
}

func (w *Watcher) send(m *Mod) {
	w.Lock()
	defer w.Unlock()
	if !w.closed {
		w.modch <- m
	}
}

// Stop watching, and close the channel passed to Watch.
func (w *Watcher) Stop() {
	w.Lock()
	defer w.Unlock()
	if !w.closed {
		w.fsw.Stop()
		close(w.modch)
		w.closed = true
	}
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

// Watch watches a set of include and exclude patterns relative to a given root.
func Watch(
	root string,
	includes []string,
	excludes []string,
	lullTime time.Duration,
	ch chan *Mod,
) (*Watcher, error) {
	evtch := make(chan EventInfo, 4096)
	newincludes, paths := baseDirs(root, includes)

	fsw, err := NewFSWatcher()
	if err != nil {
		return nil, fmt.Errorf("could not create watcher: %w", err)
	}

	for _, p := range paths {
		if err := fsw.Add(p); err != nil {
			fsw.Stop()
			return nil, fmt.Errorf("could not watch path %q: %w", p, err)
		}
	}

	fsw.Start()

	w := &Watcher{evtch: evtch, modch: ch, fsw: fsw}

	go func() {
		for ev := range fsw.Events {
			evtch <- ev
		}
		close(evtch)
	}()

	go func() {
		for {
			b := batch(lullTime, MaxLullWait, statExistenceChecker{}, evtch)
			if b == nil {
				return
			} else if !b.Empty() {
				b, err := b.normPaths(root)
				if err != nil {
					continue
				}
				b, err = b.Filter(root, newincludes, excludes)
				if err != nil {
					continue
				}
				if !b.Empty() {
					w.send(b)
				}
			}
		}
	}()
	return w, nil
}

// List all files under the root that match the specified patterns.
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
