#include <stdio.h>
#include <signal.h>

typedef void (*fptr)(int);

class A
{
private:
    // Just dummy data members
    int uno;
    int dos;
public:
    A() : uno(1), dos(2)
    {
        printf("Constructor.\n");
    }
    ~A()
    {
        printf("Destructor.\n");
    }
};

int main()
{
    try
    {
        A a;  // Test if destructor is called.
        int *plocal = 0;
        *plocal = 1;  // Makes up an AV
    }
    catch(...)
    {
        printf("Excepcion.\n");
    }
    printf("Terminando.\n");
    return 0;
}
