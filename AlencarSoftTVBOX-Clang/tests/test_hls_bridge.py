import http.server
import socketserver
import subprocess
import sys
import threading
import unittest


SEGMENT = bytes((i * 13 + 7) % 256 for i in range(1_600_000))


class Handler(http.server.BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.0"

    def log_message(self, *args):
        pass

    def do_GET(self):
        if self.path == "/live.m3u8":
            body = b"#EXTM3U\n#EXT-X-MEDIA-SEQUENCE:42\n#EXTINF:2,\nsegment.ts\n"
        elif self.path == "/segment.ts":
            body = SEGMENT
        elif self.path == "/broken.m3u8":
            self.send_response(404)
            self.send_header("Content-Length", "3")
            self.end_headers()
            self.wfile.write(b"bad")
            return
        elif self.path == "/partial.m3u8":
            self.send_response(200)
            self.send_header("Content-Length", "200")
            self.end_headers()
            self.wfile.write(b"#EXTM3U\n")
            return
        else:
            self.send_error(404)
            return
        self.send_response(200)
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)


class BridgeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.server = socketserver.TCPServer(("127.0.0.1", 0), Handler)
        cls.thread = threading.Thread(target=cls.server.serve_forever, daemon=True)
        cls.thread.start()
        cls.base = f"http://127.0.0.1:{cls.server.server_address[1]}"

    @classmethod
    def tearDownClass(cls):
        cls.server.shutdown()
        cls.server.server_close()
        cls.thread.join()

    def run_bridge(self, mode, path, expected=0, timeout=8):
        return subprocess.run(
            [sys.argv[1], mode, self.base + path, str(expected)],
            capture_output=True, timeout=timeout,
        )

    def test_segment_larger_than_old_limit_is_exact(self):
        result = self.run_bridge("stream", "/live.m3u8", len(SEGMENT), timeout=12)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout, SEGMENT)

    def test_stop_without_fifo_reader_is_bounded(self):
        result = self.run_bridge("stop", "/live.m3u8", timeout=5)
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_http_error_does_not_enter_fifo(self):
        result = self.run_bridge("empty", "/broken.m3u8")
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_incomplete_response_does_not_enter_fifo(self):
        result = self.run_bridge("empty", "/partial.m3u8")
        self.assertEqual(result.returncode, 0, result.stderr)


if __name__ == "__main__":
    unittest.main(argv=[sys.argv[0]])
