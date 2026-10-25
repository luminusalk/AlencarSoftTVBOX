# HTTPS trust store

`cacert.pem` is the Mozilla CA certificate store converted to PEM by curl,
downloaded from https://curl.se/ca/cacert.pem on 2026-10-25. Its SHA-256 is
`796f1389bfea13c0ed78f663b765baaff54a508e5483786bd6ece73c5e3e3cbe`.
The unmodified certificate data is licensed under Mozilla Public License 2.0.

For the Azamerica S1009+, install this file at `/mnt/plugin/iptv/cacert.pem`
beside the channel list. The native HTTPS client rejects connections if it
cannot load a CA bundle or verify the server hostname and certificate.
The bundle needs periodic updates as trust roots change.
