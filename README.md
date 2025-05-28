# CTMP (CoreTech Message Protocol) Protocol

This was a relatively simple challenge, so I've gone with a relatively simple approach. I've mainly focusing on making the code easy to read and handling memory efficiently. When a packet is received from the source client, the packet is handled on a single buffer and the pointer to that buffer passed to the functions that need it. The goal is to reduce memory usage and improve time efficiency by avoiding unnecessary data copying between functions.

The code is quite short, but I've created a separate library, `packet.h`, for handling the validation of the packet data.

To compile:

```bash
gcc proxy.c packet.c -o proxy
```

Make the `proxy` executable and run:

```bash
chmod +x proxy

./proxy
```
