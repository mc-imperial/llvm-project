void foo() {
  int _Atomic  target;
  int _Atomic  * a;
  a = ((void)1, &target);
  (void)a;
}
