package moddwatch

import (
	"os"
	"path/filepath"
	"reflect"
	"sort"
	"testing"
)

func TestListSkipDir(t *testing.T) {
	root := t.TempDir()
	for _, f := range []string{
		"a.go",
		"node_modules/x/y.go",
		".git/hooks/h.go",
		"old.bak/keep.go",
		"src/gen/g.go",
		"src/real.go",
	} {
		p := filepath.Join(root, filepath.FromSlash(f))
		if err := os.MkdirAll(filepath.Dir(p), 0o755); err != nil {
			t.Fatal(err)
		}
		if err := os.WriteFile(p, nil, 0o644); err != nil {
			t.Fatal(err)
		}
	}
	exclude := []string{"**/node_modules/**", "**/.git/**", "**.bak", "src/gen/**"}
	got, err := List(root, []string{"**/*.go"}, exclude)
	if err != nil {
		t.Fatal(err)
	}
	sort.Strings(got)
	// "**.bak" names the directory but not what is inside it: its files stay.
	want := []string{"a.go", "old.bak/keep.go", "src/real.go"}
	if !reflect.DeepEqual(got, want) {
		t.Errorf("List = %v, want %v", got, want)
	}
}
