#ifndef OUTPUT_H
#define OUTPUT_H


#include <stdint.h>
#include <assert.h>
#include "_output.h"
#include "component.h"

using namespace std ;


template <uint32_t W> class output : public _output {
    private:
        component *_component ;
        int _output_id  ;
    
    public:
        output(uint32_t v = 0) : _output(W, v) {
            _component = nullptr ;
            _output_id = 0 ;
        }

        output(component *c, uint32_t v = 0) : _output(W, v) {
            _component = c ;
            // Register the output with its component.
            _output_id = _component->add_output(this) ;
        }

        void operator=(output<W> o){
            set_value(o.get_value()) ;
        }

        void operator=(uint32_t v){
            set_value(v) ;
        }

        void connect(input<W> &i){
            uint32_t cmd = 'A' << 24 | _output_id << 16 | i.get_w_fd() & 0xFFFF ;  
            if (write(i.get_component()->get_ctrl_fd(), &cmd, sizeof(uint32_t)) < 0){
                fprintf(stderr, "write error: %s\n", strerror(errno)) ; 
                exit(1) ; 
            }
            else {
                _connected_inputs.insert(&i) ;
            }
            if (_drive){
                i.set_value(get_value()) ;      
            }
        }

        void disconnect(input<W> &i){
            _connected_inputs.erase(&i) ;
        }
} ;


#endif