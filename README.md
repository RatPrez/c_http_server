# HTTP Server in C

A minimal HTTP/1.1 server built from scratch in C using raw TCP sockets. No libraries, no frameworks — just the standard POSIX socket API.

## Features

- Raw TCP socket setup via `getaddrinfo`, `bind`, `listen`, `accept`
- HTTP/1.1 request parsing (method, path, version, headers)
- Multithreaded connection handling with pthreads (one thread per connection)
- Static file serving with correct MIME types
- URL routing with redirect support
- `Sec-Fetch-Dest` header detection to distinguish direct user navigation from page-initiated asset requests
- Chunked file streaming — files are sent in 64KB chunks, so large files don't load into memory
- `Accept-Ranges: bytes` support for video buffering

## Supported MIME Types

Images: `png`, `jpg`, `gif`, `svg`  
Video: `mp4`, `webm`, `ogg`  
Web: `html`, `css`, `js`

## Project Structure

```
pages/          HTML and CSS pages served by the server
pages/assets/   Static assets (images, video, etc.)
server.c        Server source
run.sh          Build and run script
```

## Running

```bash
./run.sh
```

Then open `http://localhost:8080/home` in your browser.

Requires `gcc` and `pthreads`. Linux and macOS only — Windows uses a different socket API (Winsock) and is not supported.

## Adding Pages

Drop an HTML file in `pages/` — a file at `pages/about.html` is automatically served at `/about`. Any unknown route redirects to `/help`.

Assets referenced in HTML (CSS, images, video) should go in `pages/assets/` and be referenced as `/assets/filename`.

## Known Limitations

- HTTP Range requests are not fully implemented — video seeking works within the buffered portion but jumping to unbuffered sections of large files will restart from the beginning
- HTTP only, no TLS
