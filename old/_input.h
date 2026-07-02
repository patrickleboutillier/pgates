#ifndef _INPUT_H
#define _INPUT_H

#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>


// Base class for all inputs
class _input {
    private:
        uint32_t _value ;
        uint32_t _mask ;
        bool _set ;
    protected:
        int _r_fd, _w_fd ;

    public:
        _input(uint32_t w) {
            _set = false ;
            _value = 0 ;
            _mask = (1 << w) - 1 ;

            int fd[2] ;
            if (pipe(fd) < 0) {
                fprintf(stderr, "pipe error: %s\n", strerror(errno)) ; 
                exit(1) ; 
            }
            _r_fd = fd[0] ;
            _w_fd = fd[1] ;    
        } ;

        void set_value(uint32_t v){
            if ((v != _value)||(! _set)){ 
                _value = v & _mask ;
                _set = true ;
                // Write value to pipe
                if (write(_w_fd, &_value, sizeof(uint32_t)) < 0){
                    fprintf(stderr, "pipe error: %s\n", strerror(errno)) ; 
                    exit(1) ; 
                }
            }
        }

        uint32_t get_value(){
            return _value ;
        }

        operator uint32_t(){
            return get_value() ;
        }

        int get_r_fd(){
            return _r_fd ;
        }

        int get_w_fd(){
            return _w_fd ;
        }
} ;


#endif
