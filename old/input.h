#ifndef INPUT_H
#define INPUT_H

#include <algorithm>
#include <vector>
#include <assert.h>
#include "_input.h"
#include "component.h"

using namespace std ;


template <uint32_t W> class input : public _input {
    private:
        component *_component ;
        int _input_id  ;

    public:
        input(component* c) : _input(W){
            printf("in input()\n") ;
            _component = c ;
            _input_id = _component->add_input(this) ; ;
        } ;

        component *get_component(){
            // Make sure the input is registered with its component.
            if (_input_id == -1){
                //_input_id = _component->add_input(this) ;
            }
            return _component ;
        }
} ;


#endif