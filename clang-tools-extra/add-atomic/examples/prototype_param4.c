void foo(int* p);

void bar() {
  int target;
  foo(&target);
}

void foo(int* a) {
  int** b = &a;
  (void)b;
}
