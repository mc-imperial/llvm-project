int target;

int* foo(int* x) {
  return target ? &target : x;
}

void bar() {
  int a = 2;
  int* result;
  result = foo(&a);
}
