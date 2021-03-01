int* foo(int* p);

void bar() {
  int target;
  int* result = foo(&target);
  (void)result;
}
