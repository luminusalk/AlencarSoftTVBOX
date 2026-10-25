import http.server
import socket
import subprocess
import sys
import threading
import urllib.request
import xml.etree.ElementTree as ET


SERVICE = "urn:schemas-upnp-org:service:AVTransport:1"
SEGMENT = (b"\x47" + bytes(187)) * 256
events = []


class Handler(http.server.BaseHTTPRequestHandler):
    def log_message(self, *args):
        pass

    def reply(self, code, body=b"", content_type="text/xml"):
        self.send_response(code)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        if self.path == "/description.xml":
            body = ("<root><device><serviceList><service>"
                    f"<serviceType>{SERVICE}</serviceType>"
                    "<controlURL>/AVTransport_control</controlURL>"
                    "</service></serviceList></device></root>").encode()
            return self.reply(200, body)
        if self.path == "/live.m3u8":
            playlist = ("#EXTM3U\n#EXT-X-MEDIA-SEQUENCE:42\n"
                        "#EXTINF:2.0,\nseg1.ts\n#EXTINF:2.0,\nseg2.ts\n").encode()
            return self.reply(200, playlist, "application/vnd.apple.mpegurl")
        if self.path in ("/seg1.ts", "/seg2.ts"):
            return self.reply(200, SEGMENT, "video/mp2t")
        self.reply(404)

    def do_POST(self):
        if self.path != "/AVTransport_control":
            return self.reply(404)
        body = self.rfile.read(int(self.headers["Content-Length"]))
        action = self.headers["SOAPAction"].strip('"').split("#")[-1]
        if action == "SetAVTransportURI":
            document = ET.fromstring(body)
            self.server.stream_uri = document.findtext(".//CurrentURI")
            assert self.server.stream_uri.startswith("http://127.0.0.1:")
            assert document.findtext(".//CurrentURIMetaData")
        elif action == "Play":
            with urllib.request.urlopen(self.server.stream_uri, timeout=8) as stream:
                assert stream.headers["Content-Type"] == "video/mp2t"
                assert stream.read(len(SEGMENT)) == SEGMENT
        elif action != "Stop":
            return self.reply(500)
        events.append(action)
        self.reply(200)


with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as route:
    route.connect(("1.1.1.1", 53))
    own_ip = route.getsockname()[0]
server = http.server.ThreadingHTTPServer(("0.0.0.0", 0), Handler)
thread = threading.Thread(target=server.serve_forever, daemon=True)
thread.start()
try:
    url = f"http://{own_ip}:{server.server_port}/live.m3u8"
    result = subprocess.run([sys.argv[1], url], capture_output=True,
                            text=True, timeout=30)
    print(result.stdout, result.stderr)
    assert result.returncode == 0, result.returncode
    assert events == ["SetAVTransportURI", "Play", "Stop"], events
finally:
    server.shutdown()
    server.server_close()
    thread.join()
