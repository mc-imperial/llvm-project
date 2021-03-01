int _Atomic * foo();

void bar() {
  int _Atomic  target;
  int _Atomic * x = foo();
  x = &target;
}
