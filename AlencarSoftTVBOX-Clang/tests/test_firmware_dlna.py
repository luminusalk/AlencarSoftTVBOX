import http.server
from pathlib import Path
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
            assert title == "Canal & teste", title
            assert video_class == "object.item.videoItem", video_class
            assert resource.attrib["protocolInfo"] == "http-get:*:video/mpeg:*"
            assert resource.text == uri
            self.server.stream_uri = uri
        elif action == "Play":
            with urllib.request.urlopen(self.server.stream_uri, timeout=5) as response:
                assert response.headers["Content-Type"] == "video/mpeg"
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
    finally:
        renderer.shutdown()
        renderer.server_close()
        thread.join()
