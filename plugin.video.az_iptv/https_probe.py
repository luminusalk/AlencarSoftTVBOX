# -*- coding: utf-8 -*-
"""One-time receiver probe for the embedded Python HTTPS transport."""

import os


def run(root):
    report = os.path.join(root, 'https_probe.txt')
    if os.path.exists(report):
        return
    lines = []
    try:
        import ssl
        lines.append('ssl_import=ok')
        lines.append('ssl_version=%s' % getattr(ssl, 'OPENSSL_VERSION', 'unknown'))
    except Exception as error:
        lines.append('ssl_import=%s: %s' % (type(error).__name__, error))
    try:
        import urllib2
        response = urllib2.urlopen('https://raw.githubusercontent.com/', timeout=5)
        lines.append('github_status=%s' % response.getcode())
        response.close()
    except Exception as error:
        lines.append('github_status=%s: %s' % (type(error).__name__, error))
    try:
        with open(report, 'w') as output:
            output.write('\n'.join(lines) + '\n')
    except Exception:
        pass
