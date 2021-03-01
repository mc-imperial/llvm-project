void foo() {
  int _Atomic  target;
  int _Atomic  y;
  int _Atomic  z;
  int _Atomic * A[3] = {&target, &y, &z};
  int _Atomic * p = 0;
  A[0] = p;
  int _Atomic ** q = &A[2];
  (void)q;
}
