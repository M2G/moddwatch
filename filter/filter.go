package filter

/*
#include "ds_match.h"
#include <stdlib.h>
*/
import "C"

import (
	"fmt"
	"path/filepath"
	"strings"
	"unsafe"
)
// dsMatch calls the C glob engine (ds_match.c), equivalent to
// doublestar.Match(pattern, path) replaces the
// ‘github.com/bmatcuk/doublestar/v4’ import from the original filter.go.
// MatchAny checks whether the given path matches any of the specified patterns.
func MatchAny(path string, patterns []string) (bool, error) {
	if len(patterns) == 0 {
		return false, nil
	}

	cPath := C.CString(filepath.ToSlash(path))
	defer C.free(unsafe.Pointer(cPath))

	for _, pattern := range patterns {
		cPattern := C.CString(pattern)
		result := C.ds_match(cPattern, cPath)
		C.free(unsafe.Pointer(cPattern))

		switch result {
		case C.DS_MATCH:
			return true, nil
		case C.DS_BAD_PATTERN:
			return false, fmt.Errorf("invalid pattern: %s", pattern)
		}
	}
	return false, nil
}

// File determines if a path matches a set of include and exclude patterns. At
// least one include pattern and no exclude patterns must match.
func File(
	path string,
	includePatterns []string,
	excludePatterns []string,
) (string, error) {
	if excluded, err := MatchAny(path, excludePatterns); err != nil {
		return "", err
	} else if excluded {
		return "", nil
	}
	if included, err := MatchAny(path, includePatterns); err != nil {
		return "", err
	} else if included {
		return path, nil
	}
	return "", nil
}

// Files filters an array of files using filter.File.
func Files(
	files []string,
	includePatterns []string,
	excludePatterns []string,
) ([]string, error) {
	ret := []string{}
	for _, file := range files {
		path, err := File(file, includePatterns, excludePatterns)
		if err != nil || path == "" {
			continue
		}
		ret = append(ret, path)
	}
	return ret, nil
}

// SplitPattern splits a pattern into a root directory and a trailing pattern
// specifier.
func SplitPattern(pattern string) (string, string) {
	base := pattern
	trail := ""

	split := strings.IndexAny(pattern, "*{}?[]")
	if split >= 0 {
		base = pattern[:split]
		trail = pattern[split:]
	}
	return base, trail
}
