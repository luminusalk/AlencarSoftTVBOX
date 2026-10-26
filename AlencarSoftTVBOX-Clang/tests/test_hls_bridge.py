import http.server
import os
import socketserver
import ssl
import subprocess
import sys
import tempfile
import threading
import unittest


SEGMENT = bytes((i * 13 + 7) % 256 for i in range(1_600_000))


class Handler(http.server.BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.0"

    def log_message(self, *args):
        pass

    def do_GET(self):
        if self.path == "/valid.ts":
            body = (b"\x47" + bytes(187)) * 32
        elif self.path == "/valid.m3u8":
            body = b"#EXTM3U\n#EXT-X-MEDIA-SEQUENCE:0\n#EXTINF:2,\nvalid.ts\n"
        elif self.path == "/html.ts":
            body = b"<html>Service unavailable</html>"
        elif self.path == "/redirect":
            self.send_response(302)
            self.send_header("Location", "/valid.ts")
            self.send_header("Content-Length", "0")
            self.end_headers()
            return
        elif self.path == "/live.m3u8":
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
    def test_probe_accepts_media_without_starting_bridge(self):
        for path in ("/valid.ts", "/valid.m3u8", "/redirect"):
            result = self.run_bridge("probe", path)
            self.assertEqual(result.returncode, 0, (path, result.stderr))

    def test_probe_rejects_errors_and_html(self):
        for path in ("/missing", "/html.ts", "/partial.m3u8"):
            result = self.run_bridge("probe", path)
            self.assertEqual(result.returncode, 10, (path, result.stderr))

    @classmethod
    def setUpClass(cls):
        cls.server = socketserver.TCPServer(("127.0.0.1", 0), Handler)
        cls.thread = threading.Thread(target=cls.server.serve_forever, daemon=True)
        cls.thread.start()
        cls.base = f"http://127.0.0.1:{cls.server.server_address[1]}"
        cls.tls_server = None
        if os.environ.get("HLS_TEST_TLS") == "1":
            cls.cert_dir = tempfile.TemporaryDirectory()
            cls.cert = os.path.join(cls.cert_dir.name, "localhost.crt")
            key = os.path.join(cls.cert_dir.name, "localhost.key")
            subprocess.run([
                "openssl", "req", "-x509", "-newkey", "rsa:2048", "-nodes",
                "-days", "1", "-keyout", key, "-out", cls.cert,
                "-subj", "/CN=localhost", "-addext", "subjectAltName=DNS:localhost",
                "-addext", "basicConstraints=critical,CA:TRUE",
            ], check=True, capture_output=True)
            ssl_context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
            ssl_context.load_cert_chain(cls.cert, key)
            cls.tls_server = socketserver.TCPServer(("127.0.0.1", 0), Handler)
            cls.tls_server.socket = ssl_context.wrap_socket(cls.tls_server.socket,
                                                             server_side=True)
            cls.tls_thread = threading.Thread(target=cls.tls_server.serve_forever,
                                              daemon=True)
            cls.tls_thread.start()
            cls.tls_base = f"https://localhost:{cls.tls_server.server_address[1]}"

    @classmethod
    def tearDownClass(cls):
        cls.server.shutdown()
        cls.server.server_close()
        cls.thread.join()
        if cls.tls_server:
            cls.tls_server.shutdown()
            cls.tls_server.server_close()
            cls.tls_thread.join()
            cls.cert_dir.cleanup()

    def run_bridge(self, mode, path, expected=0, timeout=8, base=None, env=None):
        return subprocess.run(
            [sys.argv[1], mode, (base or self.base) + path, str(expected)],
            capture_output=True, timeout=timeout, env=env,
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

    def test_https_certificate_and_hostname_verified(self):
        if not self.tls_server:
            self.skipTest("TLS build not requested")
        env = dict(os.environ, HLS_CA_BUNDLE=self.cert)
        result = self.run_bridge("stream", "/live.m3u8", len(SEGMENT),
                                 timeout=12, base=self.tls_base, env=env)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout, SEGMENT)

        bad_host = self.tls_base.replace("localhost", "127.0.0.1")
        result = self.run_bridge("empty", "/live.m3u8", base=bad_host, env=env)
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_embedded_ca_bundle_loads_without_receiver_file(self):
        if not self.tls_server:
            self.skipTest("TLS build not requested")
        env = dict(os.environ, HLS_TLS_TRACE="1")
        env.pop("HLS_CA_BUNDLE", None)
        result = self.run_bridge("empty", "/live.m3u8", base=self.tls_base,
                                 env=env, timeout=12)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn(b"[HLS TLS] ca-parsed rc=0", result.stderr)


if __name__ == "__main__":
    unittest.main(argv=[sys.argv[0]])
