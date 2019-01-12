//
// Created by bin zhang on 2019/1/6.
//

#ifndef INNERPROC_HANDLER_H
#define INNERPROC_HANDLER_H

struct MessageHandler{
	void onMessage(const std::shared_ptr<Message>& message) = 0;
};

struct HttpHandler{
	void onRequest(const std::shared_ptr<HttpRequest>& req,std::shared_ptr<HttpResponse>& resp) =  0;
};


#endif //INNERPROC_HANDLER_H
