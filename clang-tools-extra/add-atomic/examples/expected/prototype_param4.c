void foo(int _Atomic * p);

void bar() {
  int _Atomic  target;
  foo(&target);
}

void foo(int _Atomic * a) {
  int _Atomic ** b = &a;
  (void)b;
}
