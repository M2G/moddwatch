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

type fset map[string]bool

// existenceChecker checks the existence of a file.
type existenceChecker interface {
	Check(p string) bool
}

type statExistenceChecker struct{}

// Mod encapsulates a set of changes.
type Mod struct {
	Changed []string
	Deleted []string
	Added   []string
}

type Watcher struct {
	evtch  chan EventInfo
	modch  chan *Mod
	fsw    *FSWatcher
	closed bool
	sync.Mutex
}

func (sc statExistenceChecker) Check(p string) bool {
	fi, err := os.Stat(p)
	return err == nil && !fi.IsDir()
}

func (mod Mod) String() string {
	return fmt.Sprintf("Added: %v\nDeleted: %v\nChanged: %v", mod.Added, mod.Deleted, mod.Changed)
}

func (mod Mod) Empty() bool {
	return len(mod.Changed)+len(mod.Deleted)+len(mod.Added) == 0
}

// All returns a single list of all files changed or added : deleted files are not included.
func (mod Mod) All() []string {
	m := make(map[string]bool, len(mod.Changed)+len(mod.Added))
	for _, p := range mod.Changed {
		m[p] = true
	}
	for _, p := range mod.Added {
		m[p] = true
	}
	return sortedKeys(m)
}

// Has checks if a given Mod includes a specified file.
func (mod Mod) Has(p string) bool {
	clean := filepath.Clean(p)
	for _, list := range [][]string{mod.Changed, mod.Added} {
		for _, v := range list {
			if filepath.Clean(v) == clean {
				return true
			}
		}
	}
	return false
}

// Join two Mods together, resulting in a new structure where each list is sorted alphabetically.
func (mod Mod) Join(b Mod) Mod {
	merge := func(a, b []string) []string {
		m := make(map[string]bool, len(a)+len(b))
		for _, v := range a {
			m[v] = true
		}
		for _, v := range b {
			m[v] = true
		}
		return sortedKeys(m)
	}
	return Mod{
		Changed: merge(mod.Changed, b.Changed),
		Deleted: merge(mod.Deleted, b.Deleted),
		Added:   merge(mod.Added, b.Added),
	}
}

// Filter applies a filter, returning a new Mod structure.
func (mod Mod) Filter(includes, excludes []string) (*Mod, error) {
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

func isUnder(parent, child string) bool {
	parent = filepath.ToSlash(parent)
	child = filepath.ToSlash(child)
	off := strings.Index(child, parent)
	return off == 0 && (len(child) == len(parent) || child[len(parent)] == '/')
}

func normPaths(root string, paths []string) ([]string, error) {
	aroot, err := filepath.Abs(root)
	if err != nil {
		return nil, err
	}
	ret := make([]string, len(paths))
	for i, p := range paths {
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

func sortedKeys(m map[string]bool) []string {
	return slices.Sorted(maps.Keys(m)) // slices.Sorted available in Go 1.21
}

func resolveDir(bdir, root string) string {
	for {
		if stat, err := os.Lstat(bdir); err == nil && stat.IsDir() {
			return bdir
		}
		if bdir == "" {
			return root
		}
		bdir = filepath.Dir(bdir)
	}
}

func resolveSymlink(bdir, trailer string) (string, string, error) {
	lnk, err := os.Readlink(bdir)
	if err != nil {
		return "", "", err
	}
	if !filepath.IsAbs(lnk) {
		lnk = filepath.Join(bdir, lnk)
	}
	include := lnk
	if trailer != "" {
		include = lnk + "/" + trailer
	}
	return lnk, include, nil
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
		stat, err := os.Lstat(bdir)
		if err != nil {
			bases[i] = resolveDir(bdir, root)
			continue
		}
		if stat.Mode()&os.ModeSymlink != 0 {
			resolved, include, err := resolveSymlink(bdir, trailer)
			if err != nil {
				continue
			}
			bdir = resolved
			newincludes[i] = include
		} else {
			bdir = resolveDir(bdir, root)
		}
		bases[i] = bdir
	}
	return newincludes, bases
}

func mkmod(exists existenceChecker, added, removed, changed, renamed fset) Mod {
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
	return Mod{
		Added:   sortedKeys(added),
		Changed: sortedKeys(changed),
		Deleted: sortedKeys(removed),
	}
}

func batch(lullTime, maxTime time.Duration, exists existenceChecker, ch chan EventInfo) *Mod {
	added, removed, changed, renamed := make(fset), make(fset), make(fset), make(fset)
	hadLullMod := false
	maxTimer := time.NewTimer(maxTime)
	defer maxTimer.Stop()

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
			if !hadLullMod {
				return new(mkmod(exists, added, removed, changed, renamed))
			}
			hadLullMod = false
		case <-maxTimer.C:
			return new(mkmod(exists, added, removed, changed, renamed))
		}
	}
}

// Watch watches a set of include and exclude patterns relative to a given root.
func Watch(root string, includes, excludes []string, lullTime time.Duration, ch chan *Mod) (*Watcher, error) {
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
			}
			if b.Empty() {
				continue
			}
			b, err := b.normPaths(root)
			if err != nil {
				fmt.Fprintf(os.Stderr, "moddwatch: normPaths error: %v\n", err)
				continue
			}
			b, err = b.Filter(newincludes, excludes)
			if err != nil {
				fmt.Fprintf(os.Stderr, "moddwatch: filter error: %v\n", err)
				continue
			}
			if !b.Empty() {
				w.send(b)
			}
		}
	}()
	return w, nil
}

// List all files under the root that match the specified patterns.
func List(root string, includePatterns, excludePatterns []string) ([]string, error) {
	root = filepath.FromSlash(root)
	newincludes, bases := baseDirs(root, includePatterns)
	var ret []string

	for _, b := range bases {
		err := filepath.WalkDir(b, func(p string, d os.DirEntry, err error) error {
			if err != nil {
				return nil
			}
			fi, err := d.Info()
			if err != nil || fi.Mode()&os.ModeSymlink != 0 {
				return nil
			}
			if d.IsDir() {
				if m, err := filter.MatchAny(p, excludePatterns); err != nil && !m {
					return filepath.SkipDir
				}
				return nil
			}
			if cleanpath, err := filter.File(p, newincludes, excludePatterns); err == nil && cleanpath != "" {
				ret = append(ret, cleanpath)
			}
			return nil
		})
		if err != nil {
			return nil, err
		}
	}
	return normPaths(root, ret)
}
