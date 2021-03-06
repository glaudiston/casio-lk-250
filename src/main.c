/* miditi source code
// author: Glaudiston Gomes da Silva <glaudistong@gmail.com>
*/

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <errno.h>
#include <sys/epoll.h>
#include <sys/stat.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <string.h>

#include "src/sha1.c"

int LISTEN_PORT = 8080;

#define HTTP_DATA_BUFFER_LEN 1024
#define HTTP_HEADER_METHOD 		0
#define HTTP_HEADER_PATH		1
#define HTTP_HEADEE_PROTOCOL		2
#define HTTP_HEADER_CONNECTION 		3
#define HTTP_HEADER_UPGRADE 		4
#define HTTP_HEADER_SEC_WEBSOCKET_KEY 	5
#define HTTP_SUPPORTED_HEADERS_COUNT 	6
#define HTTP_HEADER_BUFFER_LEN 		256
struct request {
	int sockfd;
	ssize_t data_size;
	char data[HTTP_DATA_BUFFER_LEN];
	char http_method[6];
	char http_resource_path[256];
	char http_protocol[10];
	char http_headers[HTTP_SUPPORTED_HEADERS_COUNT][HTTP_HEADER_BUFFER_LEN];
};

struct response {
	char proto[5];
	char code[4];
	char status_text[256];
	char content_type[256];
	char raw_header[1024];
	char body[1024];
	ssize_t body_size;
	char data[4096];
	ssize_t data_size;
	int keep_alive;
};

char * get_mime_type(char * filepath) {
	printf("mime for %s\n",filepath);
	if ( strcmp(filepath, "static/favicon.ico") == 0 ) {
		return "image/vnd.microsoft.icon";
	}
	if ( strcmp(filepath, "static/dat.gui.min.js") == 0 ) {
		return "application/javascript; charset=utf-8";
	}
	if ( strcmp(filepath, "static/script.js") == 0 ) {
		return "application/javascript; charset=utf-8";
	}
	return "text/html; charset=utf-8";
}

char *STATIC_FILES_PREFIX="static";
struct response read_file_to_stream(struct request req, struct response resp) {
	char filepath[256];
	char path_to_look[256];
	if ( strcmp( req.http_resource_path, "/") == 0 ){
		sprintf(path_to_look, "/index.html");
	} else {
		sprintf(path_to_look, "%s", req.http_resource_path);
	}
	sprintf(filepath, "%s%s", STATIC_FILES_PREFIX, path_to_look);

	char line[1024];
	// read the file stat to get size
	struct stat statbuf;
	if ( stat(filepath, &statbuf) == -1 ){
		printf("stat fail: %m\n");
		perror("Fail to get stats");
		sprintf(resp.code, "400");
		sprintf(resp.status_text, "Not found");
		sprintf(line, "%s %s %s\n\n", resp.proto, resp.code, resp.status_text);
		send(req.sockfd, line, strlen(line), 0);
		close(req.sockfd);
		return resp;
		//TODO error
	}
	sprintf(resp.code, "200");
	sprintf(resp.status_text, "OK");
	sprintf(resp.content_type, "content-type: %s", get_mime_type(filepath));
	sprintf(line, "%s %s %s\n%s\n\n", resp.proto, resp.code, resp.status_text, resp.content_type);
	send(req.sockfd, line, strlen(line), 0);
	printf("Response raw_header: [%s]", line);
	FILE *f = fopen(filepath, "r");
	if ( !f ){
		printf("Unable to open file.");
		//TODO: set response error
	}

	char c;
	int i=0;
	while ( fread(&c, 1 , 1,f) == 1 )
	{
		i++;
		if ( send(req.sockfd, &c, 1, 0)  != 1 ){
			printf("Fail to send bytes over the socket: %m\n");
		}
	}

	if ( !resp.keep_alive ) {
		close(req.sockfd);
	}
	printf("File has %ld bytes, %m, %i\n", statbuf.st_size, i);
	return resp;
}

int allowed_static_path(char * resource_path){
	int allow = 0;
	allow |= ( strcmp(resource_path, "/" ) == 0 );
	allow |= ( strcmp(resource_path, "/dat.gui.min.js" ) == 0 );
	allow |= ( strcmp(resource_path, "/favicon.ico" ) == 0 );
	allow |= ( strcmp(resource_path, "/script.js" ) == 0 );
	return allow;
}

struct response upgrade_ws(struct request req, struct response resp)
{
	char line[1024];
	sprintf(resp.code, "101");
	sprintf(resp.status_text, "Switching Protocols");
	sprintf(line, "%s %s %s\nSec-WebSocket-Key: %s\nSec-WebSocket-Accept: %s\n\n",
			resp.proto,
			resp.code,
			resp.status_text,
			req.http_headers[HTTP_HEADER_SEC_WEBSOCKET_KEY],
			resp.http_headers[HTTP_RESP_HEADER_SEC_WEBSOCKET_ACCEPT]);
	printf("WS HANDSHAKE: [%s]", line);
	send(req.sockfd, line, strlen(line), 0);
	while (( req.data_size = recv(req.sockfd, req.data, sizeof(req.data), 0)) < 0)
	{
		if ( (req.sockfd < 0) && (errno != EINTR))
		{
			printf("Receive from websocket %d failed: %m\n", req.sockfd);
			shutdown(req.sockfd, SHUT_RDWR);
			close(req.sockfd);
			continue;
		}
	}
	printf("WEBSOCKET DATA %s", req.data);
	return resp;
}

struct request parse_headers(struct request req){
	if ( strncmp(req.data, "GET ", 4) == 0 ){
		int i;
		int l = sizeof(req.data);
		char line[sizeof(req.data)];
		char header_name[sizeof(req.data)];
		char header_value[HTTP_HEADER_BUFFER_LEN];
		int header_value_len = 0;
		int line_len=0;
		int cur_field = 0;
		for ( i=0; i < l; i++ ){
			if ( req.data[i] == 0 ) {
				break;
			}
			if ( req.data[i] == 13 ) {
				continue;
			}
			line[line_len++] = req.data[i];
			if ( req.data[i] == 10 ) {
				if ( line_len == 1 ){
					// two consectives '\n'. End of headers
					break;
				}
				printf("Header [%s]=[%s]\n", header_name, header_value);

				if ( strcmp( header_name, "Connection") == 0 ) {
					sprintf(req.http_headers[HTTP_HEADER_CONNECTION], "%s", header_value);
				}
				if ( strcmp( header_name, "Upgrade") == 0 ) {
					sprintf(req.http_headers[HTTP_HEADER_UPGRADE], "%s", header_value);
				}
				if ( strcmp( header_name, "Sec-WebSocket-Key" ) == 0 ) {
					sprintf(req.http_headers[HTTP_HEADER_SEC_WEBSOCKET_KEY], "%s", header_value);
				}
				header_name[0]=0;
				header_value_len=0;
				header_value[0]=0;
				line_len=0;
				line[0]=0;
				cur_field = 0;
				continue;
			}
			if ( cur_field == 0 && req.data[i] == ':' ) {
				strncpy( header_name, line, line_len-1 );
				header_name[line_len-1]=0;
				cur_field=1;
				i++; i++; // jump the 2 bytes: ': ';
			}
			if ( cur_field == 1 ) {
				header_value[header_value_len++]=req.data[i];
				header_value[header_value_len]=0;
			}
		}
		// req = parse_headers(req);
		// int is_to_upgrade_ws = strcmp(req.header_upgrade, "websocket") == 0;
	}

	return req;
}

int upgrade_ws_requested(struct request req)
{
	int connection_upgrade = strcmp( req.http_headers[HTTP_HEADER_CONNECTION], "keep-alive, Upgrade" ) == 0;
	int upgrade_websocket = strcmp( req.http_headers[HTTP_HEADER_UPGRADE], "websocket" ) == 0;
	return connection_upgrade && upgrade_websocket;
}

struct response process_request(struct request req)
{
	struct response resp;
	resp.keep_alive = 0;
	printf("data received: %m\n%s\n", req.data);

	sscanf(req.data, "%s %s %s\n"
			, req.http_method
			, req.http_resource_path
			, req.http_protocol);
	// int scanpos = strlen(req.http_method)+strlen(req.http_resource_path)+strlen(req.http_protocol)+3;
	sprintf(resp.proto, "HTTP/1.1");
	sprintf(resp.code, "404");
	sprintf(resp.status_text, "Not found");
	sprintf(resp.content_type, "content-type: text/plain; charset=utf-8");
	// detect http request method
	if ( strcmp(req.http_method, "GET") == 0 ){
		req = parse_headers(req);
		printf("looking for [%s]...\n",req.http_resource_path);
		if ( upgrade_ws_requested(req) ) {
			printf("upgrading to websocket connection...\n");
		 	return upgrade_ws(req, resp);
		}
		if ( allowed_static_path(req.http_resource_path) ) {
			return read_file_to_stream(req, resp);
		}
	}

	sprintf(resp.raw_header, "%s %s %s\n%s", resp.proto, resp.code, resp.status_text, resp.content_type);
	sprintf(resp.body,"resource not found\n");
	sprintf(resp.data, "%s\n%s", resp.raw_header, resp.body);
	resp.data_size = strlen(resp.data);
	printf("response:\n%s\n",resp.data);
	resp.data[resp.data_size]=0;
	if ( resp.data_size > 0 ) {
		send(req.sockfd, resp.data, resp.data_size, 0);
	}
	if ( !resp.keep_alive ) {
		close(req.sockfd);
	} else {
		printf("socket keep_alive\n");
	}
	return resp;
}

int start_server()
{
	int sockfd = socket( AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0 );
	if ( sockfd == -1 ) {
		perror("Fail to create a socket");
		return 1;
	}

	int on = 1;
	if ( setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)))
	{
		printf("Could not set %d option for reusability: %m\n", sockfd);
		return 2;
	}

	struct sockaddr_in addr, peeraddr;
	socklen_t perrlen = sizeof(peeraddr);
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(LISTEN_PORT);
	socklen_t addrlen = sizeof(addr);
	int r = bind( sockfd, (struct sockaddr *)&addr, addrlen );
	if ( r == -1 ) {
		perror("Fail to bind address");
		return 3;
	}

	int ret = listen(sockfd, SOMAXCONN);
	if ( ret == -1 ) {
		perror("Fail to listen the port");
		return 4;
	}

	int epollfd = epoll_create(1);
	if ( r == -1 ){
		perror("Fail to create epoll");
		return 5;
	}

	struct epoll_event ev;
	ev.events = EPOLLIN;
	ev.data.u64 = 0LL;
	ev.data.fd = sockfd;
	if (epoll_ctl(epollfd, EPOLL_CTL_ADD, sockfd, &ev) < 0 )
	{
		printf("Could not add server socket %d to epoll set: %m\n", sockfd);
	}

	int maxevents=10;
	struct epoll_event events[maxevents];
	int timeout = 60000;
	for (;;)
	{
		int eventcount = epoll_wait(epollfd, events, maxevents, timeout);
		if ( eventcount == -1 ) {
			perror("Fail to query epoll events.\n");
		} else if ( eventcount == 0 ) {
			printf("No new events in last %i seconds...\n", timeout  / 1000);
		} else {
			printf("There are %i events.\n", eventcount);
			int i = 0;
			for (i=0;i<eventcount;i++){
				uint32_t cur_events = events[i].events;
				int cur_fd = events[i].data.fd;
				if ( cur_events & EPOLLERR )
				{
					if ( cur_fd == sockfd ) {
						printf("epoll fail. Closing socket with fd %d.\n", cur_fd);
					} else {
						printf("Closing socket %d.\n", cur_fd);
					}
					close(cur_fd);
					continue;
				}
				if ( cur_events & EPOLLHUP )
				{
					printf("Closing socket with fd %d.\n", cur_fd);
					close(cur_fd);
					continue;
				}
				if ( cur_events & EPOLLOUT )
				{
					printf("in epollout");
					if ( cur_fd != sockfd )
					{

					}
					close(cur_fd);
					continue;
				}
				if ( cur_events & EPOLLIN )
				{
					if ( cur_fd == sockfd ) {
						// init a new connection
						int clientfd;
						while (( clientfd = accept( sockfd, (struct sockaddr *) &peeraddr , &perrlen) ) < 0) {
							if ( clientfd == -1 && errno != EINTR ) {
								printf("Accept on socket %d failed: %m\n", cur_fd);
								close(cur_fd);
								return 6;
							}
						}

						char buffer[1024];
						if ( inet_ntop(AF_INET, &peeraddr.sin_addr.s_addr, buffer, sizeof(buffer)) != NULL)
						{
							printf("Accepted connection from %s:%u assigned new sd %d\n", buffer, ntohs(peeraddr.sin_port), clientfd);
						}
						else
						{
							printf("Failed to convert address from binary to text form: %m\n");
						}

						ev.events = EPOLLIN;
						ev.data.u64 = 0LL;
						ev.data.fd = clientfd;

						if ( epoll_ctl(epollfd, EPOLL_CTL_ADD, clientfd, &ev) == -1 )
						{
							printf("Could not add client socket %d to epoll set: %m\n", clientfd);
							return 7;
						}
					}
					else
					{
						struct request req;

						req.sockfd = cur_fd;
						// receive data
						while (( req.data_size = recv(cur_fd, req.data, sizeof(req.data), 0)) < 0)
						{
							if ( (cur_fd < 0) && (errno != EINTR))
							{
								printf("Receive from socket %d failed: %m\n", cur_fd);
								shutdown(cur_fd, SHUT_RDWR);
								close(cur_fd);
								continue;
							}
						}

						if (req.data_size == 0)
						{
							printf("No data from socket with fd %d. Closing.\n",cur_fd);
							close(cur_fd);
							continue;
						}
						req.data[req.data_size] = 0;

						//struct response resp =
						process_request(req);

					}
				}
			}
		}
		fflush(stdout);
	}
	return 0;
}

int main(int argc, char ** argv)
{
	int r = start_server();
	return r;
}
