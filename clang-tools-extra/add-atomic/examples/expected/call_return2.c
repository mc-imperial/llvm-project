int _Atomic  target;

int _Atomic * foo(int _Atomic * x) {
  return target ? &target : x;
}

void bar() {
  int _Atomic  a = 2;
  int _Atomic * result;
  result = foo(&a);
}
