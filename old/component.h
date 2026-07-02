#ifndef COMPONENT_H
#define COMPONENT_H

#include <sys/socket.h>
#include "_input.h"
#include "_output.h"


class component {
    private:
        _input *_inputs[256] ;
        _output *_outputs[256] ;
        int _nb_inputs = 0;
        int _nb_outputs = 0 ;
        int ctrl_cli_fd, ctrl_srv_fd ;

    public:
        component(){
            printf("in component()\n") ;
            memset(_inputs, 256, sizeof(_input *)) ;
            memset(_outputs, 256, sizeof(_output *)) ;
            int fd[2] ;
            if (socketpair(PF_LOCAL, SOCK_STREAM, 0, fd) < 0){
                fprintf(stderr, "socketpair error: %s\n", strerror(errno)) ; 
                exit(1) ; 
            }
            ctrl_cli_fd = fd[0] ;
            ctrl_srv_fd = fd[1] ;

            run() ;  
            printf("out component()\n") ;
        }

        // Called in the parent, we need to send ourselves a message
        int add_input(_input *i){
            printf("add_input\n") ;
            _inputs[_nb_inputs++] = i ;
            return _nb_inputs - 1 ;
            /* uint32_t cmd = 'I' << 24 | i->get_r_fd() & 0xFF ;  
            if (write(get_ctrl_fd(), &cmd, sizeof(uint32_t)) < 0){
                fprintf(stderr, "write error: %s\n", strerror(errno)) ; 
                exit(1) ;
            }
            uint8_t id ;
            int n = read(get_ctrl_fd(), &id, sizeof(uint8_t)) ;
            return id ; */
            return 0 ;
        }

        int add_output(_output *o){
            //_outputs[_nb_outputs++] = o ;
            //return _nb_outputs - 1 ;
            return 0 ;
        }

        int get_ctrl_fd(){
            return ctrl_cli_fd ;
        }

        void run(){
            pid_t pid = fork() ;
            if (pid < 0){
                fprintf(stderr, "fork error: %s\n", strerror(errno)) ; 
                exit(1) ;
            }
            else if (pid > 0){
                // Parent
                return ;
            }

            _input *_inputs[256] ;
            _output *_outputs[256] ;
            int _nb_inputs = 0;
            int _nb_outputs = 0 ;
            memset(_inputs, 256, sizeof(_input *)) ;
            memset(_outputs, 256, sizeof(_output *)) ;

            printf("in child, nb_inputs=%d\n", _nb_inputs) ;
            while (1){
                fd_set input_fds ;
                FD_ZERO(&input_fds) ;
                FD_SET(ctrl_srv_fd, &input_fds) ;
                int maxfd = ctrl_srv_fd ;
                for (int i = 0 ; i < _nb_inputs ; i++){
                    int fd = _inputs[i]->get_r_fd() ;
                    if (fd > maxfd){
                        maxfd = fd ;
                    } 
                    FD_SET(fd, &input_fds) ;
                }
                if (select(maxfd+1, &input_fds, nullptr, nullptr, nullptr) < 0){
                    fprintf(stderr, "select error: %s\n", strerror(errno)) ; 
                    exit(1) ;
                }
                printf("got input\n") ;
                for (int i = 0 ; i < 1024 ; i++){
                    if (FD_ISSET(i, &input_fds)){
                        if (i == ctrl_srv_fd){
                            // Process control command
                            uint32_t c ;
                            int n = read(i, &c, sizeof(uint32_t)) ;
                            if (n < 0){
                                fprintf(stderr, "read error: %s\n", strerror(errno)) ; 
                                exit(1) ;   
                            }
                            else if (n == 0){
                                // Writer is closed, we shutdown
                                exit(0) ;
                            }
                            else if (n < sizeof(uint32_t)){
                                fprintf(stderr, "partial read from pipe: %s\n", strerror(errno)) ; 
                                exit(1) ;   
                            }
                            else {
                                uint8_t cmd = c >> 24 ; 
                                printf("command read: %c\n", cmd) ;
                                //switch (cmd){
                                //    case 'I':
                                //        _inputs[_nb_inputs++] = cmd & 0xFF ;
                                //}
                            }
                        }
                        else {
                            uint32_t v ;
                            int n = read(i, &v, sizeof(uint32_t)) ;
                            if (n < 0){
                                fprintf(stderr, "read error: %s\n", strerror(errno)) ; 
                                exit(1) ;   
                            }
                            else if (n == 0){
                                // Writer is closed, we shutdown
                                exit(0) ;
                            }
                            else if (n < sizeof(uint32_t)){
                                fprintf(stderr, "partial read from pipe: %s\n", strerror(errno)) ; 
                                exit(1) ;   
                            }
                            else {
                                _inputs[i]->set_value(v) ;
                                always(_inputs[i]) ;
                            }
                        }
                    }
                }
            }
        }

        virtual void always(const void *trigger) = 0 ;
} ;


#endif