void foo() {
  int _Atomic  target;
  int _Atomic * p = &target;
  int _Atomic * q = 0;
  int b = p == q;
  (void)b;
}
