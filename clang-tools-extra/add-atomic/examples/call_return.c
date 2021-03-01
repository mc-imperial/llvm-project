int* foo(int* x) {
  return x;
}

void bar() {
  int target = 2;
  int* result;
  result = foo(&target);
}
