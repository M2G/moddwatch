//go:build !darwin

package moddwatch

type FSEvent struct {
	Path  string
	Flags uint32
}

type FSWatcher struct {
	Events chan FSEvent
}

func NewFSWatcher() (*FSWatcher, error) {
	return &FSWatcher{
		Events: make(chan FSEvent),
	}, nil
}

func (f *FSWatcher) Add(path string) error {
	return nil
}
