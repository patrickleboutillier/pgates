#ifndef _OUTPUT_H
#define _OUTPUT_H

#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <unordered_set>
#include <_input.h>

using namespace std ;


// Base class for all outputs
class _output {
    private:
        uint32_t _value ;
        uint32_t _mask ;
        bool _set ;
    protected:
        bool _drive ;
        unordered_set<_input *> _connected_inputs ;

    public:
        _output(uint32_t w, uint32_t v = 0) {
            _set = false ;
            _value = v ;
            _mask = (1 << w) - 1 ;
            _drive = true ;
        } ;

        void set_value(uint32_t v){
            if ((v != _value)||(! _set)){ 
                _value = v & _mask ;
                _set = true ;
                if (_drive){
                    // Iterate through all the connected_inputs and call always()
                    typename unordered_set<_input *>::iterator it ;
                    for (it = _connected_inputs.begin() ; it != _connected_inputs.end() ; it++){
                        (*it)->set_value(_value) ;
                        //(*it)->always() ;
                    }
                }
            }
        }

        // Really only makes sense for output<1>...
        void toggle(){
            set_value(~ _value) ;
        }

        void pulse(){
            toggle() ;
            toggle() ;
        }

        uint32_t get_value(){
            return _value ;
        }

        operator uint32_t(){
            return get_value() ;
        }

        void drive(bool d){
            _drive = d ;

            typename unordered_set<_input *>::iterator it ;
            for (it = _connected_inputs.begin(); it != _connected_inputs.end() ; it++){
                if (_drive){
                    (*it)->set_value(_value) ;
                    //(*it)->always() ;
                }
                else {
                    // Simulate pull down resistor
                    set_value(0) ;
                    (*it)->set_value(_value) ;
                    //(*it)->always() ;
                }
            }
        }
} ;


#endif