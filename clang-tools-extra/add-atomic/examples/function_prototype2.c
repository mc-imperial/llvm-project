int* foo();

void bar() {
  int target;
  int* x = foo();
  x = &target;
}
