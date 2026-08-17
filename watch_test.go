package moddwatch

import (
	"fmt"
	"os"
	"path"
	"path/filepath"
	"reflect"
	"runtime"
	"slices"
	"strings"
	"testing"
	"time"
)

// modsEqual compares two Mod values field by field. slices.Equal already
// treats a nil slice and an empty slice as equal, so no extra normalisation
// is needed (unlike reflect.DeepEqual, which would treat them as different).
func modsEqual(a, b Mod) bool {
	return slices.Equal(a.Changed, b.Changed) &&
		slices.Equal(a.Deleted, b.Deleted) &&
		slices.Equal(a.Added, b.Added)
}

// WithTempDir creates a temp directory, changes the current working directory
// to it, and returns a function that can be called to clean up. Use it like
// this:
//      defer WithTempDir(t)()
func WithTempDir(t *testing.T) func() {
	cwd, err := os.Getwd()
	if err != nil {
		t.Fatalf("TempDir: %v", err)
	}
	tmpdir, err := os.MkdirTemp("", "")
	if err != nil {
		t.Fatalf("TempDir: %v", err)
	}
	err = os.Chdir(tmpdir)
	if err != nil {
		t.Fatalf("Chdir: %v", err)
	}
	return func() {
		err := os.Chdir(cwd)
		if err != nil {
			t.Fatalf("Chdir: %v", err)
		}
		err = os.RemoveAll(tmpdir)
		if err != nil {
			t.Fatalf("Removing tmpdir: %s", err)
		}
	}
}

func abs(path string) string {
	wd, err := os.Getwd()
	if err != nil {
		panic("Could not get current working directory")
	}
	return filepath.ToSlash(filepath.Join(wd, path))
}

var isUnderTests = []struct {
	parent   string
	child    string
	expected bool
}{
	{"/foo", "/foo/bar", true},
	{"/foo", "/foo", true},
	{"/foo", "/foobar/bar", false},
}

func TestIsUnder(t *testing.T) {
	for i, tst := range isUnderTests {
		ret := isUnder(tst.parent, tst.child)
		if ret != tst.expected {
			t.Errorf("Test %d: expected %#v, got %#v", i, tst.expected, ret)
		}
	}
}

func TestMod(t *testing.T) {
	if !(Mod{}.Empty()) {
		t.Error("Expected mod to be empty.")
	}
	m := Mod{
		Added:   []string{"add"},
		Deleted: []string{"rm"},
		Changed: []string{"change"},
	}
	if m.Empty() {
		t.Error("Expected mod not to be empty")
	}
	if !reflect.DeepEqual(m.All(), []string{"add", "change"}) {
		t.Error("Unexpeced return from Mod.All")
	}

	m = Mod{
		Added:   []string{abs("add")},
		Deleted: []string{abs("rm")},
		Changed: []string{abs("change")},
	}
	if _, err := m.normPaths("."); err != nil {
		t.Error(err)
	}
}

func testListBasic(t *testing.T) {
	var findTests = []struct {
		include  []string
		exclude  []string
		expected []string
	}{
		{
			[]string{"**"},
			[]string{},
			[]string{"a/a.test1", "a/b.test2", "b/a.test1", "b/b.test2", "x", "x.test1"},
		},
		{
			[]string{"**/*.test1"},
			[]string{},
			[]string{"a/a.test1", "b/a.test1", "x.test1"},
		},
		{
			[]string{"a"},
			[]string{},
			[]string{},
		},
		{
			[]string{"x"},
			[]string{},
			[]string{"x"},
		},
		{
			[]string{"a/a.test1"},
			[]string{},
			[]string{"a/a.test1"},
		},
		{
			[]string{"**"},
			[]string{"*.test1"},
			[]string{"a/a.test1", "a/b.test2", "b/a.test1", "b/b.test2", "x"},
		},
		{
			[]string{"**"},
			[]string{"a/**"},
			[]string{"b/a.test1", "b/b.test2", "x", "x.test1"},
		},
		{
			[]string{"**"},
			[]string{"a/*"},
			[]string{"b/a.test1", "b/b.test2", "x", "x.test1"},
		},
		{
			[]string{"**"},
			[]string{"**/*.test1", "**/*.test2"},
			[]string{"x"},
		},
	}

	defer WithTempDir(t)()
	paths := []string{
		"a/a.test1",
		"a/b.test2",
		"b/a.test1",
		"b/b.test2",
		"x",
		"x.test1",
	}
	for _, p := range paths {
		dst := filepath.Join(".", p)
		err := os.MkdirAll(filepath.Dir(dst), 0777)
		if err != nil {
			t.Fatalf("Error creating test dir: %v", err)
		}
		err = os.WriteFile(dst, []byte("test"), 0777)
		if err != nil {
			t.Fatalf("Error writing test file: %v", err)
		}
	}

	for i, tt := range findTests {
		ret, err := List(".", tt.include, tt.exclude)
		if err != nil {
			t.Fatal(err)
		}
		expected := tt.expected
		for i := range ret {
			ret[i] = filepath.ToSlash(ret[i])
		}
		if !reflect.DeepEqual(ret, expected) {
			t.Errorf(
				"%d: %#v, %#v - Expected\n%#v\ngot:\n%#v",
				i, tt.include, tt.exclude, expected, ret,
			)
		}
	}
}

func testList(t *testing.T) {
	var findTests = []struct {
		include  []string
		exclude  []string
		expected []string
	}{
		{
			[]string{"**"},
			[]string{},
			[]string{"a/a.test1", "a/b.test2", "a/sub/c.test2", "b/a.test1", "b/b.test2", "x", "x.test1"},
		},
		{
			[]string{"**/*.test1"},
			[]string{},
			[]string{"a/a.test1", "b/a.test1", "x.test1"},
		},
		{
			[]string{"**"},
			[]string{"*.test1"},
			[]string{"a/a.test1", "a/b.test2", "a/sub/c.test2", "b/a.test1", "b/b.test2", "x"},
		},
		{
			[]string{"**"},
			[]string{"a/**"},
			[]string{"b/a.test1", "b/b.test2", "x", "x.test1"},
		},
		{
			[]string{"**"},
			[]string{"a/**"},
			[]string{"b/a.test1", "b/b.test2", "x", "x.test1"},
		},
		{
			[]string{"**"},
			[]string{"**/*.test1", "**/*.test2"},
			[]string{"x"},
		},
		{
			[]string{"a/relsymlink"},
			[]string{},
			[]string{},
		},
		{
			[]string{"a/relfilesymlink"},
			[]string{},
			[]string{"x"},
		},
		{
			[]string{"a/relsymlink/**"},
			[]string{},
			[]string{"b/a.test1", "b/b.test2"},
		},
		{
			[]string{"a/**", "a/relsymlink/**"},
			[]string{},
			[]string{"a/a.test1", "a/b.test2", "a/sub/c.test2", "b/a.test1", "b/b.test2"},
		},
		{
			[]string{"a/abssymlink/**"},
			[]string{},
			[]string{"b/a.test1", "b/b.test2"},
		},
		{
			[]string{"a/**", "a/abssymlink/**"},
			[]string{},
			[]string{"a/a.test1", "a/b.test2", "a/sub/c.test2", "b/a.test1", "b/b.test2"},
		},
	}

	defer WithTempDir(t)()
	paths := []string{
		"a/a.test1",
		"a/b.test2",
		"a/sub/c.test2",
		"b/a.test1",
		"b/b.test2",
		"x",
		"x.test1",
	}
	for _, p := range paths {
		dst := path.Join(".", p)
		err := os.MkdirAll(path.Dir(dst), 0777)
		if err != nil {
			t.Fatalf("Error creating test dir: %v", err)
		}
		err = os.WriteFile(dst, []byte("test"), 0777)
		if err != nil {
			t.Fatalf("Error writing test file: %v", err)
		}
	}
	if err := os.Symlink("../../b", "./a/relsymlink"); err != nil {
		t.Fatal(err)
		return
	}
	if err := os.Symlink("../../x", "./a/relfilesymlink"); err != nil {
		t.Fatal(err)
		return
	}

	sabs, err := filepath.Abs(filepath.FromSlash("./b"))
	if err != nil {
		t.Fatal(err)
		return
	}
	if err = os.Symlink(sabs, "./a/abssymlink"); err != nil {
		t.Fatal(err)
		return
	}

	for i, tt := range findTests {
		t.Run(
			fmt.Sprintf("%.3d", i),
			func(t *testing.T) {
				ret, err := List(".", tt.include, tt.exclude)
				if err != nil {
					t.Fatal(err)
				}
				expected := tt.expected
				for i := range ret {
					if filepath.IsAbs(ret[i]) {
						wd, err := os.Getwd()
						rel, err := filepath.Rel(wd, filepath.ToSlash(ret[i]))
						if err != nil {
							t.Fatal(err)
							return
						}
						ret[i] = rel
					} else {
						ret[i] = filepath.ToSlash(ret[i])
					}
				}
				if !reflect.DeepEqual(ret, expected) {
					t.Errorf(
						"%d: %#v, %#v - Expected\n%#v\ngot:\n%#v",
						i, tt.include, tt.exclude, expected, ret,
					)
				}
			},
		)
	}
}

func TestList(t *testing.T) {
	testListBasic(t)
	if runtime.GOOS != "windows" {
		testList(t)
	}
}

const timeout = 2 * time.Second

func wait(p string) {
	p = filepath.FromSlash(p)
	for {
		_, err := os.Stat(p)
		if err != nil {
			continue
		} else {
			break
		}
	}
}

func touch(p string) {
	p = filepath.FromSlash(p)
	d := filepath.Dir(p)
	err := os.MkdirAll(d, 0777)
	if err != nil {
		panic(err)
	}

	f, err := os.OpenFile(p, os.O_WRONLY|os.O_CREATE|os.O_APPEND, 0777)
	if err != nil {
		panic(err)
	}
	if _, err := f.Write([]byte("teststring")); err != nil {
		panic(err)
	}
	if err := f.Close(); err != nil {
		panic(err)
	}
	os.ReadFile(p)
}

func events(p string) []string {
	parts := []string{}
	for _, p := range strings.Split(p, "\n") {
		if strings.HasPrefix(p, ":") {
			p = strings.TrimSpace(p)
			if !strings.HasSuffix(p, ":") {
				parts = append(parts, strings.TrimSpace(p))
			}
		}
	}
	return parts
}

func _testWatch(
	t *testing.T,
	modfunc func(),
	includes []string,
	excludes []string,
	expected Mod,
) {
	defer WithTempDir(t)()

	err := os.MkdirAll("a", 0777)
	if err != nil {
		t.Fatal(err)
	}

	err = os.MkdirAll("b", 0777)
	if err != nil {
		t.Fatal(err)
	}

	ch := make(chan *Mod, 1024)
	cwd, err := os.Getwd()
	if err != nil {
		t.Fatal(err)
		return
	}
	watcher, err := Watch(
		cwd,
		includes,
		excludes,
		time.Millisecond*200,
		ch,
	)
	if err != nil {
		t.Fatal(err)
		return
	}
	defer watcher.Stop()
	go func() {
		time.Sleep(2 * time.Second)
		watcher.Stop()
	}()

	// There's some initial startup latency between the fswatch session
	// becoming active and events actually being delivered. If we don't wait
	// a bit here, we sometimes don't receive notifications for the initial
	// event.
	go func() {
		touch("a/initial")
	}()
	for {
		evt, more := <-ch
		if !more {
			t.Errorf("Never saw initial sync event")
			return
		}
		if slices.Equal(evt.Added, []string{"a/initial"}) {
			break
		}
	}

	go modfunc()
	ret := Mod{}
	for {
		evt, more := <-ch
		if more {
			ret = ret.Join(*evt)
			if modsEqual(ret, expected) {
				watcher.Stop()
				return
			}
		} else {
			break
		}
	}
	t.Errorf("Never saw expected result, did see\n%s", ret)
}

func TestWatch(t *testing.T) {
	t.Run(
		"simple",
		func(t *testing.T) {
			_testWatch(
				t,
				func() {
					touch("a/touched")
					touch("a/initial")
				},
				[]string{"**"},
				[]string{},
				Mod{
					Added:   []string{"a/touched"},
					Changed: []string{"a/initial"},
				},
			)
		},
	)
	t.Run(
		"direct",
		func(t *testing.T) {
			_testWatch(
				t,
				func() {
					touch("a/direct")
				},
				[]string{"a/initial", "a/direct"},
				[]string{},
				Mod{
					Added: []string{"a/direct"},
				},
			)
		},
	)
	t.Run(
		"directprexisting",
		func(t *testing.T) {
			_testWatch(
				t,
				func() {
					touch("a/initial")
				},
				[]string{"a/initial"},
				[]string{},
				Mod{
					Changed: []string{"a/initial"},
				},
			)
		},
	)
	t.Run(
		"deepdirect",
		func(t *testing.T) {
			// On Linux, We can't currently pick up changes within directories
			// created after the watch started. See here for more:
			//
			// https://github.com/cortesi/modd/issues/44
			if runtime.GOOS != "linux" {
				_testWatch(
					t,
					func() {
						touch("a/deep/directory/direct")
					},
					[]string{"a/initial", "a/deep/directory/direct"},
					[]string{},
					Mod{
						Added: []string{"a/deep/directory/direct"},
					},
				)
			}
		},
	)
}