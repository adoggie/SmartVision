//
// Created by bin zhang on 2018/12/1.
//

#ifndef INNERPROC_REQUEST_H
#define INNERPROC_REQUEST_H

struct jrpc_server *server;

int server_init(struct jrpc_server *server,int port);
int server_run();
int server_stop();
int server_clean();


#endif //INNERPROC_REQUEST_H
