//go:build doublestar

package filter

import (
	"testing"

	"github.com/bmatcuk/doublestar/v4"
)

var benchPath = "internal/server/handlers/user_handler_test.go"

var benchPatterns = []struct{ name, pat string }{
	{"literal-miss", "README.md"},
	{"star-ext", "*.md"},
	{"globstar-ext", "**/*.go"},
	{"globstar-suffix", "**/*_test.go"},
	{"prefix-globstar", "internal/**"},
	{"alt", "{cmd,internal}/**/*.go"},
}

var sink bool

func BenchmarkGoDoublestar(b *testing.B) {
	for _, c := range benchPatterns {
		b.Run(c.name, func(b *testing.B) {
			for i := 0; i < b.N; i++ {
				sink, _ = doublestar.Match(c.pat, benchPath)
			}
		})
	}
}

func BenchmarkCviaCgo(b *testing.B) {
	for _, c := range benchPatterns {
		pats := []string{c.pat}
		b.Run(c.name, func(b *testing.B) {
			for i := 0; i < b.N; i++ {
				sink, _ = MatchAny(benchPath, pats)
			}
		})
	}
}

// Fixed cost of one MatchAny call: CString malloc/free + cgo transition, matching work ~0.
func BenchmarkCgoFloor(b *testing.B) {
	pats := []string{"a"}
	for i := 0; i < b.N; i++ {
		sink, _ = MatchAny("a", pats)
	}
}

// What the watcher really does per event: File() = excludes first (stop if
// one matches), then includes. The Go version mirrors that exact logic.
func fileGo(path string, inc, exc []string) bool {
	for _, p := range exc {
		if ok, _ := doublestar.Match(p, path); ok {
			return false
		}
	}
	for _, p := range inc {
		if ok, _ := doublestar.Match(p, path); ok {
			return true
		}
	}
	return false
}

var (
	typInc = []string{"**/*.go", "**/*.c", "**/*.h"}
	typExc = []string{"**/.git/**", "**/node_modules/**", "**/*_test.go"}
	// excluded by the 3rd exclude pattern / included by the 1st include pattern
	pathExcluded = "internal/server/handlers/user_handler_test.go"
	pathIncluded = "internal/server/handlers/user_handler.go"
	// matches no include and no exclude: every pattern is evaluated
	pathIgnored = "internal/server/handlers/notes.txt"
)

func BenchmarkFile(b *testing.B) {
	for _, c := range []struct{ name, path string }{
		{"excluded", pathExcluded}, {"included", pathIncluded}, {"ignored", pathIgnored},
	} {
		b.Run("Go/"+c.name, func(b *testing.B) {
			for i := 0; i < b.N; i++ {
				sink = fileGo(c.path, typInc, typExc)
			}
		})
		b.Run("C/"+c.name, func(b *testing.B) {
			for i := 0; i < b.N; i++ {
				r, _ := File(c.path, typInc, typExc)
				sink = r != ""
			}
		})
	}
}
