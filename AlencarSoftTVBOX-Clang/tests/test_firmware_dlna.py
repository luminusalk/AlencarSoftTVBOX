import http.server
from pathlib import Path
import socket
import subprocess
import sys
import tempfile
import threading
import urllib.request
import xml.etree.ElementTree as ET


AV_SERVICE = "urn:schemas-upnp-org:service:AVTransport:1"
sample = bytes([0x47]) + bytes(187)
sample *= 32
events = []


class Renderer(http.server.BaseHTTPRequestHandler):
    def log_message(self, *args):
        pass

    def respond(self, code, body=b""):
        self.send_response(code)
        self.send_header("Content-Type", "text/xml")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        if self.path == "/sample.ts":
            self.send_response(200)
            self.send_header("Content-Type", "video/mpeg")
            self.send_header("Content-Length", str(len(sample)))
            self.end_headers()
            self.wfile.write(sample)
            return
        if self.path != "/description.xml":
            return self.respond(404)
        body = ("<root><device><serviceList><service>"
                f"<serviceType>{AV_SERVICE}</serviceType>"
                "<controlURL>/_urn:schemas-upnp-org:service:AVTransport_control</controlURL>"
                "</service></serviceList></device></root>").encode()
        self.respond(200, body)

    def do_POST(self):
        if self.path != "/_urn:schemas-upnp-org:service:AVTransport_control":
            return self.respond(404)
        payload = self.rfile.read(int(self.headers["Content-Length"]))
        action = self.headers["SOAPAction"].strip('"').split("#")[-1]
        document = ET.fromstring(payload)
        if action == "SetAVTransportURI":
            uri = document.findtext(".//CurrentURI")
            metadata = document.findtext(".//CurrentURIMetaData")
            didl = ET.fromstring(metadata)
            title = didl.findtext(".//{http://purl.org/dc/elements/1.1/}title")
            video_class = didl.findtext(
                ".//{urn:schemas-upnp-org:metadata-1-0/upnp/}class")
            resource = didl.find(".//{urn:schemas-upnp-org:metadata-1-0/DIDL-Lite/}res")
            assert title.startswith("Canal & teste"), title
            assert video_class == "object.item.videoItem", video_class
            assert resource.attrib["protocolInfo"] == "http-get:*:video/mpeg:*"
            assert resource.text == uri
            self.server.stream_uri = uri
        elif action == "Play":
            if "live.ts" in self.server.stream_uri:
                req_head = urllib.request.Request(self.server.stream_uri, method="HEAD")
                with urllib.request.urlopen(req_head, timeout=5) as head_resp:
                    assert head_resp.headers["Content-Type"] in ("video/mpeg", "video/mp4")
                    assert head_resp.read() == b""
                with urllib.request.urlopen(self.server.stream_uri, timeout=5) as partial:
                    _ = partial.read(64)
            with urllib.request.urlopen(self.server.stream_uri, timeout=5) as response:
                assert response.headers["Content-Type"] in ("video/mpeg", "video/mp4")
                assert response.read() == sample
        elif action != "Stop":
            return self.respond(500)
        events.append(action)
        self.respond(200)


with tempfile.TemporaryDirectory() as directory:
    path = Path(directory) / "sample.ts"
    path.write_bytes(sample)
    renderer = http.server.ThreadingHTTPServer(("0.0.0.0", 0), Renderer)
    thread = threading.Thread(target=renderer.serve_forever, daemon=True)
    thread.start()
    try:
        result = subprocess.run([sys.argv[1], str(path)], timeout=35,
                                capture_output=True, text=True)
        print(result.stdout, result.stderr)
        assert result.returncode == 0, result.returncode
        assert events == ["SetAVTransportURI", "Play", "Stop"], events
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as route:
            route.connect(("1.1.1.1", 53))
            local_ip = route.getsockname()[0]
        direct_url = f"http://{local_ip}:{renderer.server_port}/sample.ts"
        result = subprocess.run([sys.argv[1], direct_url], timeout=35,
                                capture_output=True, text=True)
        print(result.stdout, result.stderr)
        assert result.returncode == 0, result.returncode
        assert renderer.stream_uri == direct_url
        assert events == ["SetAVTransportURI", "Play", "Stop"] * 2, events

        result = subprocess.run([sys.argv[1], str(path), "cycle"], timeout=35,
                                capture_output=True, text=True)
        print(result.stdout, result.stderr)
        assert result.returncode == 0, result.returncode
        assert events == ["SetAVTransportURI", "Play", "Stop"] * 4, events
    finally:
        renderer.shutdown()
        renderer.server_close()
        thread.join()
