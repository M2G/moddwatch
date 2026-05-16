package main

import (
	"time"

	"github.com/M2G/moddwatch"
)

func main() {
	ch := make(chan *moddwatch.Mod)

	_, err := moddwatch.Watch(
		".",
		[]string{"**"},
		nil,
		time.Second,
		ch,
	)

	if err != nil {
		panic(err)
	}

	select {}
}
