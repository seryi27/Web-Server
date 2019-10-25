void build_header(char *header, char *filePath, int length);

int initialize_server(int port);

int wait_request(int sockfd, struct sockaddr_in *cli_addr);

int send_response(int fd, char *header, void *body, int content_length);

void handle_http_request(int fd);

void resp_error(int fd, int code);

char *strlower(char *s);

char *mime_type_get(char *filename);