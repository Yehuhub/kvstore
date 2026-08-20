#include <stdexcept>

class WrongTypeError: public std::runtime_error{
    public:
        WrongTypeError() : std::runtime_error("WRONGTYPE Operation against a key holding the wrong kind of value"){};
};