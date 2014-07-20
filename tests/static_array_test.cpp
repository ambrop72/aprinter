#include <stddef.h>
#include <stdio.h>

#include <aprinter/meta/StaticArray.h>

using namespace APrinter;

template <int Index>
struct ElemValue {
    static constexpr double value () { return Index + 0.5; }
};

using TheArray = StaticArray<double, 10, ElemValue>;

int main ()
{
    for (size_t i = 0; i < TheArray::Length; i++) {
        printf("%f\n", TheArray::readAt(i));
    }
}
