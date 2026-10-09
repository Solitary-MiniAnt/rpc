#include "../include/uuid.h"
#include <iostream>

int main() {
    for (int i = 0; i < 5; i++) {
        std::cout << uuid() << std::endl;
    }
    return 0;
}