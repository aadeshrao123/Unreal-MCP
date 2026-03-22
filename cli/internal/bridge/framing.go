package bridge

import (
	"encoding/binary"
	"fmt"
	"io"
)

const (
	maxResponseSize = 100 * 1024 * 1024 // 100 MB
	recvBufferSize  = 65536             // 64 KB chunks
)

// encodeFrame prepends a 4-byte big-endian length header to payload.
func encodeFrame(payload []byte) []byte {
	frame := make([]byte, 4+len(payload))
	binary.BigEndian.PutUint32(frame[:4], uint32(len(payload)))
	copy(frame[4:], payload)
	return frame
}

// readFrame reads a length-prefixed frame: 4-byte big-endian header + payload.
func readFrame(r io.Reader) ([]byte, error) {
	header := make([]byte, 4)
	if _, err := io.ReadFull(r, header); err != nil {
		return nil, fmt.Errorf("failed to read length header: %w", err)
	}

	payloadLen := binary.BigEndian.Uint32(header)
	if payloadLen == 0 {
		return nil, fmt.Errorf("server sent empty response (length=0)")
	}
	if payloadLen > maxResponseSize {
		return nil, fmt.Errorf("response too large: %d bytes (>100MB)", payloadLen)
	}

	payload := make([]byte, payloadLen)
	if _, err := io.ReadFull(r, payload); err != nil {
		return nil, fmt.Errorf("failed to read payload (%d bytes): %w", payloadLen, err)
	}

	return payload, nil
}
