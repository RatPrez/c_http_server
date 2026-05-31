#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <signal.h>

const int DEBUG_ENABLED = 1;

const char *FILE_NOT_FOUND = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n";

struct http_request
{
    char method[16];
    char path[256];
    char version[16];
};

typedef enum {
    PNG, JPEG, GIF, SVG, HTML, CSS, JS, MP4, WEBM, OGG, UNKNOWN
} content_type_t;

int is_valid_web_page(const char *webpath)
{
    char path[512];
    snprintf(path, sizeof(path), "pages%s.html", webpath);
    FILE *f = fopen(path, "rb");
    if (f) { fclose(f); return 1; }
    return 0;
}

content_type_t get_file_type(const char *path)
{
    char *ext = strrchr(path, '.');
    if (ext == NULL)                      return UNKNOWN;

    if      (strcmp(ext, ".png")  == 0)   return PNG;
    else if (strcmp(ext, ".jpg")  == 0)   return JPEG;
    else if (strcmp(ext, ".jpeg") == 0)   return JPEG;
    else if (strcmp(ext, ".gif")  == 0)   return GIF;
    else if (strcmp(ext, ".svg")  == 0)   return SVG;
    else if (strcmp(ext, ".html") == 0)   return HTML;
    else if (strcmp(ext, ".css")  == 0)   return CSS;
    else if (strcmp(ext, ".js")   == 0)   return JS;
    else if (strcmp(ext, ".mp4")  == 0)   return MP4;
    else if (strcmp(ext, ".webm") == 0)   return WEBM;
    else if (strcmp(ext, ".ogg")  == 0)   return OGG;
    else                                  return UNKNOWN;
}

const char *get_content_type(content_type_t type)
{
    switch (type) {
        case PNG:  return "image/png";
        case JPEG: return "image/jpeg";
        case GIF:  return "image/gif";
        case SVG:  return "image/svg+xml";
        case HTML: return "text/html";
        case CSS:  return "text/css";
        case JS:   return "application/javascript";
        case MP4:  return "video/mp4";
        case WEBM: return "video/webm";
        case OGG:  return "video/ogg";
        default:   return "application/octet-stream";
    }
}

char *get_file_path(const char *wpath, content_type_t *ftype)
{
    *ftype = get_file_type(wpath);
    char *path = malloc(512);
    if (*ftype == UNKNOWN) {
        *ftype = HTML;
        snprintf(path, 512, "pages%s.html", wpath);
    } else {
        snprintf(path, 512, "pages%s", wpath);
    }
    return path;
}

void send_file(const int client_fd, const struct http_request req)
{
    content_type_t file_type;
    char *path = get_file_path(req.path, &file_type);

    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        send(client_fd, FILE_NOT_FOUND, strlen(FILE_NOT_FOUND), 0);
        free(path);
        return;
    }

    const char *content_type = get_content_type(file_type);
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);

    char header[256];
    snprintf(header, sizeof(header),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %ld\r\n"
        "Accept-Ranges: bytes\r\n"
        "\r\n", content_type, size);

    send(client_fd, header, strlen(header), 0);

    char chunk[65536];
    size_t n;
    while ((n = fread(chunk, 1, sizeof(chunk), f)) > 0) {
        send(client_fd, chunk, n, 0);
    }
    fclose(f);
    free(path);
}

void redirect(const int client_fd, const char *path)
{
    char response[256];
    snprintf(response, sizeof(response),
        "HTTP/1.1 302 Found\r\n"
        "Location: %s\r\n"
        "Content-Length: 0\r\n"
        "\r\n", path);

    send(client_fd, response, strlen(response), 0);
}

void *handle_client(void *arg)
{
    int client_fd = *(int *)arg;
    free(arg);

    char buf[4096];
    int bytes = recv(client_fd, buf, sizeof(buf) - 1, 0);
    if (bytes > 0) {
        buf[bytes] = '\0';

        struct http_request req;
        sscanf(buf, "%s %s %s", req.method, req.path, req.version);

        if (DEBUG_ENABLED) {
            printf("method:  %s\n", req.method);
            printf("path:    %s\n", req.path);
            printf("version: %s\n", req.version);
        }

        if (strcmp(req.path, "/") == 0) {
            redirect(client_fd, "/home");
            close(client_fd);
            return NULL;
        }

        if (strstr(buf, "Sec-Fetch-Dest: document") == NULL || is_valid_web_page(req.path)) {
            send_file(client_fd, req);
        } else {
            redirect(client_fd, "/help");
        }

    }
    close(client_fd);

    return NULL;
}

int create_server_socket(const char *port)
{
    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    int status = getaddrinfo(NULL, port, &hints, &res);
    if (status != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
        return -1;
    }

    int sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sockfd == -1) {
        perror("socket");
        return -1;
    }

    int yes = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes);

    if (bind(sockfd, res->ai_addr, res->ai_addrlen) == -1) {
        perror("bind");
        return -1;
    }
    freeaddrinfo(res);

    if (listen(sockfd, SOMAXCONN) == -1) {
        perror("listen");
        return -1;
    }

    return sockfd;
}

int main()
{
    int sockfd = create_server_socket("8080");
    if (sockfd == -1) return 1;

    signal(SIGPIPE, SIG_IGN);

    while (1) {
        struct sockaddr addr;
        unsigned int addr_len = sizeof(addr);

        int client_fd = accept(sockfd, &addr, &addr_len);
        if (client_fd == -1) continue;

        int *arg = malloc(sizeof(int));
        *arg = client_fd;

        pthread_t thread;
        pthread_create(&thread, NULL, handle_client, arg);
        pthread_detach(thread);
    }

    return 0;
}
