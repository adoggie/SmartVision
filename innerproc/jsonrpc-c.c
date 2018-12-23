/*
 * jsonrpc-c.c
 *
 *  Created on: Oct 11, 2012
 *      Author: hmng
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>

#include "jsonrpc-c.h"
#include "iniparser.h"
//#include "innerproc.h"

#include <yajl/yajl_parse.h>
#include <yajl/yajl_gen.h>


char *heartbeat = "{\"method\":\"heartbeat\"}\n^";
//extern app_client_t * g_app_client_list;
//extern char g_localaddress[64];

static int __jrpc_server_start(struct jrpc_server *server);
static void jrpc_procedure_destroy(struct jrpc_procedure *procedure);

struct ev_loop *loop;

// get sockaddr, IPv4 or IPv6:
static void *get_in_addr(struct sockaddr *sa) {
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*) sa)->sin_addr);
	}
	return &(((struct sockaddr_in6*) sa)->sin6_addr);
}

static int send_response(struct jrpc_connection * conn, char *response) {
	int fd = conn->fd;
	char *tmpstr = malloc(strlen(response)+2);
	memset(tmpstr,0,strlen(response)+2);

	printf("== send_response() start %d\n",time(NULL));
//	if (conn->debug_level > 1)
		printf("JSON Response:\n%s\n", response);
	sprintf(tmpstr,"%s^",response);
	write(fd, tmpstr, strlen(tmpstr));
	printf("== send_response() end %d\n",time(NULL));
	free(tmpstr);
	return 0;
}

static int send_error(struct jrpc_connection * conn, int code, char* message,
		cJSON * id) {
	int return_value = 0;
	cJSON *result_root = cJSON_CreateObject();
	cJSON *error_root = cJSON_CreateObject();
	cJSON_AddNumberToObject(error_root, "code", code);
	cJSON_AddStringToObject(error_root, "message", message);
	cJSON_AddItemToObject(result_root, "error", error_root);
	cJSON_AddItemToObject(result_root, "id", id);
	char * str_result = cJSON_Print(result_root);
	return_value = send_response(conn, str_result);
	free(str_result);
	cJSON_Delete(result_root);
	free(message);
	return return_value;
}

static int send_result(struct jrpc_connection * conn, cJSON * result,
		cJSON * id) {
	int return_value = 0;
	if(!result)
		return 0;
	cJSON *result_root = cJSON_CreateObject();
	if (result)
		cJSON_AddItemToObject(result_root, "result", result);
	cJSON_AddItemToObject(result_root, "id", id);

	char * str_result = cJSON_Print(result_root);
	return_value = send_response(conn, str_result);
	free(str_result);
	cJSON_Delete(result_root);
	return return_value;
}


static int invoke_procedure(struct jrpc_server *server,
		struct jrpc_connection * conn, char *name, cJSON *params, cJSON *id) {
	cJSON *returned = NULL;
	int procedure_found = 0;
	jrpc_context ctx;
	ctx.error_code = 0;
	ctx.error_message = NULL;
	int i = server->procedure_count;
	while (i--) {
		if (!strcmp(server->procedures[i].name, name)) {
			procedure_found = 1;
			//ctx.data = server->procedures[i].data;
			//if(!strcmp(name,"door_open_state"))
			ctx.data = conn;
			returned = server->procedures[i].function(&ctx, params, id);
			break;
		}
	}
	if (!procedure_found)
		return send_error(conn, JRPC_METHOD_NOT_FOUND,
				strdup("Method not found."), id);
	else {
		if (ctx.error_code)
			return send_error(conn, ctx.error_code, ctx.error_message, id);
		else
		{
			return send_result(conn, returned, id);
		}
	}
}

static int eval_request(struct jrpc_server *server,
		struct jrpc_connection * conn, cJSON *root) {
	cJSON *method, *params, *id;
	cJSON *result;
	result = cJSON_GetObjectItem(root,"result");
	if (result != NULL && result->type == cJSON_String) {
		cJSON *audio = cJSON_GetObjectItem(root,"audio");
		if(audio !=NULL && audio->type == cJSON_String && strncmp(result->valuestring,"doorinfoget",11) == 0)
		{
			printf("audio info: %s\n",audio->valuestring);
		}
		else
		{
			printf("door result:%s\n",result->valuestring);
			send_error(conn, JRPC_INVALID_REQUEST,
				strdup("Audio info error"), NULL);
			return -1;
		}
		return 0;

	}

	method = cJSON_GetObjectItem(root, "method");
	if (method != NULL && method->type == cJSON_String) {
		params = cJSON_GetObjectItem(root, "params");
		if (params == NULL|| params->type == cJSON_Array
		|| params->type == cJSON_Object) {
			id = cJSON_GetObjectItem(root, "id");
			if (id == NULL|| id->type == cJSON_String
			|| id->type == cJSON_Number) {
			//We have to copy ID because using it on the reply and deleting the response Object will also delete ID
				cJSON * id_copy = NULL;
				if (id != NULL)
					id_copy =
							(id->type == cJSON_String) ? cJSON_CreateString(
									id->valuestring) :
									cJSON_CreateNumber(id->valueint);
//				if (server->debug_level)
					printf("Method Invoked: %s\n", method->valuestring);
				//ztf for test interface not the result
/*
				if(strcmp(method->valuestring,"opendoor") == 0 && server->port_number == IN_PORT)
				{
					printf("OpenDoor CMD GET!\n");
					return 0;
				}
*/
				return invoke_procedure(server, conn, method->valuestring,
						params, id_copy);
			}
		}
	}
	send_error(conn, JRPC_INVALID_REQUEST,
			strdup("The JSON sent is not a valid Request object."), NULL);
	return -1;
}

static void close_connection(struct ev_loop *loop, ev_io *w) {
	ev_io_stop(loop, w);
	close(((struct jrpc_connection *) w)->fd);

	if (((struct jrpc_connection *) w)->buffer){
		free(((struct jrpc_connection *) w)->buffer);
	}
	free(((struct jrpc_connection *) w));
}

#define  MaxReadBufferSize 1024

static void connection_cb(struct ev_loop *loop, ev_io *w, int revents) {
	struct jrpc_connection *conn;
	struct jrpc_server *server = (struct jrpc_server *) w->data;
	size_t bytes_read = 0;
	//get our 'subclassed' event watcher
	conn = (struct jrpc_connection *) w;
	// jrpc_connection 结构内第一个便是ev_io,所以两者地址一样，这么玩，老夫服了
	int fd = conn->fd;


	char *read_buf = malloc(MaxReadBufferSize+1);
	int ret = 0;
	memset(read_buf,0,MaxReadBufferSize+1);

	ret = read(fd,read_buf,MaxReadBufferSize);
	if(ret == -1){
		perror("read failed.");
		free(read_buf);
		return close_connection(loop, w);
	}
	if(ret == 0){
		free(read_buf);
		if (server->debug_level)
			printf("Client closed connection.\n");
		return close_connection(loop, w);
	}
	printf("read data:%s\n",read_buf);
//	free(read_buf);
//	return ;

	char * parse_buf = NULL;

	if(conn->buffer){
		int buf_size = strlen(conn->buffer);
		int new_size = buf_size + ret + 1 ;
		parse_buf = malloc(new_size);

		memset(parse_buf,0,new_size);
		memcpy(parse_buf,conn->buffer,buf_size);
		memcpy(parse_buf + buf_size,read_buf,ret);
		free(conn->buffer);
		conn->buffer = NULL;

		free(read_buf);

	}else{
		parse_buf = read_buf;
	}
	// parse_buf  保存了当前最新的json数据缓存，可能包含多个json消息，还有不完整的json消息


	yajl_handle hand;
	yajl_status stat;
	yajl_gen_config conf = { 0, "  " };
	yajl_gen g;
	yajl_parser_config cfg = { 1, 1 };

//	g = yajl_gen_alloc(&conf, NULL); // 这个东西分配了产生内存泄露

//	hand = yajl_alloc(NULL, &cfg, NULL, (void *) g);
	hand = yajl_alloc(NULL, &cfg, NULL, NULL);

	char *cursor = parse_buf;
	stat = yajl_parse(hand, (const unsigned char*)cursor, strlen(cursor));

	int consumed = 0 ;
	while( stat == yajl_status_ok) {
		consumed = yajl_get_bytes_consumed(hand);

//		yajl_gen_clear(g);
		yajl_free(hand);

		if(consumed<=0 ){
			printf("------------------- consumed is 0 -----------\n");
			break;
		}
		char * json_msg = malloc( consumed + 1);
		json_msg[consumed] = '\0';
		memcpy(json_msg,cursor,consumed);
		get_json_message(server,conn,json_msg);
		free(json_msg);

		cursor+=consumed;
//		g = yajl_gen_alloc(&conf, NULL);
//		hand = yajl_alloc(NULL, &cfg, NULL, (void *) g);
		hand = yajl_alloc(NULL, &cfg, NULL, NULL);
		stat = yajl_parse(hand, (const unsigned char*)cursor, strlen(cursor));
	}

//	yajl_gen_clear(g);
	yajl_free(hand);


	if(stat == yajl_status_insufficient_data ){
		conn->buffer = malloc(strlen(cursor)+1);
		conn->buffer[strlen(cursor)] = '\0';
		strcpy(conn->buffer,cursor);
	}



	if (stat != yajl_status_ok && stat != yajl_status_insufficient_data){
		on_json_message_error(loop,w,cursor);
	}
	free(parse_buf);

}


void on_json_message_error(struct ev_loop *loop,ev_io *w,char * msg){
	struct jrpc_connection *conn = (struct jrpc_connection *) w;
	struct jrpc_server *server = (struct jrpc_server *) w->data;

	if (server->debug_level) {
		printf("INVALID JSON Received:\n---\n%s\n---\n", msg);
	}
	send_error(conn, JRPC_PARSE_ERROR,
			   strdup("Parse error. Invalid JSON was received by the server."), NULL);
	return close_connection(loop, w);
}

//
void get_json_message(struct ev_loop *loop,ev_io *w, char * msg){
	struct jrpc_connection *conn = (struct jrpc_connection *) w;
	struct jrpc_server *server = (struct jrpc_server *) w->data;


	cJSON * root;
	char * end_ptr;
	printf("parse json message: %s \n",msg);
	if ((root = cJSON_Parse_Stream(msg, &end_ptr)) != NULL) {
		printf("cjson decode succ..\n");
		if (server->debug_level > 1) {
			char * str_result = cJSON_Print(root);
			printf("=== Valid JSON Received:\n%s\n", str_result);
			free(str_result);
		}

		if (root->type == cJSON_Object) {
			eval_request(server, conn, root);
		}

		cJSON_Delete(root);
	} else {
		// did we parse the all buffer? If so, just wait for more.
		// else there was an error before the buffer's end
		on_json_message_error(loop,w,msg);

//		if (server->debug_level) {
//			printf("INVALID JSON Received:\n---\n%s\n---\n", msg);
//		}
//		send_error(conn, JRPC_PARSE_ERROR,
//				   strdup("Parse error. Invalid JSON was received by the server."), NULL);
//		return close_connection(loop, w);

	}
}

static void accept_cb(struct ev_loop *loop, ev_io *w, int revents) {
	char s[INET6_ADDRSTRLEN];
	struct jrpc_connection *connection_watcher;
	connection_watcher = malloc(sizeof(struct jrpc_connection));

	struct sockaddr_storage their_addr; // connector's address information
	socklen_t sin_size;
	sin_size = sizeof their_addr;
	int fd_full = 0;
	int validfd = 0;
	int i = 0;
	int ret = 0;
	//app_client_t * appt = g_app_client_list;
	
	connection_watcher->fd = accept(w->fd, (struct sockaddr *) &their_addr,
			&sin_size);

        //save the fd to connection_fd_array when the port is XXXX
	/*
        if(((struct jrpc_server *) w->data)->port_number == IN_PORT) {
        	for(i=0;i<MAX_FD_NUMBER;i++) {
			if(connection_fd_array[i] == 0) {
				connection_fd_array[connection_watcher->fd] = connection_watcher->fd;
				fd_full = 1;		
				break;
			} else {
				validfd = write(connection_fd_array[i],heartbeat,strlen(heartbeat));
				if(validfd<0) {
					connection_fd_array[i] = 0;
					if(fd_full == 0) {
						connection_fd_array[connection_watcher->fd] = connection_watcher->fd;
						fd_full = 1;
						break;
					}
				}
				
			}

		}

		if(fd_full == 0) {
			send_response(connection_watcher, "error:max connections");//max connections  receached
			free(connection_watcher);
			close(connection_watcher->fd);
			return;
		}
        }
	*/

	if (connection_watcher->fd == -1) {
		perror("accept");
		free(connection_watcher);
	} else {
		if (((struct jrpc_server *) w->data)->debug_level) {
			inet_ntop(their_addr.ss_family,
					get_in_addr((struct sockaddr *) &their_addr), s, sizeof s);
			printf("server: got connection from %s\n", s);
		}
		ev_io_init(&connection_watcher->io, connection_cb,
				connection_watcher->fd, EV_READ);
		//copy pointer to struct jrpc_server
		connection_watcher->io.data = w->data; // 把jsonrpc-server 带过去

		connection_watcher->buffer = NULL;
		/*
		connection_watcher->buffer_size = 1500;
		connection_watcher->buffer = malloc(1500);
		memset(connection_watcher->buffer, 0, 1500);
		 */
		connection_watcher->pos = 0;
		//copy debug_level, struct jrpc_connection has no pointer to struct jrpc_server
		connection_watcher->debug_level =
				((struct jrpc_server *) w->data)->debug_level;
		ev_io_start(loop, &connection_watcher->io);
	}
}

int jrpc_server_init(struct jrpc_server *server, int port_number) {
	int i = 0;

    	//init connection socket fd 
    	/*for(i=0;i<MAX_FD_NUMBER;i++){
	    	connection_fd_array[i] = 0;
    	}*/
    
//    	loop = EV_DEFAULT;
		loop = ev_default_loop(EVBACKEND_SELECT);
    	return jrpc_server_init_with_ev_loop(server, port_number, loop);
}

int jrpc_server_init_with_ev_loop(struct jrpc_server *server, 
        int port_number, struct ev_loop *loop) {
	memset(server, 0, sizeof(struct jrpc_server));
	server->loop = loop;
	server->port_number = port_number;
	char * debug_level_env = getenv("JRPC_DEBUG");
	if (debug_level_env == NULL)
		server->debug_level = 0;
	else {
		server->debug_level = strtol(debug_level_env, NULL, 10);
		printf("JSONRPC-C Debug level %d\n", server->debug_level);
	}
	return __jrpc_server_start(server);
}

static int __jrpc_server_start(struct jrpc_server *server) {
	int sockfd;
	struct addrinfo hints, *servinfo, *p;
	struct sockaddr_in sockaddr;
	unsigned int len;
	int yes = 1;
	int rv;
	char PORT[6];
	sprintf(PORT, "%d", server->port_number);
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE; // use my IP

	if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

// loop through all the results and bind to the first we can
	for (p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol))
				== -1) {
			perror("server: socket");
			continue;
		}

		if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int))
				== -1) {
			perror("setsockopt");
			//exit(1);
			continue;
		}
		//add by ztf
		/*
		if(server->port_number == 6789)
			((struct sockaddr_in *)(p->ai_addr))->sin_addr.s_addr = inet_addr("127.0.0.1");	
		*/
		if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("server: bind");
			continue;
		}

		len = sizeof(sockaddr);
		if (getsockname(sockfd, (struct sockaddr *) &sockaddr, &len) == -1) {
			close(sockfd);
			perror("server: getsockname");
			continue;
		}
		server->port_number = ntohs( sockaddr.sin_port );

		break;
	}

	if (p == NULL) {
		fprintf(stderr, "server: failed to bind\n");
		return 2;
	}

	freeaddrinfo(servinfo); // all done with this structure

	if (listen(sockfd, 5) == -1) {
		perror("listen");
		exit(1);
	}
	if (server->debug_level)
		printf("server: waiting for connections...\n");

	ev_io_init(&server->listen_watcher, accept_cb, sockfd, EV_READ);
	server->listen_watcher.data = server;
	ev_io_start(server->loop, &server->listen_watcher);
	return 0;
}

// Make the code work with both the old (ev_loop/ev_unloop)
// and new (ev_run/ev_break) versions of libev.
#ifdef EVUNLOOP_ALL
  #define EV_RUN ev_loop
  #define EV_BREAK ev_unloop
  #define EVBREAK_ALL EVUNLOOP_ALL
#else
  #define EV_RUN ev_run
  #define EV_BREAK ev_break
#endif

void jrpc_server_run(struct jrpc_server *server){
	EV_RUN(server->loop, 0);
}

int jrpc_server_stop(struct jrpc_server *server) {
	EV_BREAK(server->loop, EVBREAK_ALL);
	return 0;
}

void jrpc_server_destroy(struct jrpc_server *server){
	/* Don't destroy server */
	int i;
	for (i = 0; i < server->procedure_count; i++){
		jrpc_procedure_destroy( &(server->procedures[i]) );
	}
	free(server->procedures);
}

static void jrpc_procedure_destroy(struct jrpc_procedure *procedure){
	if (procedure->name){
		free(procedure->name);
		procedure->name = NULL;
	}
	if (procedure->data){
		free(procedure->data);
		procedure->data = NULL;
	}
}

int jrpc_register_procedure(struct jrpc_server *server,
		jrpc_function function_pointer, char *name, void * data) {
	int i = server->procedure_count++;
	if (!server->procedures)
		server->procedures = malloc(sizeof(struct jrpc_procedure));
	else {
		struct jrpc_procedure * ptr = realloc(server->procedures,
				sizeof(struct jrpc_procedure) * server->procedure_count);
		if (!ptr)
			return -1;
		server->procedures = ptr;

	}
	if ((server->procedures[i].name = strdup(name)) == NULL)
		return -1;
	server->procedures[i].function = function_pointer;
	server->procedures[i].data = data;
	return 0;
}

int jrpc_deregister_procedure(struct jrpc_server *server, char *name) {
	/* Search the procedure to deregister */
	int i;
	int found = 0;
	if (server->procedures){
		for (i = 0; i < server->procedure_count; i++){
			if (found)
				server->procedures[i-1] = server->procedures[i];
			else if(!strcmp(name, server->procedures[i].name)){
				found = 1;
				jrpc_procedure_destroy( &(server->procedures[i]) );
			}
		}
		if (found){
			server->procedure_count--;
			if (server->procedure_count){
				struct jrpc_procedure * ptr = realloc(server->procedures,
					sizeof(struct jrpc_procedure) * server->procedure_count);
				if (!ptr){
					perror("realloc");
					return -1;
				}
				server->procedures = ptr;
			}else{
				server->procedures = NULL;
			}
		}
	} else {
		fprintf(stderr, "server : procedure '%s' not found\n", name);
		return -1;
	}
	return 0;
}
