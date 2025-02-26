#include <iostream>
#include "defs.hpp"
int main()
{
    auto t = g(2);
    std::cout << t.execute() << std::endl;
    return 0;
}
