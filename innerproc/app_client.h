//
// Created by bin zhang on 2018/12/15.
//

#ifndef INNERPROC_1_5_0_APP_CLIENT_H
#define INNERPROC_1_5_0_APP_CLIENT_H

#include "innerproc.h"

app_client_t* get_app_client();
int send_app_data(char* data,size_t size);
void on_event_app_joined(int fd);
void on_event_socket_disconnected(int fd);

#endif //INNERPROC_1_5_0_APP_CLIENT_H
