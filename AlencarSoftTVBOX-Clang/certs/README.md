# HTTPS trust store

`cacert.pem` is the Mozilla CA certificate store converted to PEM by curl,
downloaded from https://curl.se/ca/cacert.pem on 2026-10-25. Its SHA-256 is
`796f1389bfea13c0ed78f663b765baaff54a508e5483786bd6ece73c5e3e3cbe`.
The unmodified certificate data is licensed under Mozilla Public License 2.0.

The build embeds this file into the C executable. No separate CA file needs
to be installed on the receiver. `HLS_CA_BUNDLE` can override it for tests.
The native HTTPS client rejects connections if it cannot parse the bundle or
verify the server hostname and certificate.
The bundle needs periodic updates as trust roots change.
