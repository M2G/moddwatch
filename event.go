package moddwatch

// Event represents a filesystem event type.
type Event uint32

const (
	Create Event = 1 << iota
	Write
	Remove
	Rename
)

// EventInfo describes a single filesystem event.
type EventInfo interface {
	Event() Event
	Path() string
	Sys() interface{}
}
