package main

import (
	"bufio"
	"encoding/json"
	"fmt"
	"os"
	"os/exec"
	"os/signal"
	"syscall"
)

type output struct {
	Text string `json:"text"`
}

func (o *output) write() {
	val, _ := json.Marshal(o)
	bf := bufio.NewWriter(os.Stdout)
	val = append(val, '\n')
	bf.Write(val)
	bf.Flush()
}

func main() {
	// setup channel to receive os signals
	sigChan := make(chan os.Signal, 1)
	signal.Notify(sigChan, syscall.SIGINT, syscall.SIGTERM)

	// setup command
	cmd := exec.Command(
		"playerctl", "metadata", "--format", "{{title}}", "--follow",
	)

	// get output pipe
	stdout, _ := cmd.StdoutPipe()

	// start the command
	_ = cmd.Start()

	// read cmd output
	scanner := bufio.NewScanner(stdout)

	// handle output
	go func() {
		for scanner.Scan() {
			if scanner.Text() != "" {
				data := output{
					Text: " " + scanner.Text(),
				}
				data.write()
				continue
			}

			var data output
			data.write()
		}
	}()

	// wait for os signal
	sig := <-sigChan
	fmt.Println("Received signal: ", sig)
}
