int _Atomic * foo(int _Atomic * x) {
  return x;
}

void bar() {
  int _Atomic  target = 2;
  int _Atomic * result;
  result = foo(&target);
}
