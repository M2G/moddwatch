package moddwatch

import (
	"os"
	"path/filepath"
	"reflect"
	"sort"
	"testing"
)

// List must give the same answer whatever way the root is spelled: patterns
// are relative to the root, not to the current directory.
func TestListRootSpelling(t *testing.T) {
	base := t.TempDir()
	root := filepath.Join(base, "proj", "sub")
	for _, f := range []string{"b.go", "c.txt", "src/a.go", "src/gen/g.go"} {
		p := filepath.Join(root, filepath.FromSlash(f))
		if err := os.MkdirAll(filepath.Dir(p), 0o755); err != nil {
			t.Fatal(err)
		}
		if err := os.WriteFile(p, nil, 0o644); err != nil {
			t.Fatal(err)
		}
	}

	tests := []struct {
		name     string
		include  []string
		exclude  []string
		expected []string
	}{
		{"globstar", []string{"**/*.go"}, nil, []string{"b.go", "src/a.go", "src/gen/g.go"}},
		// "**" is only a globstar as a whole path component; here it acts like "*".
		{"literal prefix", []string{"src/**.go"}, nil, []string{"src/a.go"}},
		{"literal prefix star", []string{"src/*.go"}, nil, []string{"src/a.go"}},
		{"top level", []string{"*.go"}, nil, []string{"b.go"}},
		{"literal exclude", []string{"**/*.go"}, []string{"src/gen/**"}, []string{"b.go", "src/a.go"}},
	}

	cwd, err := os.Getwd()
	if err != nil {
		t.Fatal(err)
	}
	t.Cleanup(func() { os.Chdir(cwd) })

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			// reference: chdir into the root and list ".".
			if err := os.Chdir(root); err != nil {
				t.Fatal(err)
			}
			dot, err := List(".", tt.include, tt.exclude)
			if err != nil {
				t.Fatal(err)
			}
			// absolute root, from a different working directory.
			if err := os.Chdir(base); err != nil {
				t.Fatal(err)
			}
			abs, err := List(root, tt.include, tt.exclude)
			if err != nil {
				t.Fatal(err)
			}
			// relative root, from the parent directory.
			rel, err := List(filepath.Join("proj", "sub"), tt.include, tt.exclude)
			if err != nil {
				t.Fatal(err)
			}
			sort.Strings(dot)
			sort.Strings(abs)
			sort.Strings(rel)
			if !reflect.DeepEqual(dot, tt.expected) {
				t.Errorf("List(\".\") = %v, want %v", dot, tt.expected)
			}
			if !reflect.DeepEqual(abs, tt.expected) {
				t.Errorf("List(absolute root) = %v, want %v", abs, tt.expected)
			}
			if !reflect.DeepEqual(rel, tt.expected) {
				t.Errorf("List(relative root) = %v, want %v", rel, tt.expected)
			}
		})
	}
}
