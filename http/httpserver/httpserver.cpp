#include <stdlib.h>
#include <memory.h>
#include <event2/event.h>
#include <event2/http.h>
#include <event2/buffer.h>
#include <event2/util.h>
#include <event2/keyvalq_struct.h>

void generic_handler(struct evhttp_request *req, void *arg)
{
    struct evbuffer *buf = evbuffer_new();
    if(!buf)
    {
        puts("failed to create response buffer \n");
        return;
    }
    char *cmdtype;
    switch (evhttp_request_get_command(req)){
        case EVHTTP_REQ_GET:
            cmdtype = "GET";
            break;
        case EVHTTP_REQ_POST:
            cmdtype = "POST";
            break;
        case EVHTTP_REQ_HEAD:
            cmdtype = "HEAD";
             break;
        default:
            cmdtype = "unknown";
            break;
            
    }
    evbuffer_add_printf(buf, "Server Receive %s Responsed. Requested: %s\n", cmdtype, evhttp_request_get_uri(req));
    
    struct evkeyvalq *headers = evhttp_request_get_input_headers(req);
    struct evkeyval *header = headers->tqh_first;
    for (header = headers->tqh_first; header; header = header->next.tqe_next) {
        printf("header  %s: %s\n", header->key, header->value);  
    }  
    
    
    struct evbuffer *buff = evhttp_request_get_input_buffer(req);
    while (evbuffer_get_length(buff)) {
        char cbuf[1024];
        int n = evbuffer_remove(buff, cbuf, 1024-1);
        if(n > 0){
            printf("server  %s \n", cbuf);
        }
        
    }
    evhttp_send_reply(req, HTTP_OK, "OK", buf);
    evbuffer_free(buf);
}

int main(int argc, char* argv[])
{
    short http_port = 80;
    char  *http_addr = "127.0.0.1";    
    struct event_base * base = event_base_new();
    struct evhttp * http_server = evhttp_new(base);
    if(!http_server)
    {
        return -1;
    }

    int ret = evhttp_bind_socket(http_server,http_addr,http_port);
    if(ret!=0)
    {
        return -1;
    }

    evhttp_set_gencb(http_server, generic_handler, NULL);

    printf("http server start OK! \n");

    event_base_dispatch(base);

    evhttp_free(http_server);
    return 0;
}
