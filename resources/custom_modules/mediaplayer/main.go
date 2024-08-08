package main

import (
	"bufio"
	"encoding/json"
	"os"
	"os/exec"
	"os/signal"
	"strings"
	"syscall"
)

// output struct defines json syntax for waybar
type output struct {
	Text string `json:"text"`
}

// write method marshals string into json
func (o *output) write() {
	val, _ := json.Marshal(o)
	bf := bufio.NewWriter(os.Stdout)
	val = append(val, '\n')
	bf.Write(val)
	bf.Flush()
}

// getPlaybackInfo gets the media playing via playerctl
func getPlaybackInfo(infoChan chan string, scanner bufio.Scanner) {
	var info string

	for scanner.Scan() {
		output_bytes, _ := exec.Command(
			"playerctl", "metadata", "--format", "{{title}}",
		).Output()

		title := strings.TrimSpace(string(output_bytes))
		status := strings.TrimSpace(scanner.Text())

		switch {
		case len(title) == 0:
			info = " Nothing Playing"
		case status == "Playing":
			info = " "
		case status == "Paused":
			info = "󰏦 "
		}

		info += title
		infoChan <- info
	}
}

// writePlaybackInfo writes playback info to stdout
func writePlaybackInfo(infoChan chan string) {
	var data output

	for {
		info := <-infoChan
		data = output{
			Text: info,
		}
		data.write()
	}
}

func main() {
	// create channel to track os signal
	sigChan := make(chan os.Signal, 1)
	signal.Notify(sigChan, syscall.SIGINT)

	infoChan := make(chan string) // holds playback info

	cmd := exec.Command(
		"playerctl", "status", "--follow",
	)

	stdout, _ := cmd.StdoutPipe()
	_ = cmd.Start()
	scanner := bufio.NewScanner(stdout)

	go getPlaybackInfo(infoChan, *scanner)
	go writePlaybackInfo(infoChan)

	<-sigChan
}
