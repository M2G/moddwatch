package filter

import (
	"errors"
	"math/rand"
	"os"
	"path"
	"strconv"
	"strings"
	"testing"
	"unicode/utf8"
)

// cMatch runs the C glob engine through the package's own API.
// bad is true when the engine reported a malformed pattern.
func cMatch(pattern, name string) (matched bool, bad bool) {
	ok, err := MatchAny(name, []string{pattern})
	return ok, err != nil
}

// refMatch is the reference: path.Match from the standard library. doublestar
// documents Match() as a drop-in replacement for path.Match(), so on the
// syntax both engines share, the two must agree.
func refMatch(pattern, name string) (matched bool, bad bool) {
	ok, err := path.Match(pattern, name)
	if errors.Is(err, path.ErrBadPattern) {
		return false, true
	}
	return ok, false
}

// inCommonSyntax keeps only the inputs where path.Match and doublestar are
// supposed to behave identically. Everything doublestar adds ({alt}, **, the
// [!x] negation) is out of scope here and covered by dedicated tables.
func inCommonSyntax(pattern, name string) bool {
	if !utf8.ValidString(pattern) || !utf8.ValidString(name) {
		return false
	}
	if strings.ContainsRune(pattern, 0) || strings.ContainsRune(name, 0) {
		return false // C strings stop at NUL
	}
	if !utf8Supported && (hasNonASCII(pattern) || hasNonASCII(name)) {
		return false
	}
	if strings.ContainsAny(pattern, "{}!") || strings.Contains(pattern, "**") {
		return false
	}
	// Class ranges are parsed differently by path.Match and by doublestar
	// (edge cases: "[-a]", "[a-]", "[0--0]", "[0-0-0]", inverted "[a-0]").
	// Plain ranges such as "[a-z]" are covered by stdlibCases; the rest is
	// left to the doublestar comparison in ds_match_doublestar_test.go.
	if strings.Contains(pattern, "-") {
		return false
	}
	// Malformed negated classes ("[^0-x0", "[^0[]", "[^00") are reported
	// differently by path.Match and by doublestar. doublestar is the
	// reference for this port, so they are left to the doublestar test.
	if strings.Contains(pattern, "[^") {
		return false
	}
	// path.Match lets a class match '/', doublestar documents that it does not.
	if strings.Contains(pattern, "[") && strings.Contains(name, "/") {
		return false
	}
	return true
}

func hasNonASCII(s string) bool {
	for i := 0; i < len(s); i++ {
		if s[i] >= utf8.RuneSelf {
			return true
		}
	}
	return false
}

var stdlibCases = []struct{ pattern, name string }{
	{"a*b", "axxb"},
	{"a*b", "a/b"},
	{"a?c", "abc"},
	{"a?c", "a/c"},
	{"[a-z]", "m"},
	{"[a-z]", "5"},
	{"[^a-z]", "5"},
	{"[^a-z]", "m"},
	{"a\\*b", "a*b"},
	{"a\\*b", "axb"},
	{"*.go", "main.go"},
	{"*.go", "dir/main.go"},
	// malformed patterns, including ones that fail before the pattern is used up
	{"a[", "b"},
	{"[", "a"},
	{"[]a]", "a"},
	{"[a-", "a"},
	{"a\\", "a"},
	{"a\\", "b"},
	{"[a-z", "x"},
}

// utf8Supported flips to true once ? and [class] work on runes instead of
// bytes (point 4 of the review). Until then the UTF-8 cases are skipped and
// the fuzzer stays on ASCII inputs.
const utf8Supported = false

var utf8Cases = []struct{ pattern, name string }{
	{"?", "é"},
	{"[é]", "é"},
	{"[^é]", "e"},
	{"[a-é]", "b"},
	{"caf?", "café"},
}

func TestMatchVsStdlib(t *testing.T) {
	for _, c := range stdlibCases {
		gm, gb := cMatch(c.pattern, c.name)
		rm, rb := refMatch(c.pattern, c.name)
		if gm != rm || gb != rb {
			t.Errorf("pattern %q name %q: C=(match %v, bad %v) path.Match=(match %v, bad %v)",
				c.pattern, c.name, gm, gb, rm, rb)
		}
	}
}

func TestMatchUTF8VsStdlib(t *testing.T) {
	if !utf8Supported {
		t.Skip("known gap: ? and [class] are byte-based in ds_match.c")
	}
	for _, c := range utf8Cases {
		gm, gb := cMatch(c.pattern, c.name)
		rm, rb := refMatch(c.pattern, c.name)
		if gm != rm || gb != rb {
			t.Errorf("pattern %q name %q: C=(match %v, bad %v) path.Match=(match %v, bad %v)",
				c.pattern, c.name, gm, gb, rm, rb)
		}
	}
}

// In doublestar, ** is only special as a whole path component: "a**/b" is
// handled like "a*/b" (see the doublestar README).
func TestMatchDoubleStarMidSegment(t *testing.T) {
	cases := []struct {
		pattern, name string
		want          bool
	}{
		{"a**/b", "axx/b", true},
		{"a**/b", "axx/y/b", false},
		{"a**/b", "a/b", true},
		{"foo**/bar", "foo/x/bar", false},
		// real globstars must keep working
		{"**/b", "x/y/b", true},
		{"a/**/b", "a/x/y/b", true},
		{"a/**", "a/x/y", true},
		{"**", "x/y/z", true},
	}
	for _, c := range cases {
		got, bad := cMatch(c.pattern, c.name)
		if bad || got != c.want {
			t.Errorf("pattern %q name %q: got (match %v, bad %v), want %v",
				c.pattern, c.name, got, bad, c.want)
		}
	}
}

// TestMatchStressNoCrash feeds random strings built from the whole pattern
// alphabet ({alt}, **, classes, ranges, escapes, multi-byte) to the C engine
// with no reference to compare against: it only has to return, without
// crashing and without touching memory it does not own. The seed is fixed so
// a failure is reproducible. Run it under AddressSanitizer to catch
// out-of-bounds reads, with more iterations if you like:
//
//	MATCH_STRESS_N=5000000 go test -asan ./filter/ -run Stress
func TestMatchStressNoCrash(t *testing.T) {
	if testing.Short() {
		t.Skip("stress test")
	}
	n := 100000
	if s := os.Getenv("MATCH_STRESS_N"); s != "" {
		v, err := strconv.Atoi(s)
		if err != nil {
			t.Fatalf("MATCH_STRESS_N: %v", err)
		}
		n = v
	}
	rng := rand.New(rand.NewSource(1))
	patAlpha := []rune("ab/*?[]{},\\^!-.é")
	nameAlpha := []rune("ab/.é")
	gen := func(alpha []rune, maxLen int) string {
		var sb strings.Builder
		for i, l := 0, rng.Intn(maxLen+1); i < l; i++ {
			sb.WriteRune(alpha[rng.Intn(len(alpha))])
		}
		return sb.String()
	}
	for i := 0; i < n; i++ {
		cMatch(gen(patAlpha, 14), gen(nameAlpha, 10))
	}
}

func FuzzMatchVsStdlib(f *testing.F) {
	for _, c := range stdlibCases {
		f.Add(c.pattern, c.name)
	}
	for _, c := range utf8Cases {
		f.Add(c.pattern, c.name)
	}
	f.Fuzz(func(t *testing.T, pattern, name string) {
		if !inCommonSyntax(pattern, name) {
			t.Skip()
		}
		gm, gb := cMatch(pattern, name)
		rm, rb := refMatch(pattern, name)
		if gm != rm || gb != rb {
			t.Errorf("pattern %q name %q: C=(match %v, bad %v) path.Match=(match %v, bad %v)",
				pattern, name, gm, gb, rm, rb)
		}
	})
}
