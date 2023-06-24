#include <stdio.h>

const char* amazingString() {
    return "An Amazing String";
}

int main()
{
    printf("%s has been returned\n", amazingString());
}