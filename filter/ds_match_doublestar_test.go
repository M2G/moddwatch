//go:build doublestar

// Differential test of the C glob engine against doublestar itself, the
// library it replaces. doublestar is only a test dependency and only built
// with the "doublestar" tag, so the module's normal build stays free of it:
//
//	go get github.com/bmatcuk/doublestar/v4
//	go test -tags doublestar ./filter/ -run VsDoublestar
//	go test -tags doublestar ./filter/ -run '^$' -fuzz FuzzMatchVsDoublestar -fuzztime=2m
//
// Unlike path.Match, doublestar knows {alt}, ** and the exact class-range
// rules, so this is the reference for the whole syntax.
package filter

import (
	"strings"
	"testing"
	"unicode/utf8"

	"github.com/bmatcuk/doublestar/v4"
)

func dsRefMatch(pattern, name string) (matched bool, bad bool) {
	ok, err := doublestar.Match(pattern, name)
	if err != nil {
		return false, true
	}
	return ok, false
}

func dsComparable(pattern, name string) bool {
	if !utf8.ValidString(pattern) || !utf8.ValidString(name) {
		return false
	}
	if strings.ContainsRune(pattern, 0) || strings.ContainsRune(name, 0) {
		return false
	}
	if !utf8Supported && (hasNonASCII(pattern) || hasNonASCII(name)) {
		return false
	}
	return true
}

var doublestarCases = []struct{ pattern, name string }{
	{"{a,b}/**/*.{go,c}", "x/a/y/z.go"},
	{"{a,b}/**/*.{go,c}", "a/y/z.c"},
	{"a/{b,{c,d}}/e", "a/c/e"},
	{"a/{b,{c,d}}/e", "a/x/e"},
	{"{*,a/**}", "a/b"},
	{"{,a}", ""},
	{"{", "a"},
	{"a}", "a"},
	{"**", "x/y/z"},
	{"**/", "a/"},
	{"/**", "/a/b"},
	{"a/**", "a"},
	{"a/**/", "a/b/"},
	{"a**/b", "axx/y/b"},
	{"**/b", "x/y/b"},
	{"a/**/b", "a/x/y/b"},
	{"[a-0]", "a"},
	{"[-a]", "-"},
	{"[a-]", "-"},
	{"[0--0]", "0"},
	{"[0-0-0]", "0"},
	{"[!a-c]", "d"},
	{"[^a-c]", "d"},
	{"[^0-x0", "0"},
	{"[^0[]", "0"},
	// '}' and ',' inside a character class are not alternation syntax
	// (doublestar >= 4.x skips classes when scanning alternatives).
	{"{[}0]", "0"},
	{"{[,]a,b}", ",a"},
	{"{[,]a,b}", "b"},
	{"{a,[}]}", "}"},
	// re-entry after an alternative: "**" is only a globstar at a segment start
	{"a{}**/0", "a0"},
	{"{a,b}**/c", "ac"},
	{"{a,b}/**/c", "a/x/c"},
	{"x/{a,}**/c", "x/ay/c"},
}

func TestMatchVsDoublestar(t *testing.T) {
	cases := append([]struct{ pattern, name string }{}, stdlibCases...)
	cases = append(cases, doublestarCases...)
	if utf8Supported {
		cases = append(cases, utf8Cases...)
	}
	for _, c := range cases {
		gm, gb := cMatch(c.pattern, c.name)
		rm, rb := dsRefMatch(c.pattern, c.name)
		if gm != rm || gb != rb {
			t.Errorf("pattern %q name %q: C=(match %v, bad %v) doublestar=(match %v, bad %v)",
				c.pattern, c.name, gm, gb, rm, rb)
		}
	}
}

func FuzzMatchVsDoublestar(f *testing.F) {
	for _, c := range stdlibCases {
		f.Add(c.pattern, c.name)
	}
	for _, c := range doublestarCases {
		f.Add(c.pattern, c.name)
	}
	f.Fuzz(func(t *testing.T, pattern, name string) {
		if !dsComparable(pattern, name) {
			t.Skip()
		}
		gm, gb := cMatch(pattern, name)
		rm, rb := dsRefMatch(pattern, name)
		if gm != rm || gb != rb {
			t.Errorf("pattern %q name %q: C=(match %v, bad %v) doublestar=(match %v, bad %v)",
				pattern, name, gm, gb, rm, rb)
		}
	})
}
