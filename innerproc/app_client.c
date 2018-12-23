#include "innerproc.h"

static app_client_t current_app_client;


void app_client_init(){
    memset(&current_app_client,0, sizeof(current_app_client));
    current_app_client.line_state = malloc(sizeof(call_line_state_t));
    memset(current_app_client.line_state,0,sizeof(call_line_state_t));

    current_app_client.current_state = OFFLINE;
    current_app_client.socket_fd = -1;
}

app_client_t* get_app_client(){
    return &current_app_client;
}

int send_app_data(char* data,size_t size){
    int ret = -1;
    if(current_app_client.socket_fd >=0 ){
        ret = write(current_app_client.socket_fd,data,size);
    }
    if(ret == -1){
        current_app_client.current_state = OFFLINE;
    }
    return ret;
}

void on_event_app_joined(int fd){
    current_app_client.socket_fd = fd;
    current_app_client.current_state = JOINED;
}

void on_event_socket_disconnected(int fd){
    if(current_app_client.socket_fd == fd){
        current_app_client.current_state = OFFLINE;
        current_app_client.socket_fd = -1;
    }
}