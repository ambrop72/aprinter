#include <stdio.h>
#include <inttypes.h>

#include <aprinter/ipstack/Struct.h>

using namespace APrinter;

APRINTER_TSTRUCT(HeaderFoo,
    (FieldA, int8_t)
    (FieldB, int64_t)
)

APRINTER_TSTRUCT(HeaderBar,
    (FieldC,   int8_t)
    (FieldD,   uint32_t)
    (FieldFoo, HeaderFoo)
)

void print(char const *data, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        printf("%02" PRIx8 " ", (uint8_t)data[i]);
    }
    printf("\n");
}

int main ()
{
    // Create a FooHeader::Val (a type which contains data), set field values.
    HeaderFoo::Val foo;
    foo.set(HeaderFoo::FieldA(), 30);
    foo.set(HeaderFoo::FieldB(), -55);
    
    print(foo.data, HeaderFooSize);
    
    // Change it via FooHeader::Ref (a type which references data).
    HeaderFoo::Ref foo_ref = foo;
    foo_ref.set(HeaderFoo::FieldA(), 61);
    
    print(foo.data, HeaderFooSize);
    
    // Get values via FooHeader::Ref.
    // Note: Val and Ref suppport get().
    HeaderFoo::Ref foo_ref1 = foo;
    printf("%" PRIi8 " %" PRIi64 "\n",
        foo_ref1.get(HeaderFoo::FieldA()),
        foo_ref1.get(HeaderFoo::FieldB()));
    
    // Allocate memory for a HeaderBar as char[] and initialize
    // parts of it through HeaderBar::Ref.
    char bar_mem[HeaderBarSize];
    HeaderBar::Ref bar_ref = HeaderBar::MakeRef(bar_mem);
    bar_ref.set(HeaderBar::FieldC(), -75);
    bar_ref.set(HeaderBar::FieldD(), 70000);
    
    // Initialize the nested HeaderFoo from foo.
    // This goes like this:
    // - Get a reference to the contained HeaderFoo via .ref(),
    //   obtaining a HeaderFoo::Ref.
    // - Call load() on the HeaderFoo::Ref to copy data from a
    //   HeaderFoo::Ref, which is created from HeaderFoo::Val
    //   automatically by a conversion operator.
    bar_ref.ref(HeaderBar::FieldFoo()).load(foo);
    
    print(bar_mem, HeaderBarSize);
    
    // Get the nested HeaderFoo from bar_ref as a value.
    // This will be a HeaderFoo::Val.
    // Change the original to prove it's a copy.
    auto foo_copy = bar_ref.get(HeaderBar::FieldFoo());
    bar_ref.ref(HeaderBar::FieldFoo()).set(HeaderFoo::FieldA(), 4);
    
    printf("%" PRIi8 " %" PRIi8 "\n",
        bar_ref.ref(HeaderBar::FieldFoo()).get(HeaderFoo::FieldA()),
        foo_copy.get(HeaderFoo::FieldA()));
    
    return 0;
}


