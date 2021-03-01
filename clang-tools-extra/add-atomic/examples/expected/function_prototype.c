int* foo(int _Atomic * p);

void bar() {
  int _Atomic  target;
  int* result = foo(&target);
  (void)result;
}
